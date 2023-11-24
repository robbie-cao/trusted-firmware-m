/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdint.h>
#include <string.h>
#include "fwu_agent.h"
#include "Driver_Flash.h"
#include "flash_layout.h"
#include "region_defs.h"
#include "uefi_capsule_parser.h"
#include "platform_base_address.h"
#include "tfm_plat_nv_counters.h"
#include "tfm_plat_defs.h"
#include "uefi_fmp.h"
#include "uart_stdout.h"
#include "soft_crc/soft_crc.h"
#include "cmsis.h"
#include "tfm_hal_platform.h"
#include "uart_stdout.h"

extern ARM_DRIVER_FLASH FWU_METADATA_AP_FLASH_DEV;
extern ARM_DRIVER_FLASH FWU_METADATA_RSS_FLASH_DEV;
extern int is_initialized_rss;
extern struct fwu_metadata_rss fwu_md_rss;

#define HOST_ACK_TIMEOUT_SEC    (6 * 60) /* About 360s, not exact. */

capsule_image_info_t capsule_info;
struct efi_guid full_capsule_image_guid = {
    .time_low = 0xe2bb9c06,
    .time_mid = 0x70e9,
    .time_hi_and_version = 0x4b14,
    .clock_seq_and_node = {0x97, 0xa3, 0x5a, 0x79, 0x13, 0x17, 0x6e, 0x3f}
};
#ifndef FWU_METADATA_AP_FLASH_DEV
    #ifndef FLASH_DEV_NAME
    #error "FWU_METADATA_AP_FLASH_DEV or AP_FLASH_DEV_NAME must be defined in flash_layout.h"
    #else
    #define FWU_METADATA_AP_FLASH_DEV AP_FLASH_DEV_NAME
    #endif
#endif

int is_initialized_ap = 0;
struct fwu_metadata_ap fwu_md_ap;

/* Stage nv counter into private metadata section of the flash.
 * Staged nv counters are written to the otp when firmware update is successful.
 * The function assumes that the api is called in the boot loading stage.
 */
enum fwu_agent_error_t fwu_stage_nv_counter(enum fwu_nv_counter_index_t index,
        uint32_t img_security_cnt)
{
    struct fwu_private_metadata priv_metadata;

    FWU_LOG_MSG("%s: enter: index = %u, val = %u\n\r", __func__,
                                index, img_security_cnt);

    if (!is_initialized_rss) {
        FWU_ASSERT(0);
    }

    if (index > FWU_MAX_NV_COUNTER_INDEX) {
        return FWU_AGENT_ERROR;
    }

    if (private_metadata_read(&priv_metadata)) {
        FWU_ASSERT(0);
    }

    if (priv_metadata.nv_counter[index] != img_security_cnt) {
        priv_metadata.nv_counter[index] = img_security_cnt;
        if (private_metadata_write(&priv_metadata)) {
            FWU_ASSERT(0);
        }
    }

    FWU_LOG_MSG("%s: exit\n\r", __func__);
    return FWU_AGENT_SUCCESS;
}

uint8_t bl2_get_boot_bank(void)
{
    uint8_t boot_index;
    struct fwu_private_metadata priv_metadata;
    FWU_LOG_MSG("%s: enter", __func__);

    if (fwu_metadata_init(FWU_RSS_FLASH_DEV, &is_initialized_rss)) {
        FWU_ASSERT(0);
    }

    if (private_metadata_read(&priv_metadata)) {
        FWU_ASSERT(0);
    }

    boot_index = priv_metadata.boot_index;
    FWU_LOG_MSG("%s: exit: booting from bank = %u\r\n", __func__, boot_index);
    return boot_index;
}

static enum fwu_agent_error_t erase_bank(enum FWU_METADATA_FLASH_DEV DEV_TYPE, uint32_t bank_offset)
{
    int32_t ret;
    uint32_t sectors;
    uint8_t img_num;
    ARM_DRIVER_FLASH *FLASH_DEV;
    enum fwu_agent_error_t agent_ret;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if ((bank_offset % FWU_METADATA_FLASH_SECTOR_SIZE) != 0) {
        return FWU_AGENT_ERROR;
    }

