/*
 * Copyright (c) 2018-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "tfm_platform_system.h"
#include "cmsis.h"
#ifdef TFM_FWU_AGENT
#include "ioctl_requests.h"
#include "fwu_agent.h"
#include "uefi_fmp.h"
#include <tfm_hal_interrupt.h>
#endif /* TFM_FWU_AGENT */

void tfm_platform_hal_system_reset(void)
{
#ifdef TFM_FWU_AGENT
    /* Reset the system */
    tfm_hal_system_reset();
#else
    /* Reset the system */
    NVIC_SystemReset();
#endif /* TFM_FWU_AGENT */
}

enum tfm_platform_err_t tfm_platform_hal_ioctl(tfm_platform_ioctl_req_t request,
                                               psa_invec  *in_vec,
                                               psa_outvec *out_vec)
{
#ifdef TFM_FWU_AGENT
    enum tfm_platform_err_t ret = TFM_PLATFORM_ERR_SUCCESS;
    enum fwu_agent_error_t result;

    switch(request) {
    case IOCTL_FWU_FLASH_IMAGES:
        result = fwu_flash_image();
        if (result == FWU_AGENT_SUCCESS) {
            tfm_platform_hal_system_reset();
        } else {
            ret = TFM_PLATFORM_ERR_SYSTEM_ERROR;
        }
        break;

    case IOCTL_FWU_HOST_ACK:
        break;

    case IOCTL_FMP_GET_IMAGE_INFO:
        break;

    default:
        ret = TFM_PLATFORM_ERR_NOT_SUPPORTED;
        break;
    }

    return ret;
#else
    (void)request;
    (void)in_vec;
    (void)out_vec;

    /* Not needed for this platform */
    return TFM_PLATFORM_ERR_NOT_SUPPORTED;
#endif /* TFM_FWU_AGENT */
}
