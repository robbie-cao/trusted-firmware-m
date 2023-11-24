/*
 * Copyright (c) 2021-2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "cmsis.h"
#include "target_cfg.h"
#include "tfm_hal_platform.h"
#include "tfm_peripherals_def.h"
#include "uart_stdout.h"
#include "device_definition.h"
#ifdef TFM_FWU_AGENT
#include "Driver_Flash.h"
#include "flash_layout.h"
#include "fwu_agent.h"
#endif /* TFM_FWU_AGENT */

#if defined(TFM_PARTITION_PROTECTED_STORAGE) || \
    defined(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE)
#include "host_base_address.h"
#include "size_defs.h"
#endif /* TFM_PARTITION_PROTECTED_STORAGE or
        * TFM_PARTITION_INTERNAL_TRUSTED_STORAGE  */

#ifdef TFM_PARTITION_PROTECTED_STORAGE
#define  RSS_ATU_REGION_PS_SLOT  16
#endif /* TFM_PARTITION_PROTECTED_STORAGE */

#ifdef TFM_PARTITION_INTERNAL_TRUSTED_STORAGE
#define RSS_ATU_REGION_ITS_SLOT  17
#endif /* TFM_PARTITION_INTERNAL_TRUSTED_STORAGE */


extern const struct memory_region_limits memory_regions;

#ifdef TFM_FWU_AGENT
extern ARM_DRIVER_FLASH FWU_METADATA_AP_FLASH_DEV;
extern ARM_DRIVER_FLASH FWU_METADATA_RSS_FLASH_DEV;
extern int is_initialized_rss;
extern int is_initialized_ap;
#endif /* TFM_FWU_AGENT */

enum tfm_hal_status_t tfm_hal_platform_init(void)
{
    enum tfm_plat_err_t plat_err = TFM_PLAT_ERR_SYSTEM_ERR;
#ifdef TFM_FWU_AGENT
    enum atu_error_t atu_err;
    enum fwu_agent_error_t ret;
#endif /* TFM_FWU_AGENT */

    plat_err = enable_fault_handlers();
    if (plat_err != TFM_PLAT_ERR_SUCCESS) {
        return TFM_HAL_ERROR_GENERIC;
    }

    plat_err = system_reset_cfg();
    if (plat_err != TFM_PLAT_ERR_SUCCESS) {
        return TFM_HAL_ERROR_GENERIC;
    }

    plat_err = init_debug();
    if (plat_err != TFM_PLAT_ERR_SUCCESS) {
        return TFM_HAL_ERROR_GENERIC;
    }

    __enable_irq();
    stdio_init();

    plat_err = nvic_interrupt_target_state_cfg();
    if (plat_err != TFM_PLAT_ERR_SUCCESS) {
        return TFM_HAL_ERROR_GENERIC;
    }

    plat_err = nvic_interrupt_enable();
    if (plat_err != TFM_PLAT_ERR_SUCCESS) {
        return TFM_HAL_ERROR_GENERIC;
    }

    plat_err = dma_init_cfg();
    if (plat_err != TFM_PLAT_ERR_SUCCESS) {
        return TFM_HAL_ERROR_GENERIC;
    }

#ifdef TFM_FWU_AGENT
    /* Initialize the ATU region for capsule image between AP and RSS. */
    atu_err = atu_initialize_region(&ATU_DEV_S, RSS_ATU_CAPSULE_ID,
            HOST_CAPSULE_DRAM_LOG_BASE, HOST_CAPSULE_DRAM_PHY_BASE,
            HOST_CAPSULE_DRAM_SIZE);
    if (atu_err != ATU_ERR_NONE) {
        return TFM_HAL_ERROR_GENERIC;
    }

    ret = fwu_metadata_init(FWU_RSS_FLASH_DEV, &is_initialized_rss);
    if (ret) {
        return TFM_HAL_ERROR_GENERIC;
    }

    ret = fwu_metadata_init(FWU_AP_FLASH_DEV, &is_initialized_ap);
    if (ret) {
        return TFM_HAL_ERROR_GENERIC;
    }
#endif /* TFM_FWU_AGENT */
    return TFM_HAL_SUCCESS;
}

uint32_t tfm_hal_get_ns_VTOR(void)
{
    return memory_regions.non_secure_code_start;
}

uint32_t tfm_hal_get_ns_MSP(void)
{
    return *((uint32_t *)memory_regions.non_secure_code_start);
}

uint32_t tfm_hal_get_ns_entry_point(void)
{
    return *((uint32_t *)(memory_regions.non_secure_code_start + 4));
}