    if ((BANK_PARTITION_SIZE % FWU_METADATA_FLASH_SECTOR_SIZE) != 0) {
        return FWU_AGENT_ERROR;
    }

    agent_ret = get_fwu_flash_and_img_num(DEV_TYPE, &FLASH_DEV, &img_num);
    if (agent_ret != FWU_AGENT_SUCCESS) {
        return FWU_AGENT_ERROR;
    }

    sectors = BANK_PARTITION_SIZE / FWU_METADATA_FLASH_SECTOR_SIZE;

    FWU_LOG_MSG("%s: erasing sectors = %u, from offset = %u\n\r", __func__,
                     sectors, bank_offset);

    for (uint32_t i = 0; i < sectors; i++) {
        ret = FLASH_DEV->EraseSector(
                bank_offset + (i * FWU_METADATA_FLASH_SECTOR_SIZE));
        if (ret != ARM_DRIVER_OK) {
            return FWU_AGENT_ERROR;
        }
    }

    FWU_LOG_MSG("%s: exit\n\r", __func__);
    return FWU_AGENT_SUCCESS;
}

static int get_image_info_in_bank(struct efi_guid* guid, uint32_t* image_bank_offset)
{
    if ((memcmp(guid, &full_capsule_image_guid, sizeof(struct efi_guid))) == 0) {
        *image_bank_offset = 0;
        return IMAGE_ALL;
    }

    return IMAGE_NOT_RECOGNIZED;
}

enum fwu_agent_error_t fwu_metadata_provision_ap(void)
{
    enum fwu_agent_error_t ret;
    uint32_t image_version = FWU_IMAGE_INITIAL_VERSION;

    ret = fwu_metadata_init(FWU_AP_FLASH_DEV, &is_initialized_ap);
    if (ret) {
        return ret;
    }
    /* Check by chance if the previous reboot had a firmware data.
     * If yes, then don't initialize metadata.
     */
    metadata_read(FWU_AP_FLASH_DEV, &fwu_md_ap.md);
    if(fwu_md_ap.md.active_index < 2 || fwu_md_ap.md.previous_active_index <2){
        if(fwu_md_ap.md.active_index ^ fwu_md_ap.md.previous_active_index)
            return FWU_AGENT_SUCCESS;
    }
    /* Provision FWU Agent Metadata AP secure flash. */
    memset(&fwu_md_ap, 0, sizeof(struct fwu_metadata_ap));

    fwu_md_ap.md.version = 1;
    fwu_md_ap.md.active_index = BANK_0;
    fwu_md_ap.md.previous_active_index = BANK_1;

    /* Bank-0 is the place where images are located at the start
     * of device lifecycle.
     */
    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_AP_FW_BANK; i++) {

        fwu_md_ap.img_entry[i].img_props[BANK_0].accepted = IMAGE_ACCEPTED;
        fwu_md_ap.img_entry[i].img_props[BANK_0].version = image_version;

        fwu_md_ap.img_entry[i].img_props[BANK_1].accepted = IMAGE_NOT_ACCEPTED;
        fwu_md_ap.img_entry[i].img_props[BANK_1].version = INVALID_VERSION;
    }

    /* Calculate CRC32 for fwu metadata. */
    fwu_md_ap.md.crc_32 = crc32((uint8_t *)&fwu_md_ap.md.version,
                             sizeof(struct fwu_metadata_ap) - sizeof(uint32_t));

    ret = metadata_write(FWU_AP_FLASH_DEV, &fwu_md_ap.md);
    if (ret) {
        return ret;
    }

    memset(&fwu_md_ap, 0, sizeof(struct fwu_metadata_ap));
    ret = metadata_read(FWU_AP_FLASH_DEV, &fwu_md_ap.md);
    if (ret) {
        return ret;
    }
    FWU_LOG_MSG("%s: provisioned values: active = %u, previous = %d\n\r",
             __func__, fwu_md_ap.md.active_index, fwu_md_ap.md.previous_active_index);

    FWU_LOG_MSG("%s: FWU METADATA PROVISIONED.\n\r", __func__);
    return FWU_AGENT_SUCCESS;
}

static enum fwu_agent_error_t flash_fip_capsule(struct fwu_metadata_ap* fwu_md_ptr,
                                        void* images, uint32_t size, uint32_t version)
{
    int ret;
    uint32_t bank_offset;
    uint32_t previous_active_index;
    uint32_t active_index;

    FWU_LOG_MSG("%s: enter: image = %p, size = %u, version = %u\n\r"
                , __func__, images, size, version);

    if (!fwu_md_ptr || !images) {
        return FWU_AGENT_ERROR;
    }

    active_index = fwu_md_ptr->md.active_index;

    if (size > FIP_BANK_PARTITION_SIZE) {
        FWU_LOG_MSG("ERROR: %s: size error\n\r",__func__);
        return FWU_AGENT_ERROR;
    }

    if (version <=
            (fwu_md_ptr->img_entry[IMAGE_0].img_props[active_index].version)) {
        FWU_LOG_MSG("ERROR: %s: version error\n\r",__func__);
        return FWU_AGENT_ERROR;
    }

    if (active_index == BANK_0) {
        previous_active_index = BANK_1;
        bank_offset = FIP_BANK_1_PARTITION_OFFSET;
    } else if (active_index == BANK_1) {
        previous_active_index = BANK_0;
        bank_offset = FIP_BANK_0_PARTITION_OFFSET;
    } else {
        FWU_LOG_MSG("ERROR: %s: active_index %d\n\r",__func__,active_index);
        return FWU_AGENT_ERROR;
    }

    if (erase_bank(FWU_AP_FLASH_DEV, bank_offset)) {
        return FWU_AGENT_ERROR;
    }
    FWU_LOG_MSG("%s: writing capsule to the flash at offset = %u...\n\r",
                      __func__, bank_offset);
    ret = FWU_METADATA_AP_FLASH_DEV.ProgramData(bank_offset, images, size);
    if (ret != size) {
        return FWU_AGENT_ERROR;
    }
    FWU_LOG_MSG("%s: images are written to bank offset = %u\n\r", __func__,
                     bank_offset);

    /* Change system state to trial bank state. */
    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_AP_FW_BANK; i++) {
        fwu_md_ptr->img_entry[i].img_props[previous_active_index].accepted =
                                                        IMAGE_NOT_ACCEPTED;
        fwu_md_ptr->img_entry[i].img_props[previous_active_index].version = version;
    }
    fwu_md_ptr->md.active_index = previous_active_index;
    fwu_md_ptr->md.previous_active_index = active_index;
    fwu_md_ptr->md.crc_32 = crc32((uint8_t *)&(fwu_md_ptr->md.version),
                              sizeof(struct fwu_metadata_ap) - sizeof(uint32_t));

    ret = metadata_write(FWU_AP_FLASH_DEV, &fwu_md_ptr->md);
    if (ret) {
        return ret;
    }

    FWU_LOG_MSG("%s: exit\n\r", __func__);
    return FWU_AGENT_SUCCESS;
}

