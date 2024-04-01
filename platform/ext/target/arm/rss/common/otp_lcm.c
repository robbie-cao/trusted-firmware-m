/*
 * Copyright (c) 2021-2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "tfm_plat_otp.h"

#include "region_defs.h"
#include "cmsis_compiler.h"
#include "device_definition.h"
#include "lcm_drv.h"
#include "cmsis.h"
#include "uart_stdout.h"
#include "tfm_hal_platform.h"
#include "rss_memory_sizes.h"

#define OTP_OFFSET(x)       (offsetof(struct lcm_otp_layout_t, x))
#define OTP_SIZE(x)         (sizeof(((struct lcm_otp_layout_t *)0)->x))
#define USER_AREA_OFFSET(x) (OTP_OFFSET(user_data) + \
                             offsetof(struct plat_user_area_layout_t, x))
#define USER_AREA_SIZE(x)   (sizeof(((struct plat_user_area_layout_t *)0)->x))

__PACKED_STRUCT plat_user_area_layout_t {
    __PACKED_UNION {
        __PACKED_STRUCT {
            uint32_t cm_locked_size;
            uint32_t cm_locked_size_zero_count;
            uint32_t cm_zero_count;

            uint32_t dm_locked_size;
            uint32_t dm_locked_size_zero_count;
            uint32_t dm_zero_count;

            __PACKED_STRUCT {
                uint32_t bl1_2_image_len;

            /* Things after this point are not touched by BL1_1, and hence are
             * modifiable by new provisioning code.
             */
                uint32_t cca_system_properties;
                uint32_t rss_id;
            } cm_locked;

            __PACKED_STRUCT {
                uint32_t bl1_rotpk_0[14];
                uint32_t bl2_encryption_key[8];

                uint32_t bl2_rotpk[MCUBOOT_IMAGE_NUMBER][8];
                uint32_t s_image_encryption_key[8];
                uint32_t ns_image_encryption_key[8];

                uint32_t iak_len;
                uint32_t iak_type;
                uint32_t iak_id[8];
                uint32_t implementation_id[8];
                uint32_t verification_service_url[8];
                uint32_t profile_definition[8];

                uint32_t secure_debug_pk[8];

                uint32_t host_rotpk_s[24];
                uint32_t host_rotpk_ns[24];
                uint32_t host_rotpk_cca[24];
            } dm_locked;

            __PACKED_STRUCT {
                uint32_t bl1_nv_counter[16];
                uint32_t bl2_nv_counter[MCUBOOT_IMAGE_NUMBER][16];
#ifdef PLATFORM_HAS_PS_NV_OTP_COUNTERS
                uint32_t ps_nv_counter[3][16];
#endif /* PLATFORM_HAS_PS_NV_OTP_COUNTERS */
                uint32_t host_nv_counter[3][16];
                uint32_t reprovisioning_bits;
            } unlocked_area;
        };
        uint8_t _pad[OTP_TOTAL_SIZE - OTP_DMA_ICS_SIZE - BL1_2_CODE_SIZE -
                     sizeof(struct lcm_otp_layout_t)];
    };

    /* These two are aligned to the end of the OTP. The size of the DMA ICS is
     * defined by the hardware, and the ROM knows the size of the bl1_2_image
     * because of the size field, so it's possible to shrink the bootloader (and
     * use the extra space for CM, DM, or unlocked data) without changing the
     * ROM. Placing the image here means that it doesn't get zero-count checked
     * with the rest of the CM data, since it's far faster to just calculate the
     * hash using the CC DMA.
     */
    uint32_t bl1_2_image[BL1_2_CODE_SIZE / sizeof(uint32_t)];
    uint32_t dma_initial_command_sequence[OTP_DMA_ICS_SIZE / sizeof(uint32_t)];
};

