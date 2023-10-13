/*
 * Copyright (c) 2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdint.h>
#include "platform_base_address.h"
#include "host_base_address.h"
#include "Driver_Flash_Strata.h"
#include "cmsis_driver_config.h"
#include "RTE_Device.h"

#ifndef ARG_UNUSED
#define ARG_UNUSED(arg)  ((void)arg)
#endif

#define FLASH0_BASE_S         BOOT_FLASH
#define FLASH0_SIZE           BOOT_FLASH_SIZE
#define FLASH0_SECTOR_SIZE    0x00001000 /* 4 kB */
#define FLASH0_PAGE_SIZE      0x00001000 /* 4 kB */
#define FLASH0_PROGRAM_UNIT   0x1        /* Minimum write size */

#if (RTE_FLASH0)
static ARM_FLASH_INFO ARM_FLASH0_DEV_DATA = {
    .sector_info    = NULL,     /* Uniform sector layout */
    .sector_count   = HOST_ACCESS_SIZE / 0x1000,
    .sector_size    = 0x1000,
    .page_size      = 256U,
    .program_unit   = 1U,
    .erased_value   = ARM_FLASH_DRV_ERASE_VALUE
};

static struct arm_strata_flash_dev_t ARM_FLASH0_DEV = {
    .dev    = &FLASH0_DEV,
    .data   = &ARM_FLASH0_DEV_DATA
};

ARM_FLASH_STRATA(ARM_FLASH0_DEV, Driver_FLASH0);

#if (defined (SPI_STRATAFLASHJ3_S) && defined (CFI_S))
static const struct cfi_dev_cfg_t CFI_DEV_CFG_S = {
    /* Define the flash base/size to be the same as the host access area, as the
     * flash may not be mapped contiguously or predictably within that area.
     */
    .base = BOOT_FLASH,
};

struct cfi_dev_t CFI_DEV_S = {
    .cfg = &CFI_DEV_CFG_S,
};

struct cfi_strataflashj3_dev_t SPI_STRATAFLASHJ3_DEV = {
    .controller = &CFI_DEV_S,
    .total_sector_cnt = 0,
    .page_size = 0,
    .sector_size = 0,
    .program_unit = 1U,
    .is_initialized = false,
};
#endif /* (SPI_STRATAFLASHJ3_S) && (CFI_S) */

#endif /* RTE_FLASH0 */

#if (RTE_FLASH1)
static ARM_FLASH_INFO ARM_FLASH1_DEV_DATA = {
    .sector_info    = NULL,     /* Uniform sector layout */
    .sector_count   = AP_FLASH_SIZE / 0x1000,
    .sector_size    = 0x1000,
    .page_size      = 256U,
    .program_unit   = 1U,
    .erased_value   = ARM_FLASH_DRV_ERASE_VALUE
};

static struct arm_strata_flash_dev_t ARM_FLASH1_DEV = {
    .dev    = &FLASH1_DEV,
    .data   = &ARM_FLASH1_DEV_DATA
};

ARM_FLASH_STRATA(ARM_FLASH1_DEV, Driver_FLASH1);

#if (defined (SPI_STRATAFLASHJ3_S) && defined (CFI_S))
static const struct cfi_dev_cfg_t CFI_DEV_CFG_S_AP = {
    /* Define the flash base/size to be the same as the host access area, as the
     * flash may not be mapped contiguously or predictably within that area.
     */
    .base = AP_FLASH_LOG_BASE,
};

struct cfi_dev_t CFI_DEV_S_AP = {
    .cfg = &CFI_DEV_CFG_S_AP,
};

struct cfi_strataflashj3_dev_t SPI_STRATAFLASHJ3_AP_DEV = {
    .controller = &CFI_DEV_S_AP,
    .total_sector_cnt = 0,
    .page_size = 0,
    .sector_size = 0,
    .program_unit = 1U,
    .is_initialized = false,
};
#endif /* (SPI_STRATAFLASHJ3_S) && (CFI_S) */
#endif