/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __TOWER_NCI_LIB_H__
#define __TOWER_NCI_LIB_H__

#include <stdint.h>

/**
 * \brief Programs Tower NCI PSAM and APU
 *
 * \param[in] nci_addr          Tower NCI base address
 * \param[in] chip_addr_offset  Offset address based on current Chip ID
 *
 * \return Returns 1 if there is an error, else 0.
 */
uint32_t program_sysctrl_tower_nci(uint32_t nci_addr,
                                   uint64_t chip_addr_offset);

/**
 * \brief Program Tower NCI peripheral block APUs
 *
 * \param[in] nci_addr          Periph Tower NCI base address
 *
 * \return Returns 1 if there is an error, else 0.
 */
uint32_t program_periph_tower_nci(uint32_t nci_addr);

#endif __TOWER_NCI_LIB_H__