static const uint16_t otp_offsets[PLAT_OTP_ID_MAX] = {
    OTP_OFFSET(huk),
    OTP_OFFSET(guk),
    0,
    USER_AREA_OFFSET(dm_locked.iak_len),
    USER_AREA_OFFSET(dm_locked.iak_type),
    USER_AREA_OFFSET(dm_locked.iak_id),

    USER_AREA_OFFSET(dm_locked.implementation_id),
    USER_AREA_OFFSET(dm_locked.verification_service_url),
    USER_AREA_OFFSET(dm_locked.profile_definition),

    USER_AREA_OFFSET(dm_locked.bl2_rotpk[0]),
    USER_AREA_OFFSET(dm_locked.bl2_rotpk[1]),
    USER_AREA_OFFSET(dm_locked.bl2_rotpk[2]),
    USER_AREA_OFFSET(dm_locked.bl2_rotpk[3]),
    USER_AREA_OFFSET(dm_locked.bl2_rotpk[4]),
    USER_AREA_OFFSET(dm_locked.bl2_rotpk[5]),
    USER_AREA_OFFSET(dm_locked.bl2_rotpk[6]),
    USER_AREA_OFFSET(dm_locked.bl2_rotpk[7]),
    USER_AREA_OFFSET(dm_locked.bl2_rotpk[8]),

    USER_AREA_OFFSET(unlocked_area.bl2_nv_counter[0]),
    USER_AREA_OFFSET(unlocked_area.bl2_nv_counter[1]),
    USER_AREA_OFFSET(unlocked_area.bl2_nv_counter[2]),
    USER_AREA_OFFSET(unlocked_area.bl2_nv_counter[3]),
    USER_AREA_OFFSET(unlocked_area.bl2_nv_counter[4]),
    USER_AREA_OFFSET(unlocked_area.bl2_nv_counter[5]),
    USER_AREA_OFFSET(unlocked_area.bl2_nv_counter[6]),
    USER_AREA_OFFSET(unlocked_area.bl2_nv_counter[7]),
    USER_AREA_OFFSET(unlocked_area.bl2_nv_counter[8]),

#ifdef PLATFORM_HAS_PS_NV_OTP_COUNTERS
    USER_AREA_OFFSET(unlocked_area.ps_nv_counter[0]),
    USER_AREA_OFFSET(unlocked_area.ps_nv_counter[1]),
    USER_AREA_OFFSET(unlocked_area.ps_nv_counter[2]),
#endif /* PLATFORM_HAS_PS_NV_OTP_COUNTERS */

    USER_AREA_OFFSET(unlocked_area.host_nv_counter[0]),
    USER_AREA_OFFSET(unlocked_area.host_nv_counter[1]),
    USER_AREA_OFFSET(unlocked_area.host_nv_counter[2]),

    USER_AREA_OFFSET(dm_locked.bl2_encryption_key),
    USER_AREA_OFFSET(dm_locked.s_image_encryption_key),
    USER_AREA_OFFSET(dm_locked.ns_image_encryption_key),

    USER_AREA_OFFSET(bl1_2_image),
    USER_AREA_OFFSET(cm_locked.bl1_2_image_len),
    OTP_OFFSET(rotpk),
    USER_AREA_OFFSET(dm_locked.bl1_rotpk_0),

    USER_AREA_OFFSET(unlocked_area.bl1_nv_counter),

    USER_AREA_OFFSET(dm_locked.secure_debug_pk),

    USER_AREA_OFFSET(dm_locked.host_rotpk_s),
    USER_AREA_OFFSET(dm_locked.host_rotpk_ns),
    USER_AREA_OFFSET(dm_locked.host_rotpk_cca),

    USER_AREA_OFFSET(cm_locked.cca_system_properties),

    USER_AREA_OFFSET(unlocked_area.reprovisioning_bits),
    USER_AREA_OFFSET(cm_locked.rss_id),
};

