/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
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

#ifndef FWU_METADATA_RSS_FLASH_DEV
    #ifndef FLASH_DEV_NAME
    #error "FWU_METADATA_RSS_FLASH_DEV or RSS_FLASH_DEV_NAME must be defined in flash_layout.h"
    #else
    #define FWU_METADATA_RSS_FLASH_DEV RSS_FLASH_DEV_NAME
    #endif
#endif

/* Import the CMSIS flash device driver */
extern ARM_DRIVER_FLASH FWU_METADATA_RSS_FLASH_DEV;

struct fwu_metadata_rss fwu_md_rss;
int is_initialized_rss = 0;

enum fwu_agent_error_t get_fwu_flash_and_img_num(enum FWU_METADATA_FLASH_DEV dev_type,
                               ARM_DRIVER_FLASH **FLASH_DEV, uint8_t *img_num)
{
    enum fwu_agent_error_t ret = FWU_AGENT_SUCCESS;

    switch (dev_type) {
    case FWU_RSS_FLASH_DEV:
        *FLASH_DEV = &FWU_METADATA_RSS_FLASH_DEV;
        *img_num = NR_OF_IMAGES_IN_RSS_FW_BANK;
        break;
    default:
        FWU_LOG_MSG("%s: This device is not supported!\n\r", __func__);
        ret = FWU_AGENT_ERROR;
        break;
    }

    return ret;
}

enum fwu_agent_state_t get_fwu_agent_state(
        struct fwu_metadata_rss *fwu_md_rss_ptr,
        struct fwu_private_metadata *priv_metadata_ptr)
{
    uint32_t boot_index;

    boot_index = priv_metadata_ptr->boot_index;

    FWU_LOG_MSG("%s: enter, boot_index=%d\n\r", __func__, boot_index);

    if (boot_index != fwu_md_rss_ptr->md.active_index) {
        return FWU_AGENT_STATE_TRIAL;
    }

    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_RSS_FW_BANK; i++) {
        if ((fwu_md_rss_ptr->img_entry[i].img_props[boot_index].accepted)
                == (IMAGE_NOT_ACCEPTED)) {
            return FWU_AGENT_STATE_TRIAL;
        }
    }

    FWU_LOG_MSG("%s: exit: FWU_AGENT_STATE_REGULAR\n\r", __func__);
    return FWU_AGENT_STATE_REGULAR;
}

enum fwu_agent_error_t private_metadata_write(struct fwu_private_metadata* p_metadata)
{
    int32_t ret;

    FWU_LOG_MSG("%s: enter: boot_index = %u\n\r", __func__,
                        p_metadata->boot_index);

    if (!p_metadata) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_RSS_FLASH_DEV.EraseSector(FWU_PRIVATE_METADATA_REPLICA_1_OFFSET);
    if (ret != ARM_DRIVER_OK) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_RSS_FLASH_DEV.ProgramData(FWU_PRIVATE_METADATA_REPLICA_1_OFFSET,
                                p_metadata, sizeof(struct fwu_private_metadata));
    if (ret != sizeof(struct fwu_private_metadata)) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: success\n\r", __func__);
    return FWU_AGENT_SUCCESS;
}

enum fwu_agent_error_t private_metadata_read(struct fwu_private_metadata* p_metadata)
{
    int32_t ret;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if (!p_metadata) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_RSS_FLASH_DEV.ReadData(FWU_PRIVATE_METADATA_REPLICA_1_OFFSET, p_metadata,
                                          sizeof(struct fwu_private_metadata));
    if (ret != sizeof(struct fwu_private_metadata)) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: success: boot_index = %u\n\r", __func__,
                        p_metadata->boot_index);

    return FWU_AGENT_SUCCESS;
}

static enum fwu_agent_error_t metadata_validate(struct metadata *p_metadata, uint8_t img_num)
{
    uint32_t fwu_md_size;
    uint32_t calculated_crc32;

    FWU_LOG_MSG("%s: enter:\n\r", __func__);

    if (!p_metadata) {
        return FWU_AGENT_ERROR;
    }

    fwu_md_size = sizeof(struct metadata) + img_num * sizeof(struct fwu_image_entry);

