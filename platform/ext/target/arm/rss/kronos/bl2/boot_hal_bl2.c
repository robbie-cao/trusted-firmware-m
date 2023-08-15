/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "boot_hal.h"

#include <string.h>

#include "atu_rss_drv.h"
#include "boot_measurement.h"
#include "bootutil/bootutil.h"
#include "bootutil/bootutil_log.h"
#include "bootutil/fault_injection_hardening.h"
#include "cmsis.h"
#include "device_definition.h"
#include "flash_map/flash_map.h"
#include "host_base_address.h"
#include "mhu.h"
#include "mhu_v3_x.h"
#include "mscp_mailbox.h"
#include "platform_base_address.h"
#include "platform_regs.h"
#include "psa/crypto.h"
#include "size_defs.h"
#include "tfm_boot_status.h"
#include "tfm_plat_defs.h"
#include "fainlight_gic_lib.h"
#ifdef CRYPTO_HW_ACCELERATOR
#include "crypto_hw.h"
#include "fih.h"
#endif /* CRYPTO_HW_ACCELERATOR */

extern volatile bool scp_doorbell;
extern ARM_DRIVER_FLASH FLASH_DEV_NAME;

static uint8_t *lcp_measurement;
static struct boot_measurement_metadata *lcp_measurement_metadata;

static volatile uint8_t chip_id = 0;

/* Read Chip ID and store it in a global variable */
static void read_chip_id()
{
    uint32_t *chip_id_reg = (uint32_t *)RSS_INTEG_REG_BASE;

    chip_id = (((*chip_id_reg) >> 0x8) & 0xFF);

    BOOT_LOG_INF("[BL2] Chip ID: %u", chip_id);
}

static int boot_add_data_to_shared_area(uint8_t        major_type,
                                        uint16_t       minor_type,
                                        size_t         size,
                                        const uint8_t *data)
{
    struct shared_data_tlv_entry tlv_entry = {0};
    struct tfm_boot_data *boot_data;
    uintptr_t tlv_end, offset;

    if (data == NULL) {
        return -1;
    }

    boot_data = (struct tfm_boot_data *)BOOT_TFM_SHARED_DATA_BASE;

    /* Check whether the shared area needs to be initialized. */
    if ((boot_data->header.tlv_magic != SHARED_DATA_TLV_INFO_MAGIC) ||
        (boot_data->header.tlv_tot_len > BOOT_TFM_SHARED_DATA_SIZE)) {

        memset((void *)BOOT_TFM_SHARED_DATA_BASE, 0, BOOT_TFM_SHARED_DATA_SIZE);
        boot_data->header.tlv_magic   = SHARED_DATA_TLV_INFO_MAGIC;
        boot_data->header.tlv_tot_len = SHARED_DATA_HEADER_SIZE;
    }

    /* Get the boundaries of TLV section. */
    tlv_end = BOOT_TFM_SHARED_DATA_BASE + boot_data->header.tlv_tot_len;
    offset  = BOOT_TFM_SHARED_DATA_BASE + SHARED_DATA_HEADER_SIZE;

    /* Check whether TLV entry is already added. Iterates over the TLV section
     * looks for the same entry if found then returns with error.
     */
    while (offset < tlv_end) {
        /* Create local copy to avoid unaligned access */
        memcpy(&tlv_entry, (const void *)offset, SHARED_DATA_ENTRY_HEADER_SIZE);
        if (GET_MAJOR(tlv_entry.tlv_type) == major_type &&
            GET_MINOR(tlv_entry.tlv_type) == minor_type) {

            return -1;
        }

        offset += SHARED_DATA_ENTRY_SIZE(tlv_entry.tlv_len);
    }

    /* Add TLV entry. */
    tlv_entry.tlv_type = SET_TLV_TYPE(major_type, minor_type);
    tlv_entry.tlv_len  = size;

    /* Check integer overflow and overflow of shared data area. */
    if (SHARED_DATA_ENTRY_SIZE(size) >
        (UINT16_MAX - boot_data->header.tlv_tot_len)) {
        return -1;
    } else if ((SHARED_DATA_ENTRY_SIZE(size) + boot_data->header.tlv_tot_len) >
               BOOT_TFM_SHARED_DATA_SIZE) {
        return -1;
    }

    offset = tlv_end;
    memcpy((void *)offset, &tlv_entry, SHARED_DATA_ENTRY_HEADER_SIZE);

    offset += SHARED_DATA_ENTRY_HEADER_SIZE;
    memcpy((void *)offset, data, size);

    boot_data->header.tlv_tot_len += SHARED_DATA_ENTRY_SIZE(size);

    return 0;
}

static int store_measurement(uint8_t index,
                             const uint8_t *measurement,
                             size_t measurement_size,
                             const struct boot_measurement_metadata *metadata,
                             bool lock_measurement)
{
    uint16_t minor_type;
    uint8_t claim;
    int rc;

    minor_type = SET_MBS_MINOR(index, SW_MEASURE_METADATA);
    rc = boot_add_data_to_shared_area(TLV_MAJOR_MBS,
                                      minor_type,
                                      sizeof(struct boot_measurement_metadata),
                                      (const uint8_t *)metadata);
    if (rc) {
        return rc;
    }

    claim = lock_measurement ? SW_MEASURE_VALUE_NON_EXTENDABLE
                             : SW_MEASURE_VALUE;
    minor_type = SET_MBS_MINOR(index, claim);
    rc = boot_add_data_to_shared_area(TLV_MAJOR_MBS,
                                      minor_type,
                                      measurement_size,
                                      measurement);

    return rc;
}

