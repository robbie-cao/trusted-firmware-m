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
    reg->nh_region[region].cfg0 = (base_addr & 0xFFFFF000);
    reg->nh_region[region].cfg1 = (base_addr >> 32);
    // Set end address
    reg->nh_region[region].cfg2 = (end_addr & 0xFFFFF000);
    reg->nh_region[region].cfg3 = (end_addr >> 32);
    // Set region valid.
    reg->nh_region[region].cfg0 |= 0x1;
    // Set ID for the Target interface.
    reg->nh_region[region].cfg2 |= target_id & 0x7F;

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

    reg->sam_status |= 0x1;

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_psam_disable(struct tower_nci_psam_dev_t *dev)
{
    struct tower_nci_psam_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    reg->sam_status &= 0xFFE;

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
    reg->region[region].prbar_h = base_addr >> 32;
    reg->region[region].prbar_l = (0xFFFFFFC0 & base_addr);
    // Set end address
    reg->region[region].prlar_h = end_addr >> 32;
    reg->region[region].prlar_l = (0xFFFFFFC0 & end_addr);

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
        reg->region[region].prid_l =
                    (reg->region[region].prid_l & 0xFFFF00FF) |
                    ((permission << 8) & 0x0000FF00);
    }

    if (id_select & T_NCI_ID_1_SELECT) {
        reg->region[region].prid_l =
                    (reg->region[region].prid_l & 0x00FFFFFF) |
                    ((permission << 24) & 0xFF000000);
    }

    if (id_select & T_NCI_ID_2_SELECT) {
        reg->region[region].prid_h =
                    (reg->region[region].prid_h & 0xFFFF00FF) |
                    ((permission << 8) & 0x0000FF00);
    }

    if (id_select & T_NCI_ID_2_SELECT) {
        reg->region[region].prid_h =
                    (reg->region[region].prid_h & 0x00FFFFFF) |
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

    reg->region[region].prbar_l |= (0x4 & (lock << 2));

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_set_br(struct tower_nci_apu_dev_t *dev,
                                          uint32_t region, uint32_t background)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    reg->region[region].prbar_l |= (0x2 & (background << 1));

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_set_region_enable(
                        struct tower_nci_apu_dev_t *dev, uint32_t region)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    reg->region[region].prbar_l |= 0x1;

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_set_id_valid(struct tower_nci_apu_dev_t *dev,
                                                uint32_t region, uint32_t valid)
{
    struct tower_nci_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return TOWER_NCI_ERR_INVALID_ARG;
    }

    reg->region[region].prlar_l |= (0xF & valid);

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
        reg->region[region].prid_l =
                    (reg->region[region].prid_l & 0xFFFFFF00) |
                    (id_value & 0x000000FF);
    }

    if (id_select & T_NCI_ID_1_SELECT) {
        reg->region[region].prid_l =
                    (reg->region[region].prid_l & 0xFF00FFFF) |
                    ((id_value << 16) & 0x00FF0000);
    }

    if (id_select & T_NCI_ID_2_SELECT) {
        reg->region[region].prid_h =
                    (reg->region[region].prid_h & 0xFFFFFF00) |
                    (id_value & 0x000000FF);
    }

    if (id_select & T_NCI_ID_3_SELECT) {
        reg->region[region].prid_h =
                    (reg->region[region].prid_h & 0xFF00FFFF) |
                    ((id_value << 16) & 0x00FF0000);
    }

    return TOWER_NCI_SUCCESS;
}

enum tower_nci_err_t tower_nci_apu_initialize_region(
                        struct tower_nci_apu_dev_t *dev, uint32_t region,
                        uint64_t base_addr, uint64_t end_addr,
                        enum tower_nci_apu_br_type_t background,
                        uint32_t permissions[4], uint8_t entity_ids[4],
                        uint32_t id_valid,
                        enum tower_nci_apu_region_enable_type_t region_enable,
                        enum tower_nci_apu_lock_type_t lock)
{
    enum tower_nci_err_t err;
    uint32_t id_idx, id_select;

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