static const uint16_t otp_sizes[PLAT_OTP_ID_MAX] = {
    OTP_SIZE(huk),
    OTP_SIZE(guk),
    sizeof(uint32_t),
    USER_AREA_SIZE(dm_locked.iak_len),
    USER_AREA_SIZE(dm_locked.iak_type),
    USER_AREA_SIZE(dm_locked.iak_id),

    USER_AREA_SIZE(dm_locked.implementation_id),
    USER_AREA_SIZE(dm_locked.verification_service_url),
    USER_AREA_SIZE(dm_locked.profile_definition),

    USER_AREA_SIZE(dm_locked.bl2_rotpk[0]),
    USER_AREA_SIZE(dm_locked.bl2_rotpk[1]),
    USER_AREA_SIZE(dm_locked.bl2_rotpk[2]),
    USER_AREA_SIZE(dm_locked.bl2_rotpk[3]),
    USER_AREA_SIZE(dm_locked.bl2_rotpk[4]),
    USER_AREA_SIZE(dm_locked.bl2_rotpk[5]),
    USER_AREA_SIZE(dm_locked.bl2_rotpk[6]),
    USER_AREA_SIZE(dm_locked.bl2_rotpk[7]),
    USER_AREA_SIZE(dm_locked.bl2_rotpk[8]),

    USER_AREA_SIZE(unlocked_area.bl2_nv_counter[0]),
    USER_AREA_SIZE(unlocked_area.bl2_nv_counter[1]),
    USER_AREA_SIZE(unlocked_area.bl2_nv_counter[2]),
    USER_AREA_SIZE(unlocked_area.bl2_nv_counter[3]),
    USER_AREA_SIZE(unlocked_area.bl2_nv_counter[4]),
    USER_AREA_SIZE(unlocked_area.bl2_nv_counter[5]),
    USER_AREA_SIZE(unlocked_area.bl2_nv_counter[6]),
    USER_AREA_SIZE(unlocked_area.bl2_nv_counter[7]),
    USER_AREA_SIZE(unlocked_area.bl2_nv_counter[8]),

#ifdef PLATFORM_HAS_PS_NV_OTP_COUNTERS
    USER_AREA_SIZE(unlocked_area.ps_nv_counter[0]),
    USER_AREA_SIZE(unlocked_area.ps_nv_counter[1]),
    USER_AREA_SIZE(unlocked_area.ps_nv_counter[2]),
#endif /* PLATFORM_HAS_PS_NV_OTP_COUNTERS */

    USER_AREA_SIZE(unlocked_area.host_nv_counter[0]),
    USER_AREA_SIZE(unlocked_area.host_nv_counter[1]),
    USER_AREA_SIZE(unlocked_area.host_nv_counter[2]),

    USER_AREA_SIZE(dm_locked.bl2_encryption_key),
    USER_AREA_SIZE(dm_locked.s_image_encryption_key),
    USER_AREA_SIZE(dm_locked.ns_image_encryption_key),

    USER_AREA_SIZE(bl1_2_image),
    USER_AREA_SIZE(cm_locked.bl1_2_image_len),
    OTP_SIZE(rotpk),
    USER_AREA_SIZE(dm_locked.bl1_rotpk_0),

    USER_AREA_SIZE(unlocked_area.bl1_nv_counter),

    USER_AREA_SIZE(dm_locked.secure_debug_pk),

    USER_AREA_SIZE(dm_locked.host_rotpk_s),
    USER_AREA_SIZE(dm_locked.host_rotpk_ns),
    USER_AREA_SIZE(dm_locked.host_rotpk_cca),

    USER_AREA_SIZE(cm_locked.cca_system_properties),

    USER_AREA_SIZE(unlocked_area.reprovisioning_bits),
    USER_AREA_SIZE(cm_locked.rss_id),
};

static uint32_t count_buffer_zero_bits(const uint8_t* buf, size_t size)
{
    size_t byte_index;
    uint8_t byte;
    uint32_t one_count = 0;

    for (byte_index = 0; byte_index < size; byte_index++) {
        byte = buf[byte_index];
        for (int bit_index = 0; bit_index < 8; bit_index++) {
            one_count += (byte >> bit_index) & 1;
        }
    }

    return (size * 8) - one_count;
}

static enum tfm_plat_err_t otp_read(uint32_t offset, uint32_t len,
                                    uint32_t buf_len, uint8_t *buf)
{
    if (buf_len < len) {
        len = buf_len;
    }

    if (lcm_otp_read(&LCM_DEV_S, offset, len, buf) != LCM_ERROR_NONE) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    } else {
        return TFM_PLAT_ERR_SUCCESS;
    }
}

static enum tfm_plat_err_t otp_write(uint32_t offset, uint32_t len,
                                     uint32_t buf_len, const uint8_t *buf)
{
    enum lcm_error_t err;

    if (buf_len > len) {
        return TFM_PLAT_ERR_INVALID_INPUT;
    }

    err = lcm_otp_write(&LCM_DEV_S, offset, buf_len, buf);
    if (err != LCM_ERROR_NONE) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    return TFM_PLAT_ERR_SUCCESS;
}

static uint32_t count_otp_zero_bits(uint32_t offset, uint32_t len)
{
    uint8_t buf[len];

    otp_read(offset, len, len, buf);
    return count_buffer_zero_bits(buf, len);
}

static enum tfm_plat_err_t verify_zero_bits_count(uint32_t offset,
                                                  uint32_t len_offset,
                                                  uint32_t len_zero_count_offset,
                                                  uint32_t zero_count_offset)
{
    enum lcm_error_t lcm_err;
    uint32_t zero_count;
    uint32_t len;

    lcm_err = lcm_otp_read(&LCM_DEV_S, len_offset, sizeof(len),
                           (uint8_t*)&len);
    if (lcm_err != LCM_ERROR_NONE) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    lcm_err = lcm_otp_read(&LCM_DEV_S, len_zero_count_offset, sizeof(zero_count),
                           (uint8_t*)&zero_count);
    if (lcm_err != LCM_ERROR_NONE) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    if (zero_count != count_buffer_zero_bits((uint8_t *)&len, sizeof(len))) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    lcm_err = lcm_otp_read(&LCM_DEV_S, zero_count_offset, sizeof(zero_count),
                           (uint8_t*)&zero_count);
    if (lcm_err != LCM_ERROR_NONE) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    if (zero_count != count_otp_zero_bits(offset, len)) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    return TFM_PLAT_ERR_SUCCESS;
}

static enum tfm_plat_err_t check_keys_for_tampering(enum lcm_lcs_t lcs)
{
    enum lcm_error_t lcm_err;
    enum tfm_plat_err_t err;
    uint32_t cm_locked_size;

    if (lcs == LCM_LCS_DM || lcs == LCM_LCS_SE) {
            err = verify_zero_bits_count(USER_AREA_OFFSET(cm_locked),
                                         USER_AREA_OFFSET(cm_locked_size),
                                         USER_AREA_OFFSET(cm_locked_size_zero_count),
                                         USER_AREA_OFFSET(cm_zero_count));
            if (err != TFM_PLAT_ERR_SUCCESS) {
                return err;
            }
    }

    if (lcs == LCM_LCS_SE) {
        /* Already been verified, don't need to check the zero-count */
        lcm_err = lcm_otp_read(&LCM_DEV_S, USER_AREA_OFFSET(cm_locked_size),
                               sizeof(cm_locked_size), (uint8_t*)&cm_locked_size);
        if (lcm_err != LCM_ERROR_NONE) {
            return TFM_PLAT_ERR_SYSTEM_ERR;
        }

        err = verify_zero_bits_count(USER_AREA_OFFSET(cm_locked) + cm_locked_size,
                                     USER_AREA_OFFSET(dm_locked_size),
                                     USER_AREA_OFFSET(dm_locked_size_zero_count),
                                     USER_AREA_OFFSET(dm_zero_count));
        if (err != TFM_PLAT_ERR_SUCCESS) {
            return err;
        }
    }

    return TFM_PLAT_ERR_SUCCESS;
}

static enum lcm_lcs_t map_otp_lcs_to_lcm_lcs(enum plat_otp_lcs_t lcs)
{
    switch (lcs) {
    case PLAT_OTP_LCS_ASSEMBLY_AND_TEST:
        return LCM_LCS_CM;
    case PLAT_OTP_LCS_PSA_ROT_PROVISIONING:
        return LCM_LCS_DM;
    case PLAT_OTP_LCS_SECURED:
        return LCM_LCS_SE;
    case PLAT_OTP_LCS_DECOMMISSIONED:
        return LCM_LCS_RMA;
    default:
        return LCM_LCS_INVALID;
    }
}

static enum plat_otp_lcs_t map_lcm_lcs_to_otp_lcs(enum lcm_lcs_t lcs)
{
    switch (lcs) {
    case LCM_LCS_CM:
        return PLAT_OTP_LCS_ASSEMBLY_AND_TEST;
    case LCM_LCS_DM:
        return PLAT_OTP_LCS_PSA_ROT_PROVISIONING;
    case LCM_LCS_SE:
        return PLAT_OTP_LCS_SECURED;
    case LCM_LCS_RMA:
        return PLAT_OTP_LCS_DECOMMISSIONED;
    default:
        return PLAT_OTP_LCS_UNKNOWN;
    }
}

static enum tfm_plat_err_t otp_read_lcs(size_t out_len, uint8_t *out)
{
    enum lcm_lcs_t lcm_lcs;
    enum plat_otp_lcs_t *lcs = (enum plat_otp_lcs_t*) out;

    if (lcm_get_lcs(&LCM_DEV_S, &lcm_lcs)) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    if (out_len != sizeof(uint32_t)) {
        return TFM_PLAT_ERR_INVALID_INPUT;
    }