int boot_store_measurement(uint8_t index,
                           const uint8_t *measurement,
                           size_t measurement_size,
                           const struct boot_measurement_metadata *metadata,
                           bool lock_measurement)
{
    uint8_t *test_zero_measurement;

    if (index >= BOOT_MEASUREMENT_SLOT_MAX) {
        return -1;
    }

    /*
     * LCP image requires multiple loads. If slot index is for LCP, then don't
     * add data to the shared area.
     */
    if(index == BOOT_MEASUREMENT_SLOT_RT_0 + RSS_FIRMWARE_LCP_ID) {
        if (measurement_size > PSA_HASH_LENGTH(MEASURED_BOOT_HASH_ALG)) {
            return -1;
        }

        test_zero_measurement = calloc(measurement_size, sizeof(uint8_t));

        /*
         * If LCP measurements are not stored, store the input measurements.
         * Else, check if the store measurements matches with the input
         * measurements. If those doesn't match, then return -1.
         */
        if (memcmp(lcp_measurement, test_zero_measurement,
                   measurement_size * sizeof(uint8_t)) == 0) {
            memcpy(lcp_measurement, measurement,
                   measurement_size * sizeof(uint8_t));
            memcpy(lcp_measurement_metadata, metadata,
                   sizeof(struct boot_measurement_metadata));
        } else {
            if ((memcmp(measurement, lcp_measurement,
                        measurement_size * sizeof(uint8_t)) != 0) ||
                (memcmp(metadata, lcp_measurement_metadata,
                        sizeof(struct boot_measurement_metadata)) != 0)) {
                return -1;
            }
        }
        return 0;
    }

    return store_measurement(index,
                             measurement,
                             measurement_size,
                             metadata,
                             lock_measurement);
}