static enum fwu_agent_error_t flash_rss_capsule(struct fwu_metadata_rss* fwu_md_ptr,
                                         void* images, uint32_t size, uint32_t version)
{
    int ret;
    uint32_t bank_offset;
    uint32_t previous_active_index;
    uint32_t active_index;

    FWU_LOG_MSG("%s: enter: image = %p, size = %u, version = %u\n\r"
                , __func__, images, size, version);

    if (!fwu_md_ptr || !images) {
        return FWU_AGENT_ERROR;
    }

    active_index = fwu_md_ptr->md.active_index;

    if (size > BANK_PARTITION_SIZE) {
        FWU_LOG_MSG("ERROR: %s: size error\n\r",__func__);
        return FWU_AGENT_ERROR;
    }

    if (version <=
            (fwu_md_ptr->img_entry[IMAGE_0].img_props[active_index].version)) {
        FWU_LOG_MSG("ERROR: %s: version error\n\r",__func__);
        return FWU_AGENT_ERROR;
    }

    if (active_index == BANK_0) {
        previous_active_index = BANK_1;
        bank_offset = BANK_1_PARTITION_OFFSET;
    } else if (active_index == BANK_1) {
        previous_active_index = BANK_0;
        bank_offset = BANK_0_PARTITION_OFFSET;
    } else {
        FWU_LOG_MSG("ERROR: %s: active_index %d\n\r",__func__,active_index);
        return FWU_AGENT_ERROR;
    }

    if (erase_bank(FWU_RSS_FLASH_DEV, bank_offset)) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: writing capsule to the flash at offset = %u...\n\r",
                      __func__, bank_offset);
    ret = FWU_METADATA_RSS_FLASH_DEV.ProgramData(bank_offset, images, size);
    if (ret != size) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: images are written to bank offset = %u\n\r", __func__,
                     bank_offset);

    /* Change system state to trial bank state. */
    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_RSS_FW_BANK; i++) {
        fwu_md_ptr->img_entry[i].img_props[previous_active_index].accepted =
                                                        IMAGE_NOT_ACCEPTED;
        fwu_md_ptr->img_entry[i].img_props[previous_active_index].version = version;
    }
    fwu_md_ptr->md.active_index = previous_active_index;
    fwu_md_ptr->md.previous_active_index = active_index;
    fwu_md_ptr->md.crc_32 = crc32((uint8_t *)&(fwu_md_ptr->md.version),
                              sizeof(struct fwu_metadata_rss) - sizeof(uint32_t));

    ret = metadata_write(FWU_RSS_FLASH_DEV, &fwu_md_ptr->md);
    if (ret) {
        return ret;
    }

    FWU_LOG_MSG("%s: exit\n\r", __func__);
    return FWU_AGENT_SUCCESS;
}

enum fwu_agent_error_t fwu_flash_image(void)
{
    enum fwu_agent_error_t rss_ret;
    enum fwu_agent_error_t fip_ret;
    enum fwu_agent_state_t current_state;
    void *capsule_ptr = (char*)HOST_CAPSULE_DRAM_LOG_BASE;
    int image_index;
    uint32_t image_bank_offset;
    uint32_t nr_images;
    struct fwu_private_metadata priv_metadata;
    void *fip_img_ptr;
    enum fwu_agent_error_t ret;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if (!is_initialized_ap || !is_initialized_rss) {
        FWU_LOG_MSG("fwu flash has not been initialized!\r\n");
    }

    if (metadata_read(FWU_RSS_FLASH_DEV, &fwu_md_rss.md)) {
        FWU_LOG_MSG("fwu read rss metadata failed!\r\n");
        return FWU_AGENT_ERROR;
    }

    if (metadata_read(FWU_AP_FLASH_DEV, &fwu_md_ap.md)) {
        FWU_LOG_MSG("fwu read ap metadata failed!\r\n");
        return FWU_AGENT_ERROR;
    }

    if (private_metadata_read(&priv_metadata)) {
        FWU_LOG_MSG("fwu read private metadata failed!\r\n");
        return FWU_AGENT_ERROR;
    }

    /* Firmware update process can only start in regular state. */
    current_state = get_fwu_agent_state(&fwu_md_rss, &priv_metadata);
    if (current_state != FWU_AGENT_STATE_REGULAR) {
        FWU_LOG_MSG("fwu metadata current state is not REGULAR!\r\n");
        return FWU_AGENT_ERROR;
    }

    memset(&capsule_info, 0, sizeof(capsule_image_info_t));
    if (uefi_capsule_retrieve_images(capsule_ptr, &capsule_info)) {
        FWU_LOG_MSG("fwu retrieve images failed!\r\n");
        return FWU_AGENT_ERROR;
    }
    nr_images = capsule_info.nr_image;

