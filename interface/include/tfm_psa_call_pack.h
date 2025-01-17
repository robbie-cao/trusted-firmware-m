/*
 * Copyright (c) 2021-2022, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __TFM_PSA_CALL_PACK_H__
#define __TFM_PSA_CALL_PACK_H__

#include "psa/client.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  31           30-28   27    26-24  23-20   19     18-16   15-0
 * +------------+-----+------+-------+-----+-------+-------+------+
 * |            |     |      | invec |     |       | outvec| type |
 * | Res        | Res | Res  | number| Res | Res   | number|      |
 * +------------+-----+------+-------+-----+-------+-------+------+
 *
 * Res: Reserved.
 */
#define TYPE_MASK       0xFFFFUL

#define IN_LEN_OFFSET   24
#define IN_LEN_MASK     (0x7UL << IN_LEN_OFFSET)

#define OUT_LEN_OFFSET  16
#define OUT_LEN_MASK    (0x7UL << OUT_LEN_OFFSET)

#define PARAM_PACK(type, in_len, out_len)                          \
        ((((uint32_t)(type)) & TYPE_MASK)                        | \
         ((((uint32_t)(in_len)) << IN_LEN_OFFSET) & IN_LEN_MASK) | \
         ((((uint32_t)(out_len)) << OUT_LEN_OFFSET) & OUT_LEN_MASK))

#define PARAM_UNPACK_TYPE(ctrl_param)                              \
        ((int32_t)(int16_t)((ctrl_param) & TYPE_MASK))

#define PARAM_UNPACK_IN_LEN(ctrl_param)                            \
        ((size_t)(((ctrl_param) & IN_LEN_MASK) >> IN_LEN_OFFSET))

#define PARAM_UNPACK_OUT_LEN(ctrl_param)                           \
        ((size_t)(((ctrl_param) & OUT_LEN_MASK) >> OUT_LEN_OFFSET))

psa_status_t tfm_psa_call_pack(psa_handle_t handle,
                               uint32_t ctrl_param,
                               const psa_invec *in_vec,
                               psa_outvec *out_vec);

#ifdef __cplusplus
}
#endif

#endif /* __TFM_PSA_CALL_PACK_H__ */