    calculated_crc32 = crc32((uint8_t *)&(p_metadata->version),
                                      fwu_md_size - sizeof(uint32_t));

    if (p_metadata->crc_32 != calculated_crc32) {
        FWU_LOG_MSG("%s: failed: crc32 calculated: 0x%x, given: 0x%x\n\r", __func__,
                    calculated_crc32, p_metadata->crc_32);
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: success\n\r", __func__);

    return FWU_AGENT_SUCCESS;
}

enum fwu_agent_error_t metadata_read(enum FWU_METADATA_FLASH_DEV DEV_TYPE,
                                           struct metadata *p_metadata)
{
    int32_t ret;
    uint32_t fwu_md_size;
    uint8_t img_num;
    ARM_DRIVER_FLASH *FLASH_DEV;
    enum fwu_agent_error_t agent_ret;

    FWU_LOG_MSG("%s: enter: flash addr = %u, size = %d\n\r", __func__,
                  FWU_METADATA_REPLICA_1_OFFSET, sizeof(struct metadata));

    if (!p_metadata) {
        return FWU_AGENT_ERROR;
    }

    agent_ret = get_fwu_flash_and_img_num(DEV_TYPE, &FLASH_DEV, &img_num);
    if (agent_ret != FWU_AGENT_SUCCESS) {
        return FWU_AGENT_ERROR;
    }

    fwu_md_size = sizeof(struct metadata) + img_num * sizeof(struct fwu_image_entry);

    ret = FLASH_DEV->ReadData(FWU_METADATA_REPLICA_1_OFFSET,
                             p_metadata, fwu_md_size);
    if (ret != fwu_md_size) {
        return FWU_AGENT_ERROR;
    }

    if (metadata_validate(p_metadata, img_num) != FWU_AGENT_SUCCESS) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: success: active = %u, previous = %d\n\r", __func__,
                  p_metadata->active_index, p_metadata->previous_active_index);

    return FWU_AGENT_SUCCESS;
}

enum fwu_agent_error_t fwu_metadata_init(enum FWU_METADATA_FLASH_DEV DEV_TYPE,
                                         int *is_initialized)
{
    enum fwu_agent_error_t agent_ret;
    ARM_FLASH_INFO* flash_info;
    uint32_t fwu_md_size;
    ARM_DRIVER_FLASH *FLASH_DEV;
    uint8_t img_num;
    int32_t ret;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if (*is_initialized) {
        FWU_LOG_MSG("%s: flash had been initialized, return\n\r", __func__);
        return FWU_AGENT_SUCCESS;
    }

    agent_ret = get_fwu_flash_and_img_num(DEV_TYPE, &FLASH_DEV, &img_num);
    if (agent_ret != FWU_AGENT_SUCCESS) {
        return FWU_AGENT_ERROR;
    }

    fwu_md_size = sizeof(struct metadata) + img_num * sizeof(struct fwu_image_entry);
    if (fwu_md_size > FWU_METADATA_FLASH_SECTOR_SIZE) {
        return FWU_AGENT_ERROR;
    }

    if (DEV_TYPE == FWU_RSS_FLASH_DEV) {
        if (sizeof(struct fwu_private_metadata) > FWU_METADATA_FLASH_SECTOR_SIZE) {
            return FWU_AGENT_ERROR;
        }
    }

    ret = FLASH_DEV->Initialize(NULL);
    if (ret != ARM_DRIVER_OK) {
        return FWU_AGENT_ERROR;
    }

    flash_info = FLASH_DEV->GetInfo();
    if (flash_info->program_unit != 1) {
        FLASH_DEV->Uninitialize();
        return FWU_AGENT_ERROR;
    }

    *is_initialized = 1;

    FWU_LOG_MSG("%s: is_initialized = %d\n\r", __func__, *is_initialized);

    return FWU_AGENT_SUCCESS;
}

enum fwu_agent_error_t metadata_write(enum FWU_METADATA_FLASH_DEV DEV_TYPE,
                                      struct metadata *p_metadata)
{
    int ret;
    uint32_t fwu_md_size;
    ARM_DRIVER_FLASH *FLASH_DEV;
    uint8_t img_num;
    enum fwu_agent_error_t agent_ret;