    for (uint32_t i = 0; i < nr_images; i++) {
        image_index = get_image_info_in_bank(&capsule_info.guid[i],
                                &image_bank_offset);
        switch(image_index) {
        case IMAGE_ALL:
            rss_ret = flash_rss_capsule(&fwu_md_rss, capsule_info.image[i],
                                       capsule_info.size[i] - SIZE_DEF_FIP_IMAGE,
                                       capsule_info.version[i]);
            fip_img_ptr = capsule_info.image[i] + capsule_info.size[i]
                          - SIZE_DEF_FIP_IMAGE - sizeof(fmp_payload_header_t);
            fip_ret = flash_fip_capsule(&fwu_md_ap, fip_img_ptr,
                          SIZE_DEF_FIP_IMAGE, capsule_info.version[i]);
            if (rss_ret != FWU_AGENT_SUCCESS || fip_ret != FWU_AGENT_SUCCESS) {
                priv_metadata.fmp_last_attempt_version = capsule_info.version[i];
                priv_metadata.fmp_last_attempt_status = LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL;
                private_metadata_write(&priv_metadata);
                fmp_set_image_info(&full_capsule_image_guid,
                        priv_metadata.fmp_version,
                        priv_metadata.fmp_last_attempt_version,
                        priv_metadata.fmp_last_attempt_status);
                ret = FWU_AGENT_ERROR;
            } else {
                ret = FWU_AGENT_SUCCESS;
            }
            break;
        default:
            FWU_LOG_MSG("%s: sent image not recognized\n\r", __func__);
            ret = FWU_AGENT_ERROR;
            break;
        }
    }

out:

    FWU_LOG_MSG("%s: exit: ret = %d\n\r", __func__, ret);
    return ret;
}

static enum fwu_agent_error_t accept_full_capsule(
          struct fwu_metadata_rss* fwu_md_rss,
          struct fwu_metadata_ap* fwu_md_ap,
          struct fwu_private_metadata* priv_metadata)
{
    uint32_t active_index;
    enum fwu_agent_error_t ret;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if (!fwu_md_rss) {
        return FWU_AGENT_ERROR;
    }

    active_index = fwu_md_rss->md.active_index;

    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_RSS_FW_BANK; i++) {
        fwu_md_rss->img_entry[i].img_props[active_index].accepted =
                                                            IMAGE_ACCEPTED;
    }

    active_index = fwu_md_ap->md.active_index;
    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_AP_FW_BANK; i++) {
        fwu_md_ap->img_entry[i].img_props[active_index].accepted =
                                                            IMAGE_ACCEPTED;
    }

    priv_metadata->boot_attempted = 0;

    ret = private_metadata_write(priv_metadata);
    if (ret) {
        return ret;
    }
    fwu_md_rss->md.crc_32 = crc32((uint8_t *)&fwu_md_rss->md.version,
                              sizeof(struct fwu_metadata_rss) - sizeof(uint32_t));

    ret = metadata_write(FWU_RSS_FLASH_DEV, &fwu_md_rss->md);
    if (ret) {
        return ret;
    }

    fwu_md_ap->md.crc_32 = crc32((uint8_t *)&fwu_md_ap->md.version,
                              sizeof(struct fwu_metadata_ap) - sizeof(uint32_t));

    ret = metadata_write(FWU_AP_FLASH_DEV, &fwu_md_ap->md);
    if (ret) {
        return ret;
    }

    FWU_LOG_MSG("%s: exit: fwu state is changed to regular\n\r", __func__);
    return FWU_AGENT_SUCCESS;
}

