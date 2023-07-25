/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __TOWER_NCI_REG_H__
#define __TOWER_NCI_REG_H__

#include <stdint.h>

/**
 * \brief Tower NCI PSAM register map
 */
struct tower_nci_psam_reg_map_t {
    volatile uint32_t sam_unit_info;
    volatile uint32_t sam_status;
             uint32_t reserved__0[2];
    volatile uint32_t htg_addr_mask_l;
    volatile uint32_t htg_addr_mask_u;
    volatile uint32_t axid_mask;
             uint32_t reserved_0;
    volatile uint32_t cmp_addr_mask_l;
    volatile uint32_t cmp_addr_mask_u;
             uint32_t reserved_1[2];
    volatile uint32_t generic_config_reg0;
    volatile uint32_t generic_config_reg1;
             uint32_t reserved_2[50];
    volatile uint32_t nh_region_cfg0;
    volatile uint32_t nh_region_cfg1;
    volatile uint32_t nh_region_cfg2;
    volatile uint32_t nh_region_cfg3;
             uint32_t reserved_3[508];
    volatile uint32_t htg_region_cfg0;
    volatile uint32_t htg_region_cfg1;
    volatile uint32_t htg_region_cfg2;
    volatile uint32_t htg_region_cfg3;
             uint32_t reserved_4[124];
    volatile uint32_t htg_tgtid_cfg;
             uint32_t reserved_5[127];
    volatile uint32_t np2_top_addr_cfg;
};

/**
 * \brief Tower NCI APU register map
 */
struct tower_nci_apu_reg_map_t {
    volatile uint32_t prbar_l;
    volatile uint32_t prbar_h;
    volatile uint32_t prlar_l;
    volatile uint32_t prlar_h;
    volatile uint32_t prid_l;
    volatile uint32_t prid_h;
             uint32_t reserved_1[1016];
    volatile uint32_t apu_ctlr;
    volatile uint32_t apu_iidr;
};

#endif /* __TOWER_NCI_REG_H__ */
