/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __FAINLIGHT_GIC_LIB_H__
#define __FAINLIGHT_GIC_LIB_H__

#include <stdint.h>

int gic_multiple_view_probe(uint32_t view0_base);

int gic_multiple_view_programming(void);

#endif /* __FAINLIGHT_GIC_LIB_H__ */
