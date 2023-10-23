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

/* Properties of image in a bank */
struct fwu_image_properties {

        /* UUID of the image in this bank */
        uuid_t img_uuid;

        /* [0]: bit describing the image acceptance status –
         *      1 means the image is accepted
         * [31:1]: MBZ
         */
        uint32_t accepted;

        /* NOTE: using the reserved field */
        /* image version */
        uint32_t version;

} __packed;

/* Image entry information */
struct fwu_image_entry {

        /* UUID identifying the image type */
        uuid_t img_type_uuid;

        /* UUID of the storage volume where the image is located */
        uuid_t location_uuid;

        /* Properties of images with img_type_uuid in the different FW banks */
        struct fwu_image_properties img_props[NR_OF_FW_BANKS];

} __packed;

struct fwu_metadata {

        /* Metadata CRC value */
        uint32_t crc_32;

        /* Metadata version */
        uint32_t version;

        /* Bank index with which device boots */
        uint32_t active_index;

        /* Previous bank index with which device booted successfully */
        uint32_t previous_active_index;

        /* Image entry information */
        struct fwu_image_entry img_entry[NR_OF_IMAGES_IN_FW_BANK];

} __packed;

/* This is Kronos speific metadata for OTA.
 * Private metadata is written at next sector following
 * FWU METADATA location */
struct fwu_private_metadata {

       /* boot_index: The bank from which system is booted from */
       uint32_t boot_index;

       /* counter: Tracking number of boot attempted so far */
       uint32_t boot_attempted;

       /* staged nv_counter: Temprary location before written to the otp */
       uint32_t nv_counter[NR_OF_IMAGES_IN_FW_BANK];

       /* FMP information */
       uint32_t fmp_version;
       uint32_t fmp_last_attempt_version;
       uint32_t fmp_last_attempt_status;

} __packed;

#define MAX_BOOT_ATTEMPTS_PER_BANK 3

struct fwu_metadata _metadata;

int is_initialized = 0;

capsule_image_info_t capsule_info;

enum fwu_agent_state_t {
    FWU_AGENT_STATE_UNKNOWN = -1,
    FWU_AGENT_STATE_REGULAR = 0,
    FWU_AGENT_STATE_TRIAL,
};

struct efi_guid full_capsule_image_guid = {
    .time_low = 0xe2bb9c06,
    .time_mid = 0x70e9,
    .time_hi_and_version = 0x4b14,
    .clock_seq_and_node = {0x97, 0xa3, 0x5a, 0x79, 0x13, 0x17, 0x6e, 0x3f}
};

#define IMAGE_ACCEPTED          (1)
#define IMAGE_NOT_ACCEPTED      (0)
#define BANK_0                  (0)
#define BANK_1                  (1)
#define IMAGE_0                 (0)
#define IMAGE_1                 (1)
#define IMAGE_2                 (2)
#define IMAGE_3                 (3)
#define IMAGE_END               (IMAGE_3)
#define IMAGE_ALL               (IMAGE_END + 1)
#define IMAGE_NOT_RECOGNIZED    (-1)
#define INVALID_VERSION         (0xffffffff)

#ifndef FWU_METADATA_FLASH_DEV
    #ifndef FLASH_DEV_NAME
    #error "FWU_METADATA_FLASH_DEV or FLASH_DEV_NAME must be defined in flash_layout.h"
    #else
    #define FWU_METADATA_FLASH_DEV FLASH_DEV_NAME
    #endif
#endif

/* Import the CMSIS flash device driver */
extern ARM_DRIVER_FLASH FWU_METADATA_FLASH_DEV;

#define HOST_ACK_TIMEOUT_SEC    (6 * 60) /* About 360 seconds, not exact */

static enum fwu_agent_state_t get_fwu_agent_state(
        struct fwu_metadata *metadata_ptr,
        struct fwu_private_metadata *priv_metadata_ptr)
{
    uint32_t boot_index;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    boot_index = priv_metadata_ptr->boot_index;

    if (boot_index != metadata_ptr->active_index) {
        return FWU_AGENT_STATE_TRIAL;
    }

    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_FW_BANK; i++) {
        if ((metadata_ptr->img_entry[i].img_props[boot_index].accepted)
                == (IMAGE_NOT_ACCEPTED)) {
            return FWU_AGENT_STATE_TRIAL;
        }
    }

    FWU_LOG_MSG("%s: exit: FWU_AGENT_STATE_REGULAR\n\r", __func__);
    return FWU_AGENT_STATE_REGULAR;
}

static enum fwu_agent_error_t private_metadata_read(
        struct fwu_private_metadata* p_metadata)
{
    int32_t ret;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if (!p_metadata) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_FLASH_DEV.ReadData(FWU_PRIVATE_METADATA_REPLICA_1_OFFSET, p_metadata,
                                          sizeof(struct fwu_private_metadata));
    if (ret != sizeof(struct fwu_private_metadata)) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: success: boot_index = %u\n\r", __func__,
                        p_metadata->boot_index);

    return FWU_AGENT_SUCCESS;
}

