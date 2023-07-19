/*
 * Copyright (c) 2021-2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "target_cfg.h"
#include "cmsis.h"

struct platform_data_t tfm_peripheral_std_uart = {
        RSS_UART0_BASE_S,
        RSS_UART0_BASE_S + 0xFFF,
        PPC_SP_DO_NOT_CONFIGURE,
        -1
};