    *lcs = map_lcm_lcs_to_otp_lcs(lcm_lcs);

    return TFM_PLAT_ERR_SUCCESS;
}

enum tfm_plat_err_t tfm_plat_otp_init(void)
{
    uint32_t otp_size;
    enum lcm_error_t err;
    enum lcm_lcs_t lcs;

    err = lcm_init(&LCM_DEV_S);
    if (err != LCM_ERROR_NONE) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    err = lcm_get_otp_size(&LCM_DEV_S, &otp_size);
    if (err != LCM_ERROR_NONE) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }
    if (otp_size < OTP_OFFSET(user_data) +
                   sizeof(struct plat_user_area_layout_t)) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    err = lcm_get_lcs(&LCM_DEV_S, &lcs);
    if (err != LCM_ERROR_NONE) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    return check_keys_for_tampering(lcs);
}

#define PLAT_OTP_ID_BL2_ROTPK_MAX PLAT_OTP_ID_BL2_ROTPK_0 + MCUBOOT_IMAGE_NUMBER
#define PLAT_OTP_ID_NV_COUNTER_BL2_MAX \
    PLAT_OTP_ID_NV_COUNTER_BL2_0 + MCUBOOT_IMAGE_NUMBER

enum tfm_plat_err_t tfm_plat_otp_read(enum tfm_otp_element_id_t id,
                                      size_t out_len, uint8_t *out)
{
    enum tfm_plat_err_t err;
    size_t size;

    if (id >= PLAT_OTP_ID_MAX) {
        return TFM_PLAT_ERR_INVALID_INPUT;
    }

    if (id >= PLAT_OTP_ID_BL2_ROTPK_MAX && id <= PLAT_OTP_ID_BL2_ROTPK_8) {
        return TFM_PLAT_ERR_UNSUPPORTED;
    }
    if (id >= PLAT_OTP_ID_NV_COUNTER_BL2_MAX && id <= PLAT_OTP_ID_NV_COUNTER_BL2_8) {
        return TFM_PLAT_ERR_UNSUPPORTED;
    }

    switch(id) {
    case PLAT_OTP_ID_LCS:
        return otp_read_lcs(out_len, out);
    case PLAT_OTP_ID_BL1_2_IMAGE:
        err = otp_read(USER_AREA_OFFSET(cm_locked.bl1_2_image_len),
                       USER_AREA_SIZE(cm_locked.bl1_2_image_len),
                       sizeof(size), (uint8_t *)&size);
        if (err != TFM_PLAT_ERR_SUCCESS) {
            return err;
        }

        return otp_read(OTP_TOTAL_SIZE - OTP_DMA_ICS_SIZE - size, size,
                        out_len, out);
    default:
        return otp_read(otp_offsets[id], otp_sizes[id], out_len, out);
    }
}

static enum tfm_plat_err_t otp_write_lcs(size_t in_len, const uint8_t *in)
{
    enum tfm_plat_err_t err;
    uint32_t lcs;
    enum lcm_lcs_t new_lcs = map_otp_lcs_to_lcm_lcs(*(uint32_t*)in);
    enum lcm_error_t lcm_err;
    uint16_t gppc_val = 0;
    uint32_t zero_bit_count;
    size_t region_size;

    if (in_len != sizeof(lcs)) {
        return TFM_PLAT_ERR_INVALID_INPUT;
    }

