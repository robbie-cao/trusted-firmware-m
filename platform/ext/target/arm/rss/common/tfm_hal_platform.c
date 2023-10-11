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

enum tfm_hal_status_t tfm_hal_platform_init(void)
{
    enum tfm_plat_err_t plat_err = TFM_PLAT_ERR_SYSTEM_ERR;
#if defined(TFM_PARTITION_PROTECTED_STORAGE) || \
    defined(TFM_PARTITION_INTERNAL_TRUSTED_STORAGE)
    enum atu_error_t atu_err;
#endif /* TFM_PARTITION_PROTECTED_STORAGE or
        * TFM_PARTITION_INTERNAL_TRUSTED_STORAGE  */

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
