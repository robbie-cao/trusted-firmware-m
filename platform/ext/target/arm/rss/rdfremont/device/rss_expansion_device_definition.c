/*
 * Copyright (c) 2023 Arm Limited. All rights reserved.
 *
 * Licensed under the Apache License Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * \file rss_expansion_device_definition.c
 * \brief This file defines exports the structures based on the peripheral
 * definitions from device_cfg.h.
 * This file is meant to be used as a helper for baremetal
 * applications and/or as an example of how to configure the generic
 * driver structures.
 */

#include "rss_expansion_device_definition.h"
#include "platform_base_address.h"

/* Arm UART PL011 driver structures */
#ifdef RSS_UART0_PL011_S
static const struct uart_pl011_dev_cfg_t RSS_UART0_PL011_DEV_CFG_S = {
    .base = RSS_UART0_BASE_S,
    .def_baudrate = DEFAULT_UART_BAUDRATE,
    .def_wlen = UART_PL011_WLEN_8,
    .def_parity = UART_PL011_PARITY_DISABLED,
    .def_stopbit = UART_PL011_STOPBIT_1};
static struct uart_pl011_dev_data_t RSS_UART0_PL011_DEV_DATA_S = {
    .state = 0,
    .uart_clk = 0,
    .baudrate = 0};
struct uart_pl011_dev_t RSS_UART0_PL011_DEV_S = {
    &(RSS_UART0_PL011_DEV_CFG_S),
    &(RSS_UART0_PL011_DEV_DATA_S)};
#endif