    switch(new_lcs) {
        case LCM_LCS_DM:
            /* Write the size of the CM locked area */
            region_size = USER_AREA_SIZE(cm_locked);
            err = otp_write(USER_AREA_OFFSET(cm_locked_size),
                            USER_AREA_SIZE(cm_locked_size),
                            sizeof(region_size), (uint8_t *)&region_size);
            if (err != TFM_PLAT_ERR_SUCCESS) {
                return err;
            }

            /* Write the zero-bit count of the CM locked area size */
            zero_bit_count = count_buffer_zero_bits((uint8_t *)&region_size,
                                                    sizeof(region_size));
            err = otp_write(USER_AREA_OFFSET(cm_locked_size_zero_count),
                            USER_AREA_SIZE(cm_locked_size_zero_count),
                            sizeof(zero_bit_count), (uint8_t *)&zero_bit_count);
            if (err != TFM_PLAT_ERR_SUCCESS) {
                return err;
            }

            /* Write the zero-count of the CM locked area */
            zero_bit_count = count_otp_zero_bits(USER_AREA_OFFSET(cm_locked),
                                                 region_size);
            err = otp_write(USER_AREA_OFFSET(cm_zero_count),
                            USER_AREA_SIZE(cm_zero_count), sizeof(zero_bit_count),
                            (uint8_t *)&zero_bit_count);
            if (err != TFM_PLAT_ERR_SUCCESS) {
                return err;
            }
            break;
        case LCM_LCS_SE:
            /* Write the size of the DM locked area */
            region_size = USER_AREA_SIZE(dm_locked);
            err = otp_write(USER_AREA_OFFSET(dm_locked_size),
                            USER_AREA_SIZE(dm_locked_size),
                            sizeof(region_size), (uint8_t *)&region_size);
            if (err != TFM_PLAT_ERR_SUCCESS) {
                return err;
            }

            /* Write the zero-bit count of the DM locked area size */
            zero_bit_count = count_buffer_zero_bits((uint8_t*)&region_size,
                                                    sizeof(region_size));
            err = otp_write(USER_AREA_OFFSET(dm_locked_size_zero_count),
                            USER_AREA_SIZE(dm_locked_size_zero_count),
                            sizeof(zero_bit_count), (uint8_t *)&zero_bit_count);
            if (err != TFM_PLAT_ERR_SUCCESS) {
                return err;
            }

            /* Write the zero-count of the DM locked area */
            zero_bit_count = count_otp_zero_bits(USER_AREA_OFFSET(dm_locked),
                                                 region_size);
            err = otp_write(USER_AREA_OFFSET(dm_zero_count),
                            USER_AREA_SIZE(dm_zero_count), sizeof(zero_bit_count),
                            (uint8_t *)&zero_bit_count);
            if (err != TFM_PLAT_ERR_SUCCESS) {
                return err;
            }
            break;
        case LCM_LCS_RMA:
            break;
        case LCM_LCS_CM:
        case LCM_LCS_INVALID:
            return TFM_PLAT_ERR_SYSTEM_ERR;
    }

    lcm_err = lcm_set_lcs(&LCM_DEV_S, new_lcs, gppc_val);
    if (lcm_err != LCM_ERROR_NONE) {
        return TFM_PLAT_ERR_SYSTEM_ERR;
    }

#ifdef TFM_DUMMY_PROVISIONING
    tfm_hal_system_reset();
#endif /* TFM_DUMMY_PROVISIONING */

    return TFM_PLAT_ERR_SUCCESS;
}

enum tfm_plat_err_t tfm_plat_otp_write(enum tfm_otp_element_id_t id,
                                       size_t in_len, const uint8_t *in)
{
    if (id >= PLAT_OTP_ID_MAX) {
        return TFM_PLAT_ERR_INVALID_INPUT;
    }

    if (id >= PLAT_OTP_ID_BL2_ROTPK_MAX && id <= PLAT_OTP_ID_BL2_ROTPK_8) {
        return TFM_PLAT_ERR_UNSUPPORTED;
    }
    if (id >= PLAT_OTP_ID_NV_COUNTER_BL2_MAX && id <= PLAT_OTP_ID_NV_COUNTER_BL2_8) {
        return TFM_PLAT_ERR_UNSUPPORTED;
    }

    switch (id) {
    case PLAT_OTP_ID_LCS:
        return otp_write_lcs(in_len, in);
    default:
        return otp_write(otp_offsets[id], otp_sizes[id], in_len, in);
    }
}


enum tfm_plat_err_t tfm_plat_otp_get_size(enum tfm_otp_element_id_t id,
                                          size_t *size)
{
    if (id >= PLAT_OTP_ID_MAX) {
        return TFM_PLAT_ERR_INVALID_INPUT;
    }

    if (id >= PLAT_OTP_ID_BL2_ROTPK_MAX && id <= PLAT_OTP_ID_BL2_ROTPK_8) {
        return TFM_PLAT_ERR_UNSUPPORTED;
    }
    if (id >= PLAT_OTP_ID_NV_COUNTER_BL2_MAX && id <= PLAT_OTP_ID_NV_COUNTER_BL2_8) {
        return TFM_PLAT_ERR_UNSUPPORTED;
    }

    *size = otp_sizes[id];

    return TFM_PLAT_ERR_SUCCESS;
}
