/*
 * Copyright (c) 2020-2021,2024 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __TFM_HAL_INTERRUPT_H__
#define __TFM_HAL_INTERRUPT_H__

#include <stdint.h>
#include "tfm_hal_defs.h"

#ifdef MHU_V3_RSS_TO_SCP
enum mhu_scp_rss_doorbell_channel {
    MHU_SCP_RSS_ATU_REQUEST_CHANNEL_ID=0,
    MHU_SCP_RSS_SYSTOP_ON_CHANNEL_ID,
    MHU_SCP_RSS_SYS_RESET_CHANNEL_ID,
    MHU_SCP_RSS_SI_CL0_CHANNEL_ID,
    MHU_SCP_RSS_SI_CL1_CHANNEL_ID,
    MHU_SCP_RSS_SI_CL2_CHANNEL_ID,
    MHU_SCP_RSS_SYS_SHUTDOWN_CHANNEL_ID,
    MHU_SCP_RSS_CHANNEL_COUNT,
};
#endif

/**
 * \brief  Enables an interrupt from the Interrupt Controller of the platform
 *
 * \param[in]   irq_num   the interrupt to be enabled with a number
 *
 * \return  TFM_HAL_ERROR_INVALID_INPUT - the irq_num is not invalid.
 *          TFM_HAL_ERROR_GENERIC - failed to enable the interrupt.
 *          TFM_HAL_SUCCESS - the interrupt is enabled.
 */
enum tfm_hal_status_t tfm_hal_irq_enable(uint32_t irq_num);

/**
 * \brief  Disables an interrupt from the Interrupt Controller of the platform
 *
 * \param[in]   irq_num   the interrupt to be disabled with a number
 *
 * \return  TFM_HAL_ERROR_INVALID_INPUT - the irq_num is not invalid.
 *          TFM_HAL_ERROR_GENERIC - failed to disable the interrupt.
 *          TFM_HAL_SUCCESS - the interrupt is disable.
 */
enum tfm_hal_status_t tfm_hal_irq_disable(uint32_t irq_num);

/**
 * \brief  Clears an active and pending interrupt.
 *
 * \param[in] irq_num   the interrupt to be cleared with a number
 *
 * \return  TFM_HAL_ERROR_INVALID_INPUT - the irq_num is not invalid.
 *          TFM_HAL_ERROR_GENERIC - failed to clear the pending interrupt.
 *          TFM_HAL_SUCCESS - the pending interrupt is cleared.
 */
enum tfm_hal_status_t tfm_hal_irq_clear_pending(uint32_t irq_num);

#endif /* __TFM_HAL_INTERRUPT_H__ */
