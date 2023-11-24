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

#ifndef FWU_METADATA_AP_FLASH_DEV
    #ifndef FLASH_DEV_NAME
    #error "FWU_METADATA_AP_FLASH_DEV or AP_FLASH_DEV_NAME must be defined in flash_layout.h"
    #else
    #define FWU_METADATA_AP_FLASH_DEV AP_FLASH_DEV_NAME
    #endif
#endif

int is_initialized_ap = 0;
struct fwu_metadata_ap fwu_md_ap;

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