static enum fwu_agent_error_t private_metadata_write(
        struct fwu_private_metadata* p_metadata)
{
    int32_t ret;

    FWU_LOG_MSG("%s: enter: boot_index = %u\n\r", __func__,
                        p_metadata->boot_index);

    if (!p_metadata) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_FLASH_DEV.EraseSector(FWU_PRIVATE_METADATA_REPLICA_1_OFFSET);
    if (ret != ARM_DRIVER_OK) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_FLASH_DEV.ProgramData(FWU_PRIVATE_METADATA_REPLICA_1_OFFSET,
                                p_metadata, sizeof(struct fwu_private_metadata));
    if (ret != sizeof(struct fwu_private_metadata)) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: success\n\r", __func__);
    return FWU_AGENT_SUCCESS;
}

static enum fwu_agent_error_t metadata_write(
                        struct fwu_metadata *p_metadata)
{
    int32_t ret;

    FWU_LOG_MSG("%s: enter: flash addr = %u, size = %d\n\r", __func__,
                  FWU_METADATA_REPLICA_1_OFFSET, sizeof(struct fwu_metadata));

    if (!p_metadata) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_FLASH_DEV.EraseSector(FWU_METADATA_REPLICA_1_OFFSET);
    if (ret != ARM_DRIVER_OK) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_FLASH_DEV.ProgramData(FWU_METADATA_REPLICA_1_OFFSET,
                                p_metadata, sizeof(struct fwu_metadata));
    if (ret != sizeof(struct fwu_metadata)) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: enter: flash addr = %u, size = %d\n\r", __func__,
                  FWU_METADATA_REPLICA_2_OFFSET, sizeof(struct fwu_metadata));

    ret = FWU_METADATA_FLASH_DEV.EraseSector(FWU_METADATA_REPLICA_2_OFFSET);
    if (ret != ARM_DRIVER_OK) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_FLASH_DEV.ProgramData(FWU_METADATA_REPLICA_2_OFFSET,
                                p_metadata, sizeof(struct fwu_metadata));
    if (ret != sizeof(struct fwu_metadata)) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: enter: flash addr = %u, size = %d\n\r", __func__,
                  FWU_METADATA_REPLICA_2_OFFSET, sizeof(struct fwu_metadata));

    ret = FWU_METADATA_FLASH_DEV.EraseSector(FWU_METADATA_REPLICA_2_OFFSET);
    if (ret != ARM_DRIVER_OK) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_FLASH_DEV.ProgramData(FWU_METADATA_REPLICA_2_OFFSET,
                                p_metadata, sizeof(struct fwu_metadata));
    if (ret != sizeof(struct fwu_metadata)) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: success: active = %u, previous = %d\n\r", __func__,
                  p_metadata->active_index, p_metadata->previous_active_index);
    return FWU_AGENT_SUCCESS;
}

static enum fwu_agent_error_t metadata_validate(struct fwu_metadata *p_metadata)
{
    int32_t ret;
    uint32_t calculated_crc32;

    FWU_LOG_MSG("%s: enter:\n\r", __func__);

    if (!p_metadata) {
        return FWU_AGENT_ERROR;
    }

    calculated_crc32 = crc32((uint8_t *)&(p_metadata->version),
                             sizeof(struct fwu_metadata) - sizeof(uint32_t));

    if (p_metadata->crc_32 != calculated_crc32) {
        FWU_LOG_MSG("%s: failed: crc32 calculated: 0x%x, given: 0x%x\n\r", __func__,
                    calculated_crc32, p_metadata->crc_32);
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: success\n\r", __func__);

    return FWU_AGENT_SUCCESS;
}

static enum fwu_agent_error_t metadata_read(struct fwu_metadata *p_metadata)
{
    int32_t ret;

    FWU_LOG_MSG("%s: enter: flash addr = %u, size = %d\n\r", __func__,
                  FWU_METADATA_REPLICA_1_OFFSET, sizeof(struct fwu_metadata));

    if (!p_metadata) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_FLASH_DEV.ReadData(FWU_METADATA_REPLICA_1_OFFSET,
                                p_metadata, sizeof(struct fwu_metadata));
    if (ret != sizeof(struct fwu_metadata)) {
        return FWU_AGENT_ERROR;
    }

    if (metadata_validate(p_metadata) != FWU_AGENT_SUCCESS) {
        return FWU_AGENT_ERROR;
    }

    FWU_LOG_MSG("%s: success: active = %u, previous = %d\n\r", __func__,
                  p_metadata->active_index, p_metadata->previous_active_index);

    return FWU_AGENT_SUCCESS;
}

enum fwu_agent_error_t fwu_metadata_init(void)
{
    int32_t ret;
    ARM_FLASH_INFO* flash_info;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if (is_initialized) {
        return FWU_AGENT_SUCCESS;
    }