static enum fwu_agent_error_t fwu_accept_image(struct efi_guid* guid,
                                               struct fwu_metadata_rss *fwu_md_rss,
                                               struct fwu_metadata_ap *fwu_md_ap,
                                               struct fwu_private_metadata *priv_metadata)
{
    enum fwu_agent_state_t current_state;
    int image_index;
    uint32_t image_bank_offset;
    enum fwu_agent_error_t ret;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    /* It is expected to receive this call only when in trial state. */
    current_state = get_fwu_agent_state(fwu_md_rss, priv_metadata);
    if (current_state != FWU_AGENT_STATE_TRIAL) {
        return FWU_AGENT_ERROR;
    }

    /* Booted from previous_active_bank, not expected
     * to receive this call in this state, rather host should
     * call fwu_select_previous.
     */
    if (fwu_md_rss->md.active_index != priv_metadata->boot_index) {
        return FWU_AGENT_ERROR;
    }

    image_index = get_image_info_in_bank(guid, &image_bank_offset);
    switch(image_index) {
    case IMAGE_ALL:
        ret = accept_full_capsule(fwu_md_rss, fwu_md_ap, priv_metadata);
        break;
    default:
        FWU_LOG_MSG("%s: sent image not recognized\n\r", __func__);
        ret = FWU_AGENT_ERROR;
        break;
    }

    FWU_LOG_MSG("%s: exit: ret = %d\n\r", __func__, ret);
    return ret;
}

static enum fwu_agent_error_t fwu_select_previous(
                           struct fwu_metadata_rss *fwu_md_rss,
                           struct fwu_metadata_ap *fwu_md_ap,
                           struct fwu_private_metadata *priv_metadata)
{
    enum fwu_agent_error_t ret;
    enum fwu_agent_state_t current_state;
    uint32_t index;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    /* It is expected to receive this call only when in trial state. */
    current_state = get_fwu_agent_state(fwu_md_rss, priv_metadata);
    if (current_state != FWU_AGENT_STATE_TRIAL) {
        return FWU_AGENT_ERROR;
    }

    /* Not expected to receive this call in this state, system
     * did not boot from previous active index.
     */
    if (fwu_md_rss->md.previous_active_index != priv_metadata->boot_index) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: trial state: active index = %u, previous active = %u\n\r",
            __func__, fwu_md_rss->md.active_index, fwu_md_rss->md.previous_active_index);

    index = fwu_md_rss->md.previous_active_index;
    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_RSS_FW_BANK; i++) {
        if (fwu_md_rss->img_entry[i].img_props[index].accepted != IMAGE_ACCEPTED)
        {
            FWU_ASSERT(0);
        }
    }

    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_AP_FW_BANK; i++) {
        if (fwu_md_ap->img_entry[i].img_props[index].accepted != IMAGE_ACCEPTED)
        {
            FWU_ASSERT(0);
        }
    }

    index = fwu_md_rss->md.active_index;
    fwu_md_rss->md.active_index = fwu_md_rss->md.previous_active_index;
    fwu_md_rss->md.previous_active_index = index;

    index = fwu_md_ap->md.active_index;
    fwu_md_ap->md.active_index = fwu_md_ap->md.previous_active_index;
    fwu_md_ap->md.previous_active_index = index;

    priv_metadata->boot_attempted = 0;

    ret = private_metadata_write(priv_metadata);
    if (ret) {
        return ret;
    }
    fwu_md_rss->md.crc_32 = crc32((uint8_t *)&fwu_md_rss->md.version,
                              sizeof(struct fwu_metadata_rss) - sizeof(uint32_t));

    fwu_md_ap->md.crc_32 = crc32((uint8_t *)&fwu_md_ap->md.version,
                              sizeof(struct fwu_metadata_ap) - sizeof(uint32_t));

    ret = metadata_write(FWU_RSS_FLASH_DEV, &fwu_md_rss->md);
    if (ret) {
        return ret;
    }

    ret = metadata_write(FWU_AP_FLASH_DEV, &fwu_md_ap->md);
    if (ret) {
        return ret;
    }

    FWU_LOG_MSG("%s: in regular state by choosing previous active bank\n\r",
                 __func__);

    FWU_LOG_MSG("%s: exit: ret = %d\n\r", __func__, ret);
    return ret;

}