    FWU_LOG_MSG("%s: enter: flash addr = %u, size = %d\n\r", __func__,
                  FWU_METADATA_REPLICA_1_OFFSET, fwu_md_size);

    if (!p_metadata) {
        return FWU_AGENT_ERROR;
    }

    agent_ret = get_fwu_flash_and_img_num(DEV_TYPE, &FLASH_DEV, &img_num);
    if (agent_ret != FWU_AGENT_SUCCESS) {
        return FWU_AGENT_ERROR;
    }

    fwu_md_size = sizeof(struct metadata) + img_num * sizeof(struct fwu_image_entry);

    ret = FLASH_DEV->EraseSector(FWU_METADATA_REPLICA_1_OFFSET);
    if (ret != ARM_DRIVER_OK) {
        return FWU_AGENT_ERROR;
    }

    ret = FLASH_DEV->ProgramData(FWU_METADATA_REPLICA_1_OFFSET,
                                p_metadata, fwu_md_size);
    if (ret != fwu_md_size) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: enter: flash addr = %u, size = %d\n\r", __func__,
                  FWU_METADATA_REPLICA_2_OFFSET, fwu_md_size);

    ret = FLASH_DEV->EraseSector(FWU_METADATA_REPLICA_2_OFFSET);
    if (ret != ARM_DRIVER_OK) {
        return FWU_AGENT_ERROR;
    }

    ret = FLASH_DEV->ProgramData(FWU_METADATA_REPLICA_2_OFFSET,
                                p_metadata, fwu_md_size);
    if (ret != fwu_md_size) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: enter: flash addr = %u, size = %d\n\r", __func__,
                  FWU_METADATA_REPLICA_2_OFFSET, fwu_md_size);

    FWU_LOG_MSG("%s: success: active = %u, previous = %d\n\r", __func__,
                  p_metadata->active_index, p_metadata->previous_active_index);
    return FWU_AGENT_SUCCESS;
}

void bl1_get_active_bl2_image(uint32_t *offset)
{
    struct fwu_private_metadata priv_metadata;
    enum fwu_agent_state_t current_state;
    uint32_t boot_attempted;
    uint32_t boot_index;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if(fwu_metadata_init(FWU_RSS_FLASH_DEV, &is_initialized_rss)) {
        FWU_ASSERT(0);
    }

    if (private_metadata_read(&priv_metadata)) {
        FWU_ASSERT(0);
    }

    if (metadata_read(FWU_RSS_FLASH_DEV, &fwu_md_rss.md)) {
        FWU_ASSERT(0);
    }

    current_state = get_fwu_agent_state(&fwu_md_rss, &priv_metadata);

    if (current_state == FWU_AGENT_STATE_REGULAR) {
        boot_index = fwu_md_rss.md.active_index;
        FWU_ASSERT(boot_index == priv_metadata.boot_index);
        boot_attempted = 0;
    } else if (current_state == FWU_AGENT_STATE_TRIAL) {
        boot_attempted = (++priv_metadata.boot_attempted);
        FWU_LOG_MSG("%s: attempting boot number = %u\n\r",
                                        __func__, boot_attempted);
        if (boot_attempted <= MAX_BOOT_ATTEMPTS_PER_BANK) {
            boot_index = fwu_md_rss.md.active_index;
            FWU_LOG_MSG("%s: booting from trial bank: %u\n\r",
                                        __func__, boot_index);
        } else if (boot_attempted <= (2 * MAX_BOOT_ATTEMPTS_PER_BANK)) {
            boot_index = fwu_md_rss.md.previous_active_index;
            FWU_LOG_MSG("%s: gave up booting from trial bank\n\r", __func__);
            FWU_LOG_MSG("%s: booting from previous active bank: %u\n\r",
                                        __func__, boot_index);
        } else {
            FWU_LOG_MSG("FWU: cannot boot system from any bank, halting...\n\r");
            FWU_ASSERT(0);
        }
    } else {
        FWU_ASSERT(0);
    }

    priv_metadata.boot_index = boot_index;
    if (private_metadata_write(&priv_metadata) < 0) {
        FWU_ASSERT(0);
    }

    if (boot_index == BANK_0) {
        *offset = BL2_BANK_0_OFFSET;
    } else if (boot_index == BANK_1) {
        *offset = BL2_BANK_1_OFFSET;
    } else {
        FWU_ASSERT(0);
    }

    FWU_LOG_MSG("%s: exit: booting from bank = %u, offset = 0x%x\n\r", __func__,
                        boot_index, *offset);
    return;
}


