/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "tower_nci_drv.h"
#include "tower_nci_reg.h"

#include <stdio.h>
#include <stdlib.h>

#define PSAM_REGION_SELECT(register, region) \
        *((&(register)) + ((0x10 * (region)) >> 2))
#define APU_REGION_SELECT(register, region) \
        (*((&(register)) + ((0x20 * (region)) >> 2)))

#define TOWER_NCI_TYPE_IS_DOMAIN(type)                          \
    ((type) == TOWER_NCI_CFGNI) || ((type) == TOWER_NCI_VD) ||  \
    ((type) == TOWER_NCI_PD) || ((type) == TOWER_NCI_CD)
#define TOWER_NCI_TYPE_IS_COMPONENT(type)                       \
    ((type) == TOWER_NCI_ASNI) || ((type) == TOWER_NCI_AMNI) || \
    ((type) == TOWER_NCI_PMU) || ((type) == TOWER_NCI_HSNI) ||  \
    ((type) == TOWER_NCI_HMNI) || ((type) == TOWER_NCI_PMNI)
#define TOWER_NCI_TYPE_IS_SUBFEATURE(type)                      \
    ((type) == TOWER_NCI_PSAM) || ((type) == TOWER_NCI_APU) ||  \
    ((type) == TOWER_NCI_FCU) || ((type) == TOWER_NCI_IDM)

/* Tower NCI Domain top registers. These are common for all domains */
typedef struct {
    volatile uint32_t node_type;
    volatile uint32_t child_node_info;
    volatile uint32_t x_pointers[];
}tower_nci_domain_cfg_hdr;

/* Tower NCI Sub-feature register set. Found in component domain */
typedef struct {
    volatile uint32_t type;
    volatile uint32_t pointer;
}tower_nci_sub_feature_cfg_attr;

/* Tower NCI Component top registers. These are common for all components */
typedef struct {
    volatile uint32_t node_type;
             uint32_t reserved_0[4];
    volatile uint32_t interface_id_0_3;
             uint32_t reserved_1[58];
    volatile uint32_t num_sub_features;
             uint32_t reserved_2;
    tower_nci_sub_feature_cfg_attr
                      sub_feature[];
}tower_nci_component_cfg_hdr;


/*
 * PSAM Programming
 */