    err = tower_nci_apu_set_id_valid(dev, region, id_valid);
    if (err != TOWER_NCI_SUCCESS) {
        return err;
    }

    for (id_idx = 0; id_idx < 4; ++id_idx) {
        switch (id_idx)
        {
        case 0: id_select = T_NCI_ID_0_SELECT; break;
        case 1: id_select = T_NCI_ID_1_SELECT; break;
        case 2: id_select = T_NCI_ID_2_SELECT; break;
        case 3: id_select = T_NCI_ID_3_SELECT; break;
        }

        err = tower_nci_apu_set_access_perms(dev, region, permissions[id_idx],
                                             id_select);
        if (err != TOWER_NCI_SUCCESS) {
            return err;
        }

        err = tower_nci_apu_assign_id(dev, region, entity_ids[id_idx],
                                      id_select);
        if (err != TOWER_NCI_SUCCESS) {
            return err;
        }
    }

    if (region_enable = T_NCI_REGION_ENABLE) {
        err = tower_nci_apu_set_region_enable(dev, region);
        if (err != TOWER_NCI_SUCCESS) {
            return 1;
        }
    }

    err = tower_nci_apu_set_lock(dev, region, lock);
    if (err != TOWER_NCI_SUCCESS) {
        return 1;
    }
    return TOWER_NCI_SUCCESS;
}

// TODO: Remove print
#define PRINT_TOWER_NCI_TREE
#ifdef PRINT_TOWER_NCI_TREE
void print(struct tower_nci_discovery_tree_t *node, int tab) {
    if(node == NULL)
        return;

    print(node->sibling, tab);

    for (int i = 0; i < tab; ++i) {
        printf("\t");
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
        case TOWER_NCI_FCU: printf("FCU"); break;
        case TOWER_NCI_IDM: printf("IDM"); break;
        case TOWER_NCI_FMU: printf("FMU"); break;
    }
    printf("[%d] -> 0x%lx\r\n", (uint32_t)node->id, (uint32_t)node->address);

    print(node->child, tab+1);
}
#endif

/*
 * Tower NCI Discovery
 */
enum tower_nci_err_t tower_nci_discovery(
                    struct tower_nci_discovery_tree_t *cfg_node,
                    const uint32_t periph_base,
                    uint8_t (*pruning_function)(uint16_t, uint16_t, uint16_t))
{
    uint16_t type = 0, id = 0;
    uint32_t address, child_count, c_idx = 0;
    uint32_t hdr_base = periph_base + cfg_node->address;

    struct tower_nci_discovery_tree_t* node;
    struct tower_nci_discovery_tree_t* sibling = NULL;

    enum tower_nci_err_t err;

    /* FMU check requires Tower NCI's config node granularity to be 64KB */
    if((cfg_node->type != TOWER_NCI_CFGNI) &&
       (*(uint32_t *)(hdr_base + 0xFFE0) != 0)) {
        cfg_node->type = TOWER_NCI_FMU;
        cfg_node->id = 0;
        return TOWER_NCI_SUCCESS;
    }

    /* Fetch number of children */
    if (TOWER_NCI_TYPE_IS_DOMAIN(cfg_node->type)) {
        child_count = ((tower_nci_domain_cfg_hdr *)(hdr_base))->child_node_info;
    } else if (TOWER_NCI_TYPE_IS_COMPONENT(cfg_node->type)) {
        if (cfg_node == TOWER_NCI_PMU) {
            /* Skipping because PMU doesn't have children */
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
        /* Skip discovering node based on the pruning function */
        if ((*pruning_function)(cfg_node->type, cfg_node->id, c_idx) != 0) {
            return TOWER_NCI_SUCCESS;
        }

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
            id = cfg_node->id;
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

        err = tower_nci_discovery(node, periph_base, pruning_function);
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
