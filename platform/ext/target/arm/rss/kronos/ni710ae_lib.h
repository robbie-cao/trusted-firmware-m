/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __NI710AE_LIB_H__
#define __NI710AE_LIB_H__

#include <stdint.h>
#include <stdbool.h>

/**
 * \brief Programs NI710AE APU
 *
 * \param[in] nci_addr          NI710AE base address
 * \param[in] chip_addr_offset  Offset address based on current Chip ID
 * \param[in] is_boot_time      Flag to determine the boot/run time
 *
 * \return Returns 1 if there is an error, else 0.
 */
uint32_t program_si_ni710ae_apu(uint32_t nci_addr, uint64_t chip_addr_offset,
                                bool is_boot_time);

#endif /* __NI710AE_LIB_H__ */