static enum fwu_agent_error_t update_nv_counters(
                        struct fwu_private_metadata* priv_metadata)
{
    enum tfm_plat_err_t err;
    uint32_t security_cnt;
    enum tfm_nv_counter_t tfm_nv_counter_i;

    for (uint8_t i = 0; i <= FWU_MAX_NV_COUNTER_INDEX; i++) {
        switch (i) {
        case FWU_BL2_NV_COUNTER:
            tfm_nv_counter_i = PLAT_NV_COUNTER_BL1_0;
            break;
        case FWU_TFM_NV_COUNTER:
            tfm_nv_counter_i = PLAT_NV_COUNTER_BL2_1;
            break;
        case FWU_FIP_NV_COUNTER:
            tfm_nv_counter_i = PLAT_NV_COUNTER_BL2_2;
            break;
        case FWU_SI_CL2_NV_COUNTER:
            tfm_nv_counter_i = PLAT_NV_COUNTER_BL2_3;
            break;
        case FWU_SI_CL1_NV_COUNTER:
            tfm_nv_counter_i = PLAT_NV_COUNTER_BL2_4;
            break;
        case FWU_SI_CL0_NV_COUNTER:
            tfm_nv_counter_i = PLAT_NV_COUNTER_BL2_5;
            break;
        case FWU_LCP_NV_COUNTER:
            tfm_nv_counter_i = PLAT_NV_COUNTER_BL2_6;
            break;
        case FWU_SCP_NV_COUNTER:
            tfm_nv_counter_i = PLAT_NV_COUNTER_BL2_7;
            break;
        default:
            FWU_ASSERT(0);
            break;
        }

        err = tfm_plat_read_nv_counter(tfm_nv_counter_i,
                        sizeof(security_cnt), (uint8_t *)&security_cnt);
        if (err != TFM_PLAT_ERR_SUCCESS) {
            return FWU_AGENT_ERROR;
        }

        if (priv_metadata->nv_counter[i] < security_cnt) {
            return FWU_AGENT_ERROR;
        } else if (priv_metadata->nv_counter[i] > security_cnt) {
            FWU_LOG_MSG("%s: updaing index = %u nv counter = %u->%u\n\r",
                        __func__, i, security_cnt,
                        priv_metadata->nv_counter[FWU_BL2_NV_COUNTER]);
            err = tfm_plat_set_nv_counter(tfm_nv_counter_i,
                                    priv_metadata->nv_counter[FWU_BL2_NV_COUNTER]);
            if (err != TFM_PLAT_ERR_SUCCESS) {
                return FWU_AGENT_ERROR;
            }
        }
    }

    FWU_LOG_MSG("%s: exit\n\r", __func__);
    return FWU_AGENT_SUCCESS;
}

static void disable_host_ack_timer(void)
{
    FWU_LOG_MSG("%s: timer to reset is disabled\n\r", __func__);
    SysTick->CTRL &= (~SysTick_CTRL_ENABLE_Msk);
}

enum fwu_agent_error_t fwu_host_ack(void)
{
    enum fwu_agent_error_t ret;
    struct fwu_private_metadata priv_metadata;
    enum fwu_agent_state_t current_state;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if (!is_initialized_rss || !is_initialized_ap) {
        return FWU_AGENT_ERROR;
    }

    if (metadata_read(FWU_RSS_FLASH_DEV, &fwu_md_rss.md)) {
        FWU_ASSERT(0);
    }

    if (metadata_read(FWU_AP_FLASH_DEV, &fwu_md_ap.md)) {
        FWU_ASSERT(0);
    }

    if (private_metadata_read(&priv_metadata)) {
        ret = FWU_AGENT_ERROR;
        goto out;
    }

