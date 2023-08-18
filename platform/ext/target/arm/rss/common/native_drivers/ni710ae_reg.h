/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __NI710AE_REG_H__
#define __NI710AE_REG_H__

#include <stdint.h>

/**
 * \brief NI710AE APU register map
 */
struct ni710ae_apu_reg_map_t {
    struct {
        volatile uint32_t prbar_l;
        volatile uint32_t prbar_h;
        volatile uint32_t prlar_l;
        volatile uint32_t prlar_h;
        volatile uint32_t prid_l;
        volatile uint32_t prid_h;
        volatile uint32_t reserved_0[2];
    } region[32];
    volatile uint32_t reserved_1[766];
    volatile uint32_t apu_ctlr;
    volatile uint32_t apu_iidr;
};

#endif /* __NI710AE_REG_H__ */