    /* Code assumes everything fits into a sector */
    if (sizeof(struct fwu_metadata) > FWU_METADATA_FLASH_SECTOR_SIZE) {
        return FWU_AGENT_ERROR;
    }

    if (sizeof(struct fwu_private_metadata) > FWU_METADATA_FLASH_SECTOR_SIZE) {
        return FWU_AGENT_ERROR;
    }

    ret = FWU_METADATA_FLASH_DEV.Initialize(NULL);
    if (ret != ARM_DRIVER_OK) {
        return FWU_AGENT_ERROR;
    }

    flash_info = FWU_METADATA_FLASH_DEV.GetInfo();
    if (flash_info->program_unit != 1) {
        FWU_METADATA_FLASH_DEV.Uninitialize();
        return FWU_AGENT_ERROR;
    }

    is_initialized = 1;

    FWU_LOG_MSG("%s: is_initialized = %d\n\r", __func__, is_initialized);

    return FWU_AGENT_SUCCESS;
}

void bl1_get_active_bl2_image(uint32_t *offset)
{
    struct fwu_private_metadata priv_metadata;
    enum fwu_agent_state_t current_state;
    uint32_t boot_attempted;
    uint32_t boot_index;

    FWU_LOG_MSG("%s: enter\n\r", __func__);

    if (fwu_metadata_init()) {
        FWU_ASSERT(0);
    }

    if (private_metadata_read(&priv_metadata)) {
        FWU_ASSERT(0);
    }

    if (metadata_read(&_metadata)) {
        FWU_ASSERT(0);
    }

    current_state = get_fwu_agent_state(&_metadata, &priv_metadata);

    if (current_state == FWU_AGENT_STATE_REGULAR) {
        boot_index = _metadata.active_index;
        FWU_ASSERT(boot_index == priv_metadata.boot_index);
        boot_attempted = 0;
    } else if (current_state == FWU_AGENT_STATE_TRIAL) {
        boot_attempted = (++priv_metadata.boot_attempted);
        FWU_LOG_MSG("%s: attempting boot number = %u\n\r",
                                        __func__, boot_attempted);
        if (boot_attempted <= MAX_BOOT_ATTEMPTS_PER_BANK) {
            boot_index = _metadata.active_index;
            FWU_LOG_MSG("%s: booting from trial bank: %u\n\r",
                                        __func__, boot_index);
        } else if (boot_attempted <= (2 * MAX_BOOT_ATTEMPTS_PER_BANK)) {
            boot_index = _metadata.previous_active_index;
            FWU_LOG_MSG("%s: gave up booting from trial bank\n\r", __func__);
            FWU_LOG_MSG("%s: booting from previous active bank: %u\n\r",
                                        __func__, boot_index);
        } else {
            FWU_LOG_MSG("%s: cannot boot system from any bank, halting...\n\r", __func__);
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

enum fwu_agent_error_t fwu_metadata_provision(void)
{
    enum fwu_agent_error_t ret;
    struct fwu_private_metadata priv_metadata;
    uint8_t image_version = FWU_IMAGE_INITIAL_VERSION;

    ret = fwu_metadata_init();
    if (ret) {
        return ret;
    }
    /*
     * Check by chance if the previous reboot had a firmware data.
     * If yes, then don't initialize metadata.
     */
    metadata_read(&_metadata);
    if(_metadata.active_index < 2 || _metadata.previous_active_index <2){
        if(_metadata.active_index ^ _metadata.previous_active_index)
            return FWU_AGENT_SUCCESS;
    }
    /* Provision FWU Agent Metadata */
    memset(&_metadata, 0, sizeof(struct fwu_metadata));

    _metadata.version = 1;
    _metadata.active_index = BANK_0;
    _metadata.previous_active_index = BANK_1;

    /* Bank-0 is the place where images are located at the
     * start of device lifecycle.
     */
    for (uint8_t i = 0; i < NR_OF_IMAGES_IN_FW_BANK; i++) {

        _metadata.img_entry[i].img_props[BANK_0].accepted = IMAGE_ACCEPTED;
        _metadata.img_entry[i].img_props[BANK_0].version = image_version;

        _metadata.img_entry[i].img_props[BANK_1].accepted = IMAGE_NOT_ACCEPTED;
        _metadata.img_entry[i].img_props[BANK_1].version = INVALID_VERSION;
    }

    /* Calculate CRC32 for fwu metadata */
    _metadata.crc_32 = crc32((uint8_t *)&_metadata.version,
                             sizeof(struct fwu_metadata) - sizeof(uint32_t));

    ret = metadata_write(&_metadata);
    if (ret) {
        return ret;
    }

    memset(&_metadata, 0, sizeof(struct fwu_metadata));
    ret = metadata_read(&_metadata);
    if (ret) {
        return ret;
    }
    FWU_LOG_MSG("%s: provisioned values: active = %u, previous = %d\n\r",
             __func__, _metadata.active_index, _metadata.previous_active_index);

    /* Provision Private metadata for update agent which is shared
       beween BL1 and RSS runtime */
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
