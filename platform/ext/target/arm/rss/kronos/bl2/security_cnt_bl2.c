/*
 * Copyright (c) 2019-2021,2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "bootutil/security_cnt.h"
#include "tfm_plat_nv_counters.h"
#include "tfm_plat_defs.h"
#include "bootutil/fault_injection_hardening.h"
#include <stdint.h>
#include "tfm_plat_provisioning.h"
#include "fwu_agent.h"
#include "bootutil/security_cnt.h"
#include "bootutil/fault_injection_hardening.h"
#include "bootutil/bootutil.h"
#include "bootutil/bootutil_log.h"
#include <stdint.h>

#define TFM_BOOT_NV_COUNTER_FIRST    PLAT_NV_COUNTER_BL2_0
#define TFM_BOOT_NV_COUNTER_MAX      TFM_BOOT_NV_COUNTER_FIRST + \
                                        MCUBOOT_IMAGE_NUMBER

static enum tfm_nv_counter_t get_nv_counter_from_image_id(uint32_t image_id)
{
    uint32_t nv_counter;

    /* Avoid integer overflow */
    if ((UINT32_MAX - TFM_BOOT_NV_COUNTER_FIRST) < image_id) {
        return TFM_BOOT_NV_COUNTER_MAX;
    }

    nv_counter = TFM_BOOT_NV_COUNTER_FIRST + image_id;

    /* Check the existence of the enumerated counter value */
    if (nv_counter >= TFM_BOOT_NV_COUNTER_MAX) {
        return TFM_BOOT_NV_COUNTER_MAX;
    }

    return (enum tfm_nv_counter_t)nv_counter;
}

fih_ret boot_nv_security_counter_init(void)
{
    FIH_DECLARE(fih_rc, FIH_FAILURE);

    fih_rc = fih_ret_encode_zero_equality(tfm_plat_init_nv_counter());

    FIH_RET(fih_rc);
}

fih_ret boot_nv_security_counter_get(uint32_t image_id, fih_int *security_cnt)
{
    enum tfm_nv_counter_t nv_counter;
    FIH_DECLARE(fih_rc, FIH_FAILURE);
    uint32_t security_cnt_soft;

    /* Check if it's a null-pointer. */
    if (!security_cnt) {
        FIH_RET(FIH_FAILURE);
    }

    nv_counter = get_nv_counter_from_image_id(image_id);
    if (nv_counter >= TFM_BOOT_NV_COUNTER_MAX) {
        FIH_RET(FIH_FAILURE);
    }

    fih_rc = fih_ret_encode_zero_equality(
             tfm_plat_read_nv_counter(nv_counter,
                                      sizeof(security_cnt_soft),
                                      (uint8_t *)&security_cnt_soft));
    *security_cnt = fih_int_encode(security_cnt_soft);

    FIH_RET(fih_rc);
}

int32_t boot_nv_security_counter_update(uint32_t image_id,
                                        uint32_t img_security_cnt)
{
    enum tfm_nv_counter_t nv_counter;
    enum tfm_plat_err_t err;
    enum fwu_agent_error_t fwu_err;
    enum fwu_nv_counter_index_t tfm_nv_counter_i;

    nv_counter = get_nv_counter_from_image_id(image_id);
    if (nv_counter >= TFM_BOOT_NV_COUNTER_MAX) {
        return -1;
    }

    if (tfm_plat_provisioning_is_required()) {
        err = tfm_plat_set_nv_counter(nv_counter, img_security_cnt);
        if (err != TFM_PLAT_ERR_SUCCESS) {
            return -1;
        }

    } else {
        switch (nv_counter) {
        case PLAT_NV_COUNTER_BL2_0:
            tfm_nv_counter_i = FWU_BL2_NV_COUNTER;
            break;
        case PLAT_NV_COUNTER_BL2_1:
            tfm_nv_counter_i = FWU_TFM_NV_COUNTER;
            break;
        case PLAT_NV_COUNTER_BL2_2:
            tfm_nv_counter_i = FWU_FIP_NV_COUNTER;
            break;
        case PLAT_NV_COUNTER_BL2_3:
            tfm_nv_counter_i = FWU_SI_CL2_NV_COUNTER;
            break;
        case PLAT_NV_COUNTER_BL2_4:
            tfm_nv_counter_i = FWU_SI_CL1_NV_COUNTER;
            break;
        case PLAT_NV_COUNTER_BL2_5:
            tfm_nv_counter_i = FWU_SI_CL0_NV_COUNTER;
            break;
        case PLAT_NV_COUNTER_BL2_6:
            tfm_nv_counter_i = FWU_LCP_NV_COUNTER;
            break;
        case PLAT_NV_COUNTER_BL2_7:
            tfm_nv_counter_i = FWU_SCP_NV_COUNTER;
            break;
        default:
            return -1;
        }

        fwu_err = fwu_stage_nv_counter(tfm_nv_counter_i, img_security_cnt);
        if (fwu_err != FWU_AGENT_SUCCESS) {
            return -1;
        }
    }

    return 0;
}