enum fwu_agent_error_t fwu_metadata_provision_rss(void)
{
    enum fwu_agent_error_t ret;
    struct fwu_private_metadata priv_metadata;
    uint32_t image_version = FWU_IMAGE_INITIAL_VERSION;

    ret = fwu_metadata_init(FWU_RSS_FLASH_DEV, &is_initialized_rss);
    if (ret) {
        return ret;
    }
    /* Check by chance if the previous reboot had a firmware data.
     * If yes, then don't initialize metadata.
     */
    metadata_read(FWU_RSS_FLASH_DEV, &fwu_md_rss.md);

    if(fwu_md_rss.md.active_index < 2 || fwu_md_rss.md.previous_active_index < 2){
        if(fwu_md_rss.md.active_index ^ fwu_md_rss.md.previous_active_index)
            return FWU_AGENT_SUCCESS;
    }

    /* Provision FWU Agent Metadata to RSS flash. */
    memset(&fwu_md_rss, 0, sizeof(struct fwu_metadata_rss));

    fwu_md_rss.md.version = 1;
    fwu_md_rss.md.active_index = BANK_0;
    fwu_md_rss.md.previous_active_index = BANK_1;

    /* Bank-0 is the place where images are located at the
     * start of device lifecycle.
     */
    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_RSS_FW_BANK; i++) {

        fwu_md_rss.img_entry[i].img_props[BANK_0].accepted = IMAGE_ACCEPTED;
        fwu_md_rss.img_entry[i].img_props[BANK_0].version = image_version;

        fwu_md_rss.img_entry[i].img_props[BANK_1].accepted = IMAGE_NOT_ACCEPTED;
        fwu_md_rss.img_entry[i].img_props[BANK_1].version = INVALID_VERSION;
    }

    /* Calculate CRC32 for fwu metadata. */
    fwu_md_rss.md.crc_32 = crc32((uint8_t *)&fwu_md_rss.md.version,
                             sizeof(struct fwu_metadata_rss) - sizeof(uint32_t));

    ret = metadata_write(FWU_RSS_FLASH_DEV, &fwu_md_rss.md);
    if (ret) {
        return ret;
    }

    memset(&fwu_md_rss, 0, sizeof(struct fwu_metadata_rss));
    ret = metadata_read(FWU_RSS_FLASH_DEV, &fwu_md_rss.md);
    if (ret) {
        return ret;
    }
    FWU_LOG_MSG("%s: provisioned values: active = %u, previous = %d\n\r",
             __func__, fwu_md_rss.md.active_index, fwu_md_rss.md.previous_active_index);

    /* Provision Private metadata for update agent which is shared
       beween BL1 and RSS runtime. */
    memset(&priv_metadata, 0, sizeof(struct fwu_private_metadata));

    priv_metadata.boot_index = BANK_0;
    priv_metadata.boot_attempted = 0;

    priv_metadata.fmp_version = FWU_IMAGE_INITIAL_VERSION;
    priv_metadata.fmp_last_attempt_version = FWU_IMAGE_INITIAL_VERSION;
    priv_metadata.fmp_last_attempt_status = LAST_ATTEMPT_STATUS_SUCCESS;

    ret = private_metadata_write(&priv_metadata);
    if (ret) {
        return ret;
    }

    memset(&priv_metadata, 0, sizeof(struct fwu_private_metadata));
    ret = private_metadata_read(&priv_metadata);
    if (ret) {
        return ret;
    }
    FWU_LOG_MSG("%s: provisioned values: boot_index = %u\n\r", __func__,
                        priv_metadata.boot_index);

    FWU_LOG_MSG("%s: FWU METADATA PROVISIONED.\n\r", __func__);
    return FWU_AGENT_SUCCESS;
}