enum tower_nci_err_t tower_nci_psam_nhregion_init(
                        struct tower_nci_psam_dev_t *dev, uint64_t base_addr,
                        uint64_t end_addr, uint64_t target_id, uint64_t region)
{
    struct tower_nci_psam_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    // Set base address
    PSAM_REGION_SELECT(reg->nh_region_cfg0, region) = (base_addr & 0xFFFFF000);
    PSAM_REGION_SELECT(reg->nh_region_cfg1, region) = (base_addr >> 32);
    // Set end address
    PSAM_REGION_SELECT(reg->nh_region_cfg2, region) = (end_addr & 0xFFFFF000);
    PSAM_REGION_SELECT(reg->nh_region_cfg3, region) = (end_addr >> 32);
    // Set region valid.
    PSAM_REGION_SELECT(reg->nh_region_cfg0, region) |= 0x1;
    // Set ID for the Target interface.
    PSAM_REGION_SELECT(reg->nh_region_cfg2, region) |= target_id & 0x7F;

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_psam_set_default_tgtid(
                        struct tower_nci_psam_dev_t *dev, uint32_t tgtid)
{
    struct tower_nci_psam_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    reg->sam_status =  (reg->sam_status & 0xFFFFFF01) |(tgtid << 1);

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_psam_enable(struct tower_nci_psam_dev_t *dev)
{
    struct tower_nci_psam_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    reg->sam_status = 0x1;

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_psam_disable(struct tower_nci_psam_dev_t *dev)
{
    struct tower_nci_psam_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    reg->sam_status = 0x0;

    return TOWER_NCI_SUCCESS;
}

/*
 * APU Programming
 */
enum tower_nci_err_t tower_nci_apu_set_addr_range(
                        struct tower_nci_apu_dev_t *dev, uint32_t region,
                        uint64_t base_addr, uint64_t end_addr)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    // Set base address
    APU_REGION_SELECT(reg->prbar_h, region) = base_addr >> 32;
    APU_REGION_SELECT(reg->prbar_l, region) = (0xFFFFFFC0 & base_addr);
    // Set end address
    APU_REGION_SELECT(reg->prlar_h, region) = end_addr >> 32;
    APU_REGION_SELECT(reg->prlar_l, region) = (0xFFFFFFC0 & end_addr);

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_set_access_perms(
                        struct tower_nci_apu_dev_t *dev, uint32_t region,
                        uint32_t permission, uint32_t id_select)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    if (id_select & T_NCI_ID_0_SELECT) {
        APU_REGION_SELECT(reg->prid_l, region) =
                    (APU_REGION_SELECT(reg->prid_l, region) & 0xFFFF00FF) |
                    ((permission << 8) & 0x0000FF00);
    }

    if (id_select & T_NCI_ID_1_SELECT) {
        APU_REGION_SELECT(reg->prid_l, region) =
                    (APU_REGION_SELECT(reg->prid_l, region) & 0x00FFFFFF) |
                    ((permission << 24) & 0xFF000000);
    }

    if (id_select & T_NCI_ID_2_SELECT) {
        APU_REGION_SELECT(reg->prid_h, region) =
                    (APU_REGION_SELECT(reg->prid_h, region) & 0xFFFF00FF) |
                    ((permission << 8) & 0x0000FF00);
    }

    if (id_select & T_NCI_ID_2_SELECT) {
        APU_REGION_SELECT(reg->prid_h, region) =
                    (APU_REGION_SELECT(reg->prid_h, region) & 0x00FFFFFF) |
                    ((permission << 24) & 0xFF000000);
    }

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_set_lock(struct tower_nci_apu_dev_t *dev,
                                            uint32_t region, uint32_t lock)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    APU_REGION_SELECT(reg->prbar_l, region) |= (0x4 & (lock << 2));

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_set_br(struct tower_nci_apu_dev_t *dev,
                                          uint32_t region, uint32_t background)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    APU_REGION_SELECT(reg->prbar_l, region) |= (0x2 & (background << 1));

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_set_region_enable(
                        struct tower_nci_apu_dev_t *dev, uint32_t region)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    APU_REGION_SELECT(reg->prbar_l, region) |= 0x1;

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_set_id_valid(struct tower_nci_apu_dev_t *dev,
                                                uint32_t region, uint32_t valid)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    APU_REGION_SELECT(reg->prlar_l, region) |= (0xF | valid);

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_enable(struct tower_nci_apu_dev_t *dev)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    reg->apu_ctlr |= 0x1;

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_sync_err_enable(
                            struct tower_nci_apu_dev_t *dev)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    reg->apu_ctlr |= 0x4;

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_assign_id(struct tower_nci_apu_dev_t *dev,
                        uint32_t region, uint32_t id_value, uint32_t id_select)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    if (id_select & T_NCI_ID_0_SELECT) {
        APU_REGION_SELECT(reg->prid_l, region) =
                    (APU_REGION_SELECT(reg->prid_l, region) & 0xFFFFFF00) |
                    (id_value & 0x000000FF);
    }

    if (id_select & T_NCI_ID_1_SELECT) {
        APU_REGION_SELECT(reg->prid_l, region) =
                    (APU_REGION_SELECT(reg->prid_l, region) & 0xFF00FFFF) |
                    ((id_value << 16) & 0x00FF0000);
    }

    if (id_select & T_NCI_ID_2_SELECT) {
        APU_REGION_SELECT(reg->prid_h, region) =
                    (APU_REGION_SELECT(reg->prid_h, region) & 0xFFFFFF00) |
                    (id_value & 0x000000FF);
    }

    if (id_select & T_NCI_ID_3_SELECT) {
        APU_REGION_SELECT(reg->prid_h, region) =
                    (APU_REGION_SELECT(reg->prid_h, region) & 0xFF00FFFF) |
                    ((id_value << 16) & 0x00FF0000);
    }

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_initialize_region(
                        struct tower_nci_apu_dev_t *dev, uint32_t region,
                        uint64_t base_addr, uint64_t end_addr,
                        enum tower_nci_apu_br_type_t background, uint32_t permission,
                        uint32_t id_select, uint32_t valid)
{
    enum tower_nci_err_t err;

    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    err = tower_nci_apu_set_addr_range(dev, region, base_addr, end_addr);
    if (err != TOWER_NCI_SUCCESS) {
        return err;
    }

    err = tower_nci_apu_set_br(dev, region, background);
    if (err != TOWER_NCI_SUCCESS) {
        return err;
    }

    err = tower_nci_apu_set_id_valid(dev, region, valid);
    if (err != TOWER_NCI_SUCCESS) {
        return err;
    }

    err = tower_nci_apu_set_access_perms(dev, region, permission, id_select);
    if (err != TOWER_NCI_SUCCESS) {
        return err;
    }

    return TOWER_NCI_SUCCESS;
}

// TODO: Remove print
#define PRINT_TOWER_NCI_TREE
#ifdef PRINT_TOWER_NCI_TREE
void print(struct tower_nci_discovery_tree_t *node, int tab) {
    if(node == NULL)
        return;
    for (int i = 0; i < tab; ++i) {
        printf("  ");
    }
    switch(node->type) {
        case TOWER_NCI_CFGNI: printf("CFGNI"); break;
        case TOWER_NCI_VD: printf("VD"); break;
        case TOWER_NCI_PD: printf("PD"); break;
        case TOWER_NCI_CD: printf("CD"); break;
        case TOWER_NCI_ASNI: printf("ASNI"); break;
        case TOWER_NCI_AMNI: printf("AMNI"); break;
        case TOWER_NCI_PMU: printf("PMU"); break;
        case TOWER_NCI_HSNI: printf("HSNI"); break;
        case TOWER_NCI_HMNI: printf("HMNI"); break;
        case TOWER_NCI_PMNI: printf("PMNI"); break;
        case TOWER_NCI_PSAM: printf("PSAM"); break;
        case TOWER_NCI_APU: printf("APU"); break;
    }
    printf(" -> %lx addr: %lx\r\n", (uint32_t)node->id, (uint32_t)node->address);

    print(node->child, tab+1);
    print(node->sibling, tab);
}
#endif

/*
 * Tower NCI Discovery
 */
enum tower_nci_err_t tower_nci_discovery(struct tower_nci_discovery_tree_t *cfg_node,
                                         const uint32_t periph_base)
{
    uint16_t type = 0, id = 0;
    uint32_t address, child_count, c_idx = 0;
    uint32_t hdr_base = periph_base + cfg_node->address;

    struct tower_nci_discovery_tree_t* node;
    struct tower_nci_discovery_tree_t* sibling = NULL;

    enum tower_nci_err_t err;

    /* TODO:
     *  The following checks for the FMU component and skips addition of FMU
     *  node to the discovery tree.
     *
     *  Currently, FVP has a bug while reading the offset at 0xFFE0 - faults out
     *  while reading the value. Once this issue debugged when can enable this
     *  check.
     */
    // if((cfg_node->type != TOWER_NCI_CFGNI) &&
    //    (*(uint32_t *)(hdr_base + 0xFFE0) != 0)) {
    //     /* Skipping FMUs */
    //     return TOWER_NCI_SUCCESS;
    // }

    if (TOWER_NCI_TYPE_IS_DOMAIN(cfg_node->type)) {
        child_count = ((tower_nci_domain_cfg_hdr *)(hdr_base))->child_node_info;
    } else if (TOWER_NCI_TYPE_IS_COMPONENT(cfg_node->type)) {
        if (cfg_node == TOWER_NCI_PMU) {
            // PMU doesn't have children
            return TOWER_NCI_SUCCESS;
        }
        child_count = ((tower_nci_component_cfg_hdr *)(hdr_base))->
                                                               num_sub_features;
    } else if (TOWER_NCI_TYPE_IS_SUBFEATURE(cfg_node->type)) {
        return TOWER_NCI_SUCCESS;
    } else {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    for(; c_idx < child_count; ++c_idx) {
        if (TOWER_NCI_TYPE_IS_DOMAIN(cfg_node->type)) {
            address = ((tower_nci_domain_cfg_hdr *)hdr_base)->x_pointers[c_idx];
            tower_nci_domain_cfg_hdr *child_hdr = periph_base + address;
            type = child_hdr->node_type & 0xFFFF;
            id = child_hdr->node_type >> 16;
        } else {
            tower_nci_component_cfg_hdr *child_hdr = periph_base +
                                                            cfg_node->address;
            address = child_hdr->sub_feature[c_idx].pointer;
            switch(child_hdr->sub_feature[c_idx].type) {
                case 0x0: type = TOWER_NCI_APU; break;
                case 0x1: type = TOWER_NCI_PSAM; break;
                case 0x2: type = TOWER_NCI_FCU; break;
                case 0x3: type = TOWER_NCI_IDM; break;
                default: return TOWER_NCI_ERR;
            }
        }

        node = calloc(sizeof(struct tower_nci_discovery_tree_t), 1);

        memcpy(node,
                &(struct tower_nci_discovery_tree_t) {
                .type = type,
                .id = id,
                .address = address,
                .sibling = sibling,
                }, sizeof(struct tower_nci_discovery_tree_t));
        cfg_node->child = node;
        cfg_node->children++;
        sibling = node;

        err = tower_nci_discovery(node, periph_base);
        if (err != TOWER_NCI_SUCCESS) {
            return err;
        }
    }

//Todo: Remove print call
#ifdef PRINT_TOWER_NCI_TREE
    if(cfg_node->type == 0) print(cfg_node, 0);
#endif
    return TOWER_NCI_SUCCESS;
}

uint32_t tower_nci_fetch_offset_address(struct tower_nci_discovery_tree_t *root,
                                        uint16_t component_type,
                                        uint16_t component_id,
                                        uint16_t sub_feature_type)
{
    struct tower_nci_discovery_tree_t *node;
    uint32_t ret_addr;

    if(root == NULL) {
        return -1;
    }

    if (root->id == component_id && root->type == component_type) {
        node = root->child;
        while(node != NULL) {
            if (sub_feature_type == node->type) {
                return node->address;
            }
            node = node->sibling;
        }
    }

    ret_addr = tower_nci_fetch_offset_address(root->child, component_type,
                                              component_id, sub_feature_type);
    if (ret_addr != -1) {
        return ret_addr;
    }

    return tower_nci_fetch_offset_address(root->sibling, component_type,
                                          component_id, sub_feature_type);
}

void free_discovery_tree(struct tower_nci_discovery_tree_t* tree) {
    if(tree == NULL)
        return;

    free_discovery_tree(tree->child);
    free_discovery_tree(tree->sibling);
    free(tree);
}

#undef PSAM_REGION_SELECT
#undef APU_REGION_SELECT
