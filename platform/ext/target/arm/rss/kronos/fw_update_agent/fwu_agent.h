/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef FWU_AGENT_H
#define FWU_AGENT_H

#include "Driver_Flash.h"
#include "uefi_capsule_parser.h"

#define ENABLE_FWU_AGENT_DEBUG_LOGS
#ifdef ENABLE_FWU_AGENT_DEBUG_LOGS
    #include <stdio.h>
    #define FWU_LOG_MSG(f_, ...) printf((f_), ##__VA_ARGS__)
#else
    #define FWU_LOG_MSG(f_, ...)
#endif

#define FWU_ASSERT(_c_)                                                        \
                if (!(_c_)) {                                                  \
                    FWU_LOG_MSG("%s:%d assert hit\n\r", __func__, __LINE__);   \
                    while(1) {};                                               \
                }                                                              \

/* Version used for the very first image of the device. */
#define FWU_IMAGE_INITIAL_VERSION 0
#define IMAGE_ACCEPTED          (1)
#define IMAGE_NOT_ACCEPTED      (0)
#define BANK_0                  (0)
#define BANK_1                  (1)
#define IMAGE_0                 (0)
#define IMAGE_1                 (1)
#define IMAGE_2                 (2)
#define IMAGE_3                 (3)
#define IMAGE_4                 (4)
#define IMAGE_5                 (5)
#define IMAGE_6                 (6)
#define IMAGE_7                 (7)
#define IMAGE_END               (IMAGE_7)
#define IMAGE_ALL               (IMAGE_END + 1)
#define IMAGE_NOT_RECOGNIZED    (-1)
#define INVALID_VERSION         (0xffffffff)
#define MAX_BOOT_ATTEMPTS_PER_BANK 3

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

struct metadata {

        /* Metadata CRC value */
        uint32_t crc_32;

        /* Metadata version */
        uint32_t version;

        /* Bank index with which device boots */
        uint32_t active_index;

        /* Previous bank index with which device booted successfully */
        uint32_t previous_active_index;

} __packed;

/* This is Kronos speific metadata for OTA.
 * Private metadata is written at next sector following
 * FWU METADATA location */
struct fwu_private_metadata {

       /* boot_index: the bank from which system is booted from */
       uint32_t boot_index;

       /* counter: tracking number of boot attempted so far */
       uint32_t boot_attempted;

       /* staged nv_counter: temprary location before written to the otp */
       uint32_t nv_counter[NR_OF_IMAGES_IN_RSS_FW_BANK];

       /* FMP information */
       uint32_t fmp_version;
       uint32_t fmp_last_attempt_version;
       uint32_t fmp_last_attempt_status;

} __packed;

struct fwu_metadata_rss {

        struct metadata md;

        /* Image entry information */
        struct fwu_image_entry img_entry[NR_OF_IMAGES_IN_RSS_FW_BANK];

} __packed;

struct fwu_metadata_ap {

        struct metadata md;

        /* Image entry information */
        struct fwu_image_entry img_entry[NR_OF_IMAGES_IN_AP_FW_BANK];

} __packed;
enum fwu_agent_error_t {
        FWU_AGENT_SUCCESS = 0,
        FWU_AGENT_ERROR = (-1)
};

enum FWU_METADATA_FLASH_DEV {
        FWU_RSS_FLASH_DEV = 0,
        FWU_AP_FLASH_DEV
};

enum fwu_agent_state_t {
    FWU_AGENT_STATE_UNKNOWN = -1,
    FWU_AGENT_STATE_REGULAR = 0,
    FWU_AGENT_STATE_TRIAL,
};

enum fwu_agent_error_t fwu_metadata_provision_rss(void);
enum fwu_agent_error_t metadata_read(enum FWU_METADATA_FLASH_DEV DEV_TYPE,
                                     struct metadata *p_metadata);
enum fwu_agent_error_t metadata_write(enum FWU_METADATA_FLASH_DEV DEV_TYPE,
                                      struct metadata *p_metadata);
enum fwu_agent_error_t fwu_metadata_init(enum FWU_METADATA_FLASH_DEV DEV_TYPE,
                                         int *is_initialized);
/* Host to RSS:
 * Host responds with this api to acknowledge its successful boot.
 */
enum fwu_agent_error_t fwu_host_ack(void);
void bl1_get_active_bl2_image(uint32_t *bank_offset);
uint8_t bl2_get_boot_bank(void);
enum fwu_agent_state_t get_fwu_agent_state(
        struct fwu_metadata_rss *fwu_md_rss_ptr,
        struct fwu_private_metadata *priv_metadata_ptr);

/* When in trial state, start the timer for host to respond.
 * Diable timer when host responds back either by calling
 * fwu_accept_image or fwu_select_previous.
 * Otherwise, resets the system.
 */
void host_acknowledgement_timer_to_reset(void);

enum fwu_nv_counter_index_t {
    FWU_BL2_NV_COUNTER = 0,
    FWU_TFM_NV_COUNTER,
    FWU_FIP_NV_COUNTER,
    FWU_SI_CL2_NV_COUNTER,
    FWU_SI_CL1_NV_COUNTER,
    FWU_SI_CL0_NV_COUNTER,
    FWU_LCP_NV_COUNTER,
    FWU_SCP_NV_COUNTER,
    FWU_MAX_NV_COUNTER_INDEX = FWU_SCP_NV_COUNTER
};

/* Stage nv counter into private metadata section of the flash.
 * Staged nv counters are written to the otp when firmware update is successful.
 * The function assumes that the api is called in the boot loading stage.
 */
enum fwu_agent_error_t fwu_stage_nv_counter(enum fwu_nv_counter_index_t index,
        uint32_t img_security_cnt);
#endif /* FWU_AGENT_H */