static int32_t gic_multiple_view_init(void)
{
    enum atu_error_t atu_err;
    uint32_t err;

    atu_err = atu_initialize_region(&ATU_DEV_S, RSS_ATU_FAINLIGHT_GIC_ID,
                                    SI_GIC_VIEW_0_BASE_S_LOG,
                                    SI_GIC_VIEW_0_BASE_S_PHY,
                                    SI_GIC_VIEW_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    err = gic_multiple_view_probe(SI_GIC_VIEW_0_BASE_S_LOG);
    if (err != 0) {
        BOOT_LOG_INF("BL2: Error probing GIC Multiple Views device");
        goto free_atu;
    }

    err = gic_multiple_view_programming();
    if (err != 0) {
        BOOT_LOG_INF("BL2: Error programming GIC Multiple Views");
        goto free_atu;
    }

free_atu:
    atu_err = atu_uninitialize_region(&ATU_DEV_S, RSS_ATU_FAINLIGHT_GIC_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    return 0;
}

int32_t boot_platform_init(void)
{
    int32_t result;

    read_chip_id();

    result = gic_multiple_view_init();
    if (result != ARM_DRIVER_OK) {
        return 1;
    }

    result = FLASH_DEV_NAME.Initialize(NULL);
    if (result != ARM_DRIVER_OK) {
        return 1;
    }

    return 0;
}

int32_t boot_platform_post_init(void)
{
#ifdef CRYPTO_HW_ACCELERATOR
    int32_t result;

    result = crypto_hw_accelerator_init();
    if (result) {
        return 1;
    }

    (void)fih_delay_init();
#endif /* CRYPTO_HW_ACCELERATOR */

    return 0;
}

/*
 * ================================ Secure ====================================
 */

static enum atu_error_t rss_si_atu_config(uint8_t si_cluster)
{
    enum atu_error_t atu_err;
    uint8_t region;
    uint32_t log_addr;
    uint64_t phys_addr;
    uint32_t size;

    switch (si_cluster) {
    case MHU_V3_RSS_SI_CL0:
        region = HOST_SI_CL0_RSS_MAILBOX_ATU_ID;
        log_addr = HOST_SI_CL0_RSS_MAILBOX_BASE_S;
        phys_addr = HOST_SI_CL0_RSS_MAILBOX_PHYS_BASE;
        size = HOST_SI_CL0_RSS_MAILBOX_ATU_SIZE;
        break;
    case MHU_V3_RSS_SI_CL1:
        region = HOST_SI_CL1_RSS_MAILBOX_ATU_ID;
        log_addr = HOST_SI_CL1_RSS_MAILBOX_BASE_S;
        phys_addr = HOST_SI_CL1_RSS_MAILBOX_PHYS_BASE;
        size = HOST_SI_CL1_RSS_MAILBOX_ATU_SIZE;
        break;
    case MHU_V3_RSS_SI_CL2:
        region = HOST_SI_CL2_RSS_MAILBOX_ATU_ID;
        log_addr = HOST_SI_CL2_RSS_MAILBOX_BASE_S;
        phys_addr = HOST_SI_CL2_RSS_MAILBOX_PHYS_BASE;
        size = HOST_SI_CL2_RSS_MAILBOX_ATU_SIZE;
        break;
    default:
        return ATU_ERR_INVALID_ARG;
    }

    /* Configure RSS ATU to access the Safety Island Cluster SRAM region
     * for MHU Outband Msg Buffer
     */
    atu_err = atu_initialize_region(&ATU_DEV_S, region, log_addr,
                                    phys_addr, size);

    if (atu_err != ATU_ERR_NONE) {
        BOOT_LOG_ERR("BL2: Failed to map SI CL%d<->RSS MHU Outband "
                "msg region in ATU: error:%d", si_cluster, atu_err);
        goto end;
    }

    /* Configure output bus attribute for the ATU region */
    atu_err = set_axnsc(&ATU_DEV_S, ATU_ROBA_SET_0, region);
    if (atu_err != ATU_ERR_NONE) {
        BOOT_LOG_ERR("BL2: Failed to set SI CL%d<->RSS MHU Outband msg region "
                "as secure in ATU mapping: error:%d", si_cluster, atu_err);
        goto end;
    }

    BOOT_LOG_INF("BL2: Mapped SI CL%d<->RSS MHU Outband msg region in ATU",
                 si_cluster);

end:
    return atu_err;
}

static int boot_platform_pre_load_secure()
{
    enum atu_error_t atu_err;

    /* Configure RSS ATU to access AP SRAM region for MHU Outband Msg Buffer */
    atu_err = atu_initialize_region(&ATU_DEV_S, HOST_AP_RSS_MAILBOX_ATU_ID,
            HOST_AP_RSS_MAILBOX_BASE_S, HOST_AP_RSS_MAILBOX_PHYS_BASE,
            HOST_AP_RSS_MAILBOX_ATU_SIZE);

    if (atu_err != ATU_ERR_NONE) {
        BOOT_LOG_ERR("BL2: Failed to map AP<->RSS MHU Outband msg region in ATU"
                ": error:%d", atu_err);
        return -1;
    }

    /* Configure output bus attribute for the ATU region */
    atu_err = set_axnsc(&ATU_DEV_S, ATU_ROBA_SET_0, HOST_AP_RSS_MAILBOX_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        BOOT_LOG_ERR("BL2: Failed to set AP<->RSS MHU Outband msg region as "
                "secure in ATU mapping: error:%d", atu_err);
        goto fail_ap;
    }

    BOOT_LOG_INF("BL2: Mapped AP<->RSS MHU Outband msg region in ATU");

    /* Configure RSS ATU region for Safety Island Cluster 0 */
    atu_err = rss_si_atu_config(MHU_V3_RSS_SI_CL0);
    if (atu_err != ATU_ERR_NONE) {
        goto fail_si_cl0;
    }

    /* Configure RSS ATU region for Safety Island Cluster 1 */
    atu_err = rss_si_atu_config(MHU_V3_RSS_SI_CL1);
    if (atu_err != ATU_ERR_NONE) {
        goto fail_si_cl1;
    }

    /* Configure RSS ATU region for Safety Island Cluster 2 */
    atu_err = rss_si_atu_config(MHU_V3_RSS_SI_CL2);
    if (atu_err != ATU_ERR_NONE) {
        goto fail_si_cl2;
    }

    return 0;

fail_ap:
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_AP_RSS_MAILBOX_ATU_ID);
    return -1;
fail_si_cl0:
    atu_err = atu_uninitialize_region(&ATU_DEV_S,
                                      HOST_SI_CL0_RSS_MAILBOX_ATU_ID);
    return -1;
fail_si_cl1:
    atu_err = atu_uninitialize_region(&ATU_DEV_S,
                                      HOST_SI_CL1_RSS_MAILBOX_ATU_ID);
    return -1;
fail_si_cl2:
    atu_err = atu_uninitialize_region(&ATU_DEV_S,
                                      HOST_SI_CL2_RSS_MAILBOX_ATU_ID);
    return -1;
}

static int boot_platform_post_load_secure()
{
#ifdef RSS_USE_SI_FLASH
	enum atu_error_t err;

    /* All the images had been loaded from SI NVM flash */
    /* Close SI NVM flash ATU region */
    err = atu_uninitialize_region(&ATU_DEV_S, 6);
    if (err != ATU_ERR_NONE) {
        return 1;
    }
#endif

    return 0;
}

/*
 * ============================ Non-Secure ====================================
 */

static int boot_platform_pre_load_non_secure()
{
    return 0;
}

static int boot_platform_post_load_non_secure()
{
    return 0;
}

/*
 * =================================== SCP ====================================
 */
#ifdef ATU_SCP
enum atu_error_t scp_atu_init(void)
{
    enum atu_error_t atu_error;
    enum mscp_atu_req_error_t scp_atu_error;
    uint8_t idx;

    scp_atu_error = init_atu_request_handler(&SCP_ATU_INFO);
    if(scp_atu_error != MSCP_ATU_REQ_ERR_NONE) {
        BOOT_LOG_INF("BL2: ERROR! Failed to initialize SCP ATU context\n");
    };

    /* Initialize the translation regions in SCP ATU */
    for (idx = CMN_CONFIG_SCP_ATU_REGION; idx < SCP_ATU_REGION_COUNT; idx++) {
        atu_error = atu_initialize_region(&ATU_DEV_SCP, idx,
                scp_atu_regions[idx].log_addr,
                (HOST_REMOTE_CHIP_PERIPH_OFFSET(chip_id) +
                 scp_atu_regions[idx].phy_addr),
                scp_atu_regions[idx].size);

        if (atu_error != ATU_ERR_NONE) {
            BOOT_LOG_INF("BL2: ERROR! SCP ATU region %u init status: %d\n",
                    idx, atu_error);
            return atu_error;
        }

        /* Store the region info in the context */
        SCP_ATU_INFO.active_regions_list[idx].region_owner_id =
            ATU_REGION_OWNER_ID_RSS;

        SCP_ATU_INFO.active_regions_list[idx].log_addr_start =
            scp_atu_regions[idx].log_addr;

        SCP_ATU_INFO.active_regions_list[idx].log_addr_end =
            ((scp_atu_regions[idx].log_addr +
              scp_atu_regions[idx].size) - 1);

        SCP_ATU_INFO.active_regions_list[idx].phy_addr_start =
            scp_atu_regions[idx].phy_addr;

        SCP_ATU_INFO.active_regions_list[idx].phy_addr_end =
            ((scp_atu_regions[idx].phy_addr +
              scp_atu_regions[idx].size) - 1);

        SCP_ATU_INFO.active_regions_list[idx].is_enabled = true;

        SCP_ATU_INFO.active_regions_count++;
        BOOT_LOG_INF("BL2: SCP ATU region: %u", idx);
        BOOT_LOG_INF("[%p - %p]-->[%lx%lx - %lx%lx]\n",
                (void *)SCP_ATU_INFO.active_regions_list[idx].log_addr_start,
                (void *)SCP_ATU_INFO.active_regions_list[idx].log_addr_end,
                (SCP_ATU_INFO.active_regions_list[idx].phy_addr_start & (0xFFFFFFFF << 31)),
                SCP_ATU_INFO.active_regions_list[idx].phy_addr_start,
                (SCP_ATU_INFO.active_regions_list[idx].phy_addr_end & (0xFFFFFFFF << 31)),
                SCP_ATU_INFO.active_regions_list[idx].phy_addr_end);

        if (scp_atu_regions[idx].access_type != ATU_ACCESS_ROOT) {
            continue;
        }

        /*
         * Configure the output bus attributes for ATU regions used to access
         * CMN config space and Cluster utility region in the AP memory map.
         * These regions are accessible by root only.
         *
         * Access Rights:
         * | NSE | PROT[1] | attr
         * | 0   | 0       | secure
         * | 0   | 1       | non-secure
         * | 1   | 0       | root
         * | 1   | 1       | realm
         *
         * | PROT[0] | attr
         * | 0       | unprivileged
         * | 1       | privileged
         */
        atu_error = set_axnsc(&ATU_DEV_SCP, ATU_ROBA_SET_1, idx);
        if (atu_error != ATU_ERR_NONE) {
            BOOT_LOG_INF("BL2: Unable to modify AxNSE for ATU region %u\n",
                    idx);
            return atu_error;
        }

        atu_error = set_axprot1(&ATU_DEV_SCP, ATU_ROBA_SET_0, idx);
        if (atu_error != ATU_ERR_NONE) {
            BOOT_LOG_INF("BL2: Unable to modify AxPROT1 for ATU region %u\n",
                    idx);
            return atu_error;
        }
    }

    return 0;
}
#endif

static int initialize_rss_scp_mhu(void)
{
    enum mhu_error_t err;

    /* Initialize the RSS to SCP Sender MHU */
    err = mhu_init_sender(&MHU_V3_RSS_TO_SCP_DEV);
    if (err != MHU_ERR_NONE) {
        BOOT_LOG_ERR("BL2: RSS to SCP MHU driver init failed:%d", err);
        return -1;
    }

    /* Initialize the SCP to RSS Receiver MHU */
    err = mhu_init_receiver(&MHU_V3_SCP_TO_RSS_DEV);
    if (err != MHU_ERR_NONE) {
        BOOT_LOG_ERR("BL2: SCP to RSS MHU driver init failed:%d", err);
        return -1;
    }

    BOOT_LOG_INF("BL2: RSS<->SCP MHU initialized");
    return 0;
}

/* Fuction called before scp firmware is loaded. */
static int boot_platform_pre_load_scp(void)
{
    enum mscp_mailbox_error_t status;
    enum atu_error_t atu_err;
    int32_t mhu_err;

    BOOT_LOG_INF("BL2: SCP pre load start");

    /* Configure RSS ATU to access RSS header region for SCP */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SCP_IMG_HDR_ATU_ID,
                                    HOST_SCP_HDR_ATU_BASE_S,
                                    RSS_HDR_PHYS_BASE,
                                    RSS_IMG_HDR_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Configure RSS ATU to access SCP SRAM code region */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SCP_IMG_CODE_ATU_ID,
                                    HOST_SCP_CODE_BASE_S,
                                    HOST_SCP_PHYS_BASE,
                                    HOST_SCP_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Configure RSS ATU to access SCP INIT_CTRL region */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SCP_INIT_ATU_ID,
                                    HOST_SCP_INIT_BASE_S,
                                    HOST_SCP_INIT_PHYS_BASE,
                                    HOST_SCP_INIT_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    /* Initialize RSS<-->SCP MHU */
    mhu_err = initialize_rss_scp_mhu();
    if (mhu_err) {
        return mhu_err;
    }

#ifdef ATU_SCP
    /* Configure RSS ATU to access SCP ATU */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SCP_ATU_ATU_ID,
                                    HOST_SCP_ATU_BASE_S,
                                    HOST_SCP_ATU_PHYS_BASE,
                                    HOST_SCP_ATU_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    /* Configure RSS ATU to access RSM region */
    atu_err = atu_initialize_region(&ATU_DEV_S,
            HOST_SCP_RSS_MAILBOX_ATU_ID,
            HOST_SCP_RSS_MAILBOX_BASE_S,
            (HOST_REMOTE_CHIP_PERIPH_OFFSET(chip_id) +
             HOST_SCP_RSS_MAILBOX_PHYS_BASE),
            HOST_SCP_RSS_MAILBOX_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    /* Initialize the SCP-RSS shared mailbox status */
    status = mailbox_init(&scp2rss_mbx_info);
    if (status != MSCP_MAILBOX_ERR_NONE) {
        BOOT_LOG_INF("BL2: Failed to initialize SCP-RSS mailbox\n");
        return 1;
    }

    /*
     * If SCP ATU control is enabled, then skip configuring the SCP ATU regions
     * except the RSM shared SRAM region in order to indicate that the SCP ATU
     * driver can configure the SCP ATU.
     */
#   ifndef ENABLE_SCP_ATU_CTRL
    /* Configure the SCP ATU */
    atu_err = scp_atu_init();
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
#endif
#ifdef ENABLE_SCP_ATU_CTRL
    /* Enable SCP ATU control */
    *(volatile uint32_t *)RSS_ATU_AP |= 0x1;
#endif
    /* Enable SCP<-->RSS MHUv3 Interrupt */
    NVIC_ClearPendingIRQ(CMU_MHU4_Receiver_IRQn);
    NVIC_EnableIRQ(CMU_MHU4_Receiver_IRQn);
#endif

    BOOT_LOG_INF("BL2: SCP pre load complete");

    return 0;
}

/* Fuction called after scp firmware is loaded. */
static int boot_platform_post_load_scp(void)
{
    enum atu_error_t atu_err;
    struct mscp_sysctrl *scp_init = (void *) HOST_SCP_INIT_BASE_S;

    BOOT_LOG_INF("BL2: SCP post load start");

    /* Clear the header from the header region but avoid removing anything that
     * the region may be incidentally overlapping.
     */
    memset(HOST_SCP_IMG_BASE_S, 0, BL2_HEADER_SIZE);

    scp_init->cpuwait = 0x1;
    scp_init->cpuwait = 0x0;
    BOOT_LOG_INF("SCP is released out of reset");

    /* Close RSS ATU region configured to access RSS header region for SCP */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_SCP_IMG_HDR_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Close RSS ATU region configured to access SCP SRAM code region */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_SCP_IMG_CODE_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Close RSS ATU region configured to access SCP INIT_CTRL region */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_SCP_INIT_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: SCP post load complete");

    return 0;
}

/*
 * =================================== LCP ====================================
 */

/* Fuction called before lcp firmware is loaded. */
static int boot_platform_pre_load_lcp(void)
{
    enum atu_error_t atu_err;
    int32_t err;

    BOOT_LOG_INF("BL2: LCP pre load start");

    BOOT_LOG_INF("BL2: Wait for doorbell from SCP before starting LCP...");

    /*
     * Wait till the SCP turns on the SYSTOP power domain. This is required
     * as the next image that will be loaded by MCUBOOT is LCP and in order
     * to load the LCM firmware, the SYSTOP domain should be in ON state which
     * can be done by SCP only.
     */
    while (1) {
        if (scp_doorbell) {
            BOOT_LOG_INF("BL2: Doorbell received from SCP!");
            break;
        }
    }

    /* Intialize LCP boot measurements */
    lcp_measurement = calloc(PSA_HASH_LENGTH(MEASURED_BOOT_HASH_ALG),
                            sizeof(uint8_t));
    lcp_measurement_metadata =
            calloc(1, sizeof(struct boot_measurement_metadata));
    /* Configure RSS ATU to access RSS header region for LCP0 */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_LCP_IMG_HDR_ATU_ID,
                                    HOST_LCP_HDR_ATU_BASE_S,
                                    RSS_HDR_PHYS_BASE,
                                    RSS_IMG_HDR_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /*
     * Configure RSS ATU region to access the Cluster utility space via SCP
     * address translation window. This will require both RSS and SCP ATU to be
     * programmed correctly.
     */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_LCP_IMG_CODE_ATU_ID,
                                    HOST_LCP_CODE_BASE_S,
                                    (HOST_REMOTE_CHIP_PERIPH_OFFSET(chip_id) +
                                     HOST_LCP_0_PHYS_BASE),
                                    HOST_LCP_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: LCP pre load complete");

    return 0;
}

/* Fuction called after lcp firmware is loaded. */
static int boot_platform_post_load_lcp(void)
{
    enum atu_error_t atu_err;
    enum mhu_v3_x_error_t mhu_error;
    struct boot_rsp rsp;
    int lcp_idx, rc;
    fih_ret fih_rc = FIH_FAILURE;

    BOOT_LOG_INF("BL2: LCP post load start");

    /* Clear the header from the header region but avoid removing anything that
     * the region may be incidentally overlapping.
     */
    memset(HOST_LCP_IMG_BASE_S, 0, BL2_HEADER_SIZE);

    /* Close RSS ATU region configured to access LCP SRAM code region */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_LCP_IMG_CODE_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    /* Copy the ITCM contents from LCP0 to other LCPs */
    for (int lcp_idx = 1; lcp_idx < RSS_LCP_COUNT; lcp_idx++) {
        /*
         * Configure RSS ATU region to access the SCP Cluster utility space
         * and map to the i-th LCP's ITCM
         */
        atu_err = atu_initialize_region(&ATU_DEV_S,
                                        HOST_LCP_IMG_CODE_ATU_ID,
                                        HOST_LCP_CODE_BASE_S,
                                        (HOST_REMOTE_CHIP_PERIPH_OFFSET(chip_id) +
                                         HOST_LCP_N_PHYS_BASE(lcp_idx)),
                                        HOST_LCP_ATU_SIZE);
        if (atu_err != ATU_ERR_NONE) {
            return 1;
        }

        /* Cleaning 'rsp' to avoid accidentally loading
         * the NS image in case of a fault injection attack. */
        memset(&rsp, 0, sizeof(struct boot_rsp));

        FIH_CALL(boot_go_for_image_id, fih_rc, &rsp, RSS_FIRMWARE_LCP_ID);
        if (FIH_NOT_EQ(fih_rc, FIH_SUCCESS)) {
            BOOT_LOG_ERR("Unable to find bootable LCP image");
            FIH_PANIC;
        }

        /* Clear the header from the header region but avoid removing anything that
         * the region may be incidentally overlapping.
         */
        memset(HOST_LCP_IMG_BASE_S, 0, BL2_HEADER_SIZE);

        /* Close RSS ATU region configured to access LCP SRAM code region */
        atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_LCP_IMG_CODE_ATU_ID);
        if (atu_err != ATU_ERR_NONE) {
            return 1;
        }
    }

    /*
     * Save the boot measurement after LCP images are loaded.
     */
    rc = store_measurement(
        (uint8_t)BOOT_MEASUREMENT_SLOT_RT_0 + RSS_FIRMWARE_LCP_ID,
        lcp_measurement,
        PSA_HASH_LENGTH(MEASURED_BOOT_HASH_ALG) * sizeof(uint8_t),
        lcp_measurement_metadata, false);
    if (rc) {
        return -1;
    }

    /* Close RSS ATU region configured to access RSS header region for LCP */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_LCP_IMG_HDR_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: LCP post load complete");

    return 0;
}

/*
 * ================================== AP BL2 ==================================
 */

/* Fuction called before ap bl2 firmware is loaded. */
static int boot_platform_pre_load_ap_bl2(void)
{
    enum atu_error_t atu_err;

    BOOT_LOG_INF("BL2: AP BL2 pre load start");

    /* Configure RSS ATU to access RSS header region for AP BL2 */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_AP_BL2_IMG_HDR_ATU_ID,
                                    HOST_AP_BL2_HDR_ATU_BASE_S,
                                    RSS_HDR_PHYS_BASE,
                                    RSS_IMG_HDR_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Configure RSS ATU to access AP BL2 SRAM code region */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_AP_BL2_IMG_CODE_ATU_ID,
                                    HOST_AP_BL2_CODE_BASE_S,
                                    HOST_AP_BL2_PHYS_BASE,
                                    HOST_AP_BL2_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: AP BL2 pre load complete");

    return 0;
}

/* Fuction called after ap bl2 firmware is loaded. */
static int boot_platform_post_load_ap_bl2(void)
{
    enum atu_error_t atu_err;
    enum mhu_v3_x_error_t mhu_error;

    BOOT_LOG_INF("BL2: AP BL2 post load start");

    /* Clear the header from the header region but avoid removing anything that
     * the region may be incidentally overlapping.
     */
    memset(HOST_AP_BL2_IMG_BASE_S, 0, BL2_HEADER_SIZE);

    /*
     * Send doorbell to SCP to indicate that the RSS initialization is
     * complete and that the SCP can release the LCPs and turn on the
     * primary AP core.
     */
    mhu_error = mhu_v3_x_doorbell_write(&MHU_V3_RSS_TO_SCP_DEV, 1, 0x1);

    if (mhu_error != MHU_V_3_X_ERR_NONE) {
        return mhu_error;
    }
    BOOT_LOG_INF("BL2: RSS-->SCP doorbell set!");

    /* Close RSS ATU region configured to access RSS header region for AP BL2 */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_AP_BL2_IMG_HDR_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Close RSS ATU region configured to access AP BL2 SRAM code region */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_AP_BL2_IMG_CODE_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: AP BL2 post load complete");

    return 0;
}

/*
 * ================================== Safety Island Cluster0 ==================================
 */

/* Fuction called before si cl0 firmware is loaded. */
static int boot_platform_pre_load_si_cl0(void)
{
    enum atu_error_t atu_err;

    BOOT_LOG_INF("BL2: SI CL0 pre load start");

    /* Configure RSS ATU to access RSS header region for SI CL0 */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SI_CL0_IMG_HDR_ATU_ID,
                                    HOST_SI_CL0_HDR_ATU_BASE_S,
                                    RSS_HDR_PHYS_BASE,
                                    RSS_IMG_HDR_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Configure RSS ATU to access AP BL2 SRAM code region */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SI_CL0_IMG_CODE_ATU_ID,
                                    HOST_SI_CL0_CODE_BASE_S,
                                    HOST_SI_CL0_SRAM_PHYS_BASE,
                                    HOST_SI_CL0_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: SI CL0 pre load complete");

    return 0;
}

/* Fuction called after si cl0 firmware is loaded. */
static int boot_platform_post_load_si_cl0(void)
{
    enum atu_error_t atu_err;
    enum mhu_v3_x_error_t mhu_error;

    BOOT_LOG_INF("BL2: SI CL0 post load start");
    /* Clear the header from the header region but avoid removing anything that
     * the region may be incidentally overlapping.
     */
    memset(HOST_SI_CL0_IMG_BASE_S, 0, BL2_HEADER_SIZE);
    /*
     * Send doorbell to SCP to indicate that the RSS initialization is
     * complete and that the SCP can release safety island cluster0
     */
    BOOT_LOG_INF("Telling SCP to reset SI CL0");
    mhu_error = mhu_v3_x_doorbell_write(&MHU_V3_RSS_TO_SCP_DEV, 2, 0x1);

    if (mhu_error != MHU_V_3_X_ERR_NONE) {
        return mhu_error;
    }
    BOOT_LOG_INF("BL2: RSS-->SCP doorbell set!");

    /* Close RSS ATU region configured to access RSS header region for SI CL0 */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_SI_CL0_IMG_HDR_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Close RSS ATU region configured to access SI CL0 SRAM code region */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_SI_CL0_IMG_CODE_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: SI CL0 post load complete");

    return 0;
}

/*
 * ================================== Safety Island Cluster1 ==================================
 */

/* Fuction called before si cl1 firmware is loaded. */
static int boot_platform_pre_load_si_cl1(void)
{
    enum atu_error_t atu_err;

    BOOT_LOG_INF("BL2: SI CL1 pre load start");

    /* Configure RSS ATU to access RSS header region for SI CL1 */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SI_CL1_IMG_HDR_ATU_ID,
                                    HOST_SI_CL1_HDR_ATU_BASE_S,
                                    RSS_HDR_PHYS_BASE,
                                    RSS_IMG_HDR_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Configure RSS ATU to access AP BL2 SRAM code region */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SI_CL1_IMG_CODE_ATU_ID,
                                    HOST_SI_CL1_CODE_BASE_S,
                                    HOST_SI_CL1_SRAM_PHYS_BASE,
                                    HOST_SI_CL1_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: SI CL1 pre load complete");

    return 0;
}

/* Fuction called after si cl1 firmware is loaded. */
static int boot_platform_post_load_si_cl1(void)
{
    enum atu_error_t atu_err;
    enum mhu_v3_x_error_t mhu_error;

    BOOT_LOG_INF("BL2: SI CL1 post load start");

    /* Clear the header from the header region but avoid removing anything that
     * the region may be incidentally overlapping.
     */
    memset(HOST_SI_CL1_IMG_BASE_S, 0, BL2_HEADER_SIZE);
    /*
     * Send doorbell to SCP to indicate that the RSS initialization is
     * complete and that the SCP can release safety island cluster1
     */
    BOOT_LOG_INF("Telling SCP to reset SI CL1");
    mhu_error = mhu_v3_x_doorbell_write(&MHU_V3_RSS_TO_SCP_DEV, 3, 0x1);

    if (mhu_error != MHU_V_3_X_ERR_NONE) {
        return mhu_error;
    }
    BOOT_LOG_INF("BL2: RSS-->SCP doorbell set!");

    /* Close RSS ATU region configured to access RSS header region for SI CL1 */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_SI_CL1_IMG_HDR_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Close RSS ATU region configured to access SI CL1 SRAM code region */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_SI_CL1_IMG_CODE_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: SI CL1 post load complete");

    return 0;
}

/*
 * ================================== Safety Island Cluster2 ==================================
 */

/* Fuction called before si cl2 firmware is loaded. */
static int boot_platform_pre_load_si_cl2(void)
{
    enum atu_error_t atu_err;

    BOOT_LOG_INF("BL2: SI CL2 pre load start");

    /* Configure RSS ATU to access RSS header region for SI CL2 */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SI_CL2_IMG_HDR_ATU_ID,
                                    HOST_SI_CL2_HDR_ATU_BASE_S,
                                    RSS_HDR_PHYS_BASE,
                                    RSS_IMG_HDR_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Configure RSS ATU to access SI CL2 SRAM code region */
    atu_err = atu_initialize_region(&ATU_DEV_S,
                                    HOST_SI_CL2_IMG_CODE_ATU_ID,
                                    HOST_SI_CL2_CODE_BASE_S,
                                    HOST_SI_CL2_SRAM_PHYS_BASE,
                                    HOST_SI_CL2_ATU_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: SI CL2 pre load complete");

    return 0;
}

/* Fuction called after si cl2 firmware is loaded. */
static int boot_platform_post_load_si_cl2(void)
{
    enum atu_error_t atu_err;
    enum mhu_v3_x_error_t mhu_error;

    BOOT_LOG_INF("BL2: SI CL2 post load start");

    /* Clear the header from the header region but avoid removing anything that
     * the region may be incidentally overlapping.
     */
    memset(HOST_SI_CL2_IMG_BASE_S, 0, BL2_HEADER_SIZE);
    /*
     * Send doorbell to SCP to indicate that the RSS initialization is
     * complete and that the SCP can release safety island cluster2
     */
	BOOT_LOG_INF("Telling SCP to reset SI CL2");
    mhu_error = mhu_v3_x_doorbell_write(&MHU_V3_RSS_TO_SCP_DEV, 4, 0x1);

    if (mhu_error != MHU_V_3_X_ERR_NONE) {
        return mhu_error;
    }
    BOOT_LOG_INF("BL2: RSS-->SCP doorbell set!");

    /* Close RSS ATU region configured to access RSS header region for SI CL2 */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_SI_CL2_IMG_HDR_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }
    /* Close RSS ATU region configured to access SI CL2 SRAM code region */
    atu_err = atu_uninitialize_region(&ATU_DEV_S, HOST_SI_CL2_IMG_CODE_ATU_ID);
    if (atu_err != ATU_ERR_NONE) {
        return 1;
    }

    BOOT_LOG_INF("BL2: SI CL2 post load complete");

    return 0;
}
/*
 * ================================= VECTORS ==================================
 */

/* Array of function pointers to call before each image is loaded indexed by image id */
static int (*boot_platform_pre_load_vector[RSS_FIRMWARE_COUNT]) (void) = {
    [RSS_FIRMWARE_SECURE_ID]        = boot_platform_pre_load_secure,
    [RSS_FIRMWARE_NON_SECURE_ID]    = boot_platform_pre_load_non_secure,
    [RSS_FIRMWARE_SCP_ID]           = boot_platform_pre_load_scp,
    [RSS_FIRMWARE_LCP_ID]           = boot_platform_pre_load_lcp,
    [RSS_FIRMWARE_SI_CL0_ID]        = boot_platform_pre_load_si_cl0,
    [RSS_FIRMWARE_SI_CL1_ID]        = boot_platform_pre_load_si_cl1,
    [RSS_FIRMWARE_SI_CL2_ID]        = boot_platform_pre_load_si_cl2,
    [RSS_FIRMWARE_AP_BL2_ID]        = boot_platform_pre_load_ap_bl2,
};

/* Array of function pointers to call after each image is loaded indexed by image id */
static int (*boot_platform_post_load_vector[RSS_FIRMWARE_COUNT]) (void) = {
    [RSS_FIRMWARE_SECURE_ID]        = boot_platform_post_load_secure,
    [RSS_FIRMWARE_NON_SECURE_ID]    = boot_platform_post_load_non_secure,
    [RSS_FIRMWARE_SCP_ID]           = boot_platform_post_load_scp,
    [RSS_FIRMWARE_LCP_ID]           = boot_platform_post_load_lcp,
    [RSS_FIRMWARE_SI_CL0_ID]        = boot_platform_post_load_si_cl0,
    [RSS_FIRMWARE_SI_CL1_ID]        = boot_platform_post_load_si_cl1,
    [RSS_FIRMWARE_SI_CL2_ID]        = boot_platform_post_load_si_cl2,
    [RSS_FIRMWARE_AP_BL2_ID]        = boot_platform_post_load_ap_bl2,
};

/*
 * ============================== LOAD FUNCTIONS ==============================
 */
int boot_platform_pre_load(uint32_t image_id)
{

    if (image_id >= RSS_FIRMWARE_COUNT) {
        BOOT_LOG_WRN("BL2: no pre load for image %d", image_id);
        return 0;
    }

    return boot_platform_pre_load_vector[image_id]();
}

int boot_platform_post_load(uint32_t image_id)
{
    if (image_id >= RSS_FIRMWARE_COUNT) {
        BOOT_LOG_WRN("BL2: no post load for image %d", image_id);
        return 0;
    }

    return boot_platform_post_load_vector[image_id]();
}