    current_state = get_fwu_agent_state(&fwu_md_rss, &priv_metadata);
    if (current_state == FWU_AGENT_STATE_REGULAR) {

        /* Nothing to be done. */
        ret = FWU_AGENT_SUCCESS;

        fmp_set_image_info(&full_capsule_image_guid,
                priv_metadata.fmp_version,
                priv_metadata.fmp_last_attempt_version,
                priv_metadata.fmp_last_attempt_status);

        goto out;

    } else if (current_state != FWU_AGENT_STATE_TRIAL) {
        FWU_ASSERT(0);
    }

    if (fwu_md_rss.md.active_index != priv_metadata.boot_index) {

        /* Firmware update failed, revert back to previous bank. */
        priv_metadata.fmp_last_attempt_version =
         fwu_md_rss.img_entry[IMAGE_0].img_props[fwu_md_rss.md.active_index].version;

        priv_metadata.fmp_last_attempt_status = LAST_ATTEMPT_STATUS_ERROR_UNSUCCESSFUL;

        ret = fwu_select_previous(&fwu_md_rss, &fwu_md_ap, &priv_metadata);

    } else {

        /* Firmware update successful. */
        priv_metadata.fmp_version =
         fwu_md_rss.img_entry[IMAGE_0].img_props[fwu_md_rss.md.active_index].version;
        priv_metadata.fmp_last_attempt_version =
         fwu_md_rss.img_entry[IMAGE_0].img_props[fwu_md_rss.md.active_index].version;

        priv_metadata.fmp_last_attempt_status = LAST_ATTEMPT_STATUS_SUCCESS;

        ret = fwu_accept_image(&full_capsule_image_guid, &fwu_md_rss, &fwu_md_ap,
                                &priv_metadata);
        if (!ret) {
            ret = update_nv_counters(&priv_metadata);
        }
    }

    if (ret == FWU_AGENT_SUCCESS) {
        disable_host_ack_timer();
        fmp_set_image_info(&full_capsule_image_guid,
                priv_metadata.fmp_version,
                priv_metadata.fmp_last_attempt_version,
                priv_metadata.fmp_last_attempt_status);
    }

out:

    FWU_LOG_MSG("%s: exit: ret = %d\n\r", __func__, ret);
    return ret;
}

static int systic_counter = 0;

void SysTick_Handler(void)
{
    systic_counter++;
    if (systic_counter % 10 == 0) {
        SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
        stdio_output_string("*", 1);
        SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    }
    if (systic_counter == HOST_ACK_TIMEOUT_SEC) {
        SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
        stdio_output_string("timer expired!\n\r",
                           sizeof("timer expired!\n\r"));
        tfm_hal_system_reset();
    }
}

/* When in trial state, start the timer for host to respond.
 * Diable timer when host responds back either by calling
 * fwu_accept_image or fwu_select_previous.
 * Otherwise, resets the system.
 */
void host_acknowledgement_timer_to_reset(void)
{
    struct fwu_private_metadata priv_metadata;
    enum fwu_agent_state_t current_state;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if (!is_initialized_rss || !is_initialized_ap) {
        FWU_ASSERT(0);
    }

    if (private_metadata_read(&priv_metadata)) {
        FWU_ASSERT(0);
    }

    if (metadata_read(FWU_RSS_FLASH_DEV, &fwu_md_rss.md)) {
        FWU_ASSERT(0);
    }

    current_state = get_fwu_agent_state(&fwu_md_rss, &priv_metadata);

    if (current_state == FWU_AGENT_STATE_TRIAL) {
        FWU_LOG_MSG("%s: in trial state, starting host ack timer\n\r",
                        __func__);
        systic_counter = 0;
        if (SysTick_Config(SysTick_LOAD_RELOAD_Msk)) {
            FWU_LOG_MSG("%s: timer init failed\n\r", __func__);
            FWU_ASSERT(0);
        } else {
            FWU_LOG_MSG("%s: timer started: seconds to expire : %u\n\r",
                        __func__, HOST_ACK_TIMEOUT_SEC);
        }
    }

    FWU_LOG_MSG("%s: exit\n\r", __func__);
    return;
}
