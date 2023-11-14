/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "ni710ae_drv.h"
#include "ni710ae_reg.h"

#include <stdio.h>
#include <stdlib.h>

#define NI710AE_TYPE_IS_DOMAIN(type)                          \
    ((type) == NI710AE_CFGNI) || ((type) == NI710AE_VD) ||  \
    ((type) == NI710AE_PD) || ((type) == NI710AE_CD)
#define NI710AE_TYPE_IS_COMPONENT(type)                       \
    ((type) == NI710AE_ASNI) || ((type) == NI710AE_AMNI) || \
    ((type) == NI710AE_PMU) || ((type) == NI710AE_HSNI) ||  \
    ((type) == NI710AE_HMNI) || ((type) == NI710AE_PMNI) || \
    ((type) == NI710AE_FMU)
#define NI710AE_TYPE_IS_SUBFEATURE(type)                      \
    ((type) == NI710AE_APU) || ((type) == NI710AE_SAM) ||  \
    ((type) == NI710AE_FCU) || ((type) == NI710AE_IDM) || \
    ((type) == NI710AE_RAS)

/* NI710AE Domain top registers. These are common for all domains */
typedef struct {
    volatile uint32_t node_type;
    volatile uint32_t child_node_info;
    volatile uint32_t x_pointers[];
} ni710ae_domain_cfg_hdr;

/* NI710AE Sub-feature register set. Found in component domain */
typedef struct {
    volatile uint32_t type;
    volatile uint32_t pointer;
} ni710ae_sub_feature_cfg_attr;

/* NI710AE Component top registers. These are common for all components */
typedef struct {
    volatile uint32_t node_type;
             uint32_t reserved_0[8];
    volatile uint32_t num_sub_features;
    ni710ae_sub_feature_cfg_attr sub_feature[];
} ni710ae_component_cfg_hdr;

/*
 * APU Programming
 */
enum ni710ae_err_t
ni710ae_apu_set_addr_range(struct ni710ae_apu_dev_t *dev, uint32_t region,
                           uint64_t base_addr, uint64_t end_addr)
{
    struct ni710ae_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return NI710AE_ERR_INVALID_ARG;
    }

    // Set base address
    reg->region[region].prbar_h = base_addr >> 32;
    reg->region[region].prbar_l = (0xFFFFFFC0 & base_addr);
    // Set end address
    reg->region[region].prlar_h = end_addr >> 32;
    reg->region[region].prlar_l = (0xFFFFFFC0 & end_addr);

    return NI710AE_SUCCESS;
}

enum ni710ae_err_t
ni710ae_apu_set_access_perms(struct ni710ae_apu_dev_t *dev, uint32_t region,
                             uint32_t permission, uint32_t id_select)
{
    struct ni710ae_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return NI710AE_ERR_INVALID_ARG;
    }

    if (id_select & NCI_ID_0_SELECT) {
        reg->region[region].prid_l =
                    (reg->region[region].prid_l & 0xFFFF00FF) |
                    ((permission << 8) & 0x0000FF00);
    }

    if (id_select & NCI_ID_1_SELECT) {
        reg->region[region].prid_l =
                    (reg->region[region].prid_l & 0x00FFFFFF) |
                    ((permission << 24) & 0xFF000000);
    }

    if (id_select & NCI_ID_2_SELECT) {
        reg->region[region].prid_h =
                    (reg->region[region].prid_h & 0xFFFF00FF) |
                    ((permission << 8) & 0x0000FF00);
    }

    if (id_select & NCI_ID_3_SELECT) {
        reg->region[region].prid_h =
                    (reg->region[region].prid_h & 0x00FFFFFF) |
                    ((permission << 24) & 0xFF000000);
    }

    return NI710AE_SUCCESS;
}

enum ni710ae_err_t
ni710ae_apu_set_lock(struct ni710ae_apu_dev_t *dev, uint32_t region,
                     uint32_t lock)
{
    struct ni710ae_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return NI710AE_ERR_INVALID_ARG;
    }

    reg->region[region].prbar_l |= (0x4 & (lock << 2));

    return NI710AE_SUCCESS;
}

enum ni710ae_err_t ni710ae_apu_set_br(struct ni710ae_apu_dev_t *dev,
                                      uint32_t region, uint32_t background)
{
    struct ni710ae_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return NI710AE_ERR_INVALID_ARG;
    }

    reg->region[region].prbar_l |= (0x2 & (background << 1));

    return NI710AE_SUCCESS;
}

enum ni710ae_err_t
ni710ae_apu_set_region_enable(struct ni710ae_apu_dev_t *dev, uint32_t region)
{
    struct ni710ae_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return NI710AE_ERR_INVALID_ARG;
    }

    reg->region[region].prbar_l |= 0x1;

    return NI710AE_SUCCESS;
}

enum ni710ae_err_t
ni710ae_apu_set_id_valid(struct ni710ae_apu_dev_t *dev, uint32_t region,
                         uint32_t valid)
{
    struct ni710ae_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return NI710AE_ERR_INVALID_ARG;
    }

    reg->region[region].prlar_l |= (0xF & valid);

    return NI710AE_SUCCESS;
}

enum ni710ae_err_t ni710ae_apu_enable(struct ni710ae_apu_dev_t *dev)
{
    struct ni710ae_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return NI710AE_ERR_INVALID_ARG;
    }

    reg->apu_ctlr |= 0x1;

    return NI710AE_SUCCESS;
}

enum ni710ae_err_t
ni710ae_apu_sync_err_enable(struct ni710ae_apu_dev_t *dev)
{
    struct ni710ae_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return NI710AE_ERR_INVALID_ARG;
    }

    reg->apu_ctlr |= 0x4;

    return NI710AE_SUCCESS;
}

enum ni710ae_err_t
ni710ae_apu_assign_id(struct ni710ae_apu_dev_t *dev, uint32_t region,
                      uint32_t id_value, uint32_t id_select)
{
    struct ni710ae_apu_reg_map_t* reg = dev->base;
    if (dev == NULL) {
        return NI710AE_ERR_INVALID_ARG;
    }

    if (id_select & NCI_ID_0_SELECT) {
        reg->region[region].prid_l =
                    (reg->region[region].prid_l & 0xFFFFFF00) |
                    (id_value & 0x000000FF);
    }

    if (id_select & NCI_ID_1_SELECT) {
        reg->region[region].prid_l =
                    (reg->region[region].prid_l & 0xFF00FFFF) |
                    ((id_value << 16) & 0x00FF0000);
    }

    if (id_select & NCI_ID_2_SELECT) {
        reg->region[region].prid_h =
                    (reg->region[region].prid_h & 0xFFFFFF00) |
                    (id_value & 0x000000FF);
    }

    if (id_select & NCI_ID_3_SELECT) {
        reg->region[region].prid_h =
                    (reg->region[region].prid_h & 0xFF00FFFF) |
                    ((id_value << 16) & 0x00FF0000);
    }

    return NI710AE_SUCCESS;
}

enum ni710ae_err_t ni710ae_apu_initialize_region(
                        struct ni710ae_apu_dev_t *dev, uint32_t region,
                        uint64_t base_addr, uint64_t end_addr,
                        enum ni710ae_apu_br_type_t background,
                        uint32_t permissions[4], uint8_t entity_ids[4],
                        uint32_t id_valid,
                        enum ni710ae_apu_region_enable_type_t region_enable,
                        enum ni710ae_apu_lock_type_t lock)
{
    enum ni710ae_err_t err;
    uint32_t id_idx, id_select;

    if (dev == NULL) {
        return NI710AE_ERR_INVALID_ARG;
    }

    err = ni710ae_apu_set_addr_range(dev, region, base_addr, end_addr);
    if (err != NI710AE_SUCCESS) {
        return err;
    }

    err = ni710ae_apu_set_br(dev, region, background);
    if (err != NI710AE_SUCCESS) {
        return err;
    }

    err = ni710ae_apu_set_id_valid(dev, region, id_valid);
    if (err != NI710AE_SUCCESS) {
        return err;
    }

    for (id_idx = 0; id_idx < 4; ++id_idx) {
        switch (id_idx)
        {
        case 0: id_select = NCI_ID_0_SELECT; break;
        case 1: id_select = NCI_ID_1_SELECT; break;
        case 2: id_select = NCI_ID_2_SELECT; break;
        case 3: id_select = NCI_ID_3_SELECT; break;
        }

        err = ni710ae_apu_set_access_perms(dev, region, permissions[id_idx],
                                           id_select);
        if (err != NI710AE_SUCCESS) {
            return err;
        }

        err = ni710ae_apu_assign_id(dev, region, entity_ids[id_idx],
                                      id_select);
        if (err != NI710AE_SUCCESS) {
            return err;
        }
    }

    if (region_enable = NCI_REGION_ENABLE) {
        err = ni710ae_apu_set_region_enable(dev, region);
        if (err != NI710AE_SUCCESS) {
            return 1;
        }
    }

    err = ni710ae_apu_set_lock(dev, region, lock);
    if (err != NI710AE_SUCCESS) {
        return 1;
    }
    return NI710AE_SUCCESS;
}

#define PRINT_NI710AE_TREE
#ifdef PRINT_NI710AE_TREE
void print_ni710ae(struct ni710ae_discovery_tree_t *node, int tab) {
    if(node == NULL)
        return;

    print(node->sibling, tab);

    for (int i = 0; i < tab; ++i) {
        printf("\t");
    }
    switch(node->type) {
        case NI710AE_CFGNI: printf("CFGNI"); break;
        case NI710AE_VD: printf("VD"); break;
        case NI710AE_PD: printf("PD"); break;
        case NI710AE_CD: printf("CD"); break;
        case NI710AE_ASNI: printf("ASNI"); break;
        case NI710AE_AMNI: printf("AMNI"); break;
        case NI710AE_PMU: printf("PMU"); break;
        case NI710AE_HSNI: printf("HSNI"); break;
        case NI710AE_HMNI: printf("HMNI"); break;
        case NI710AE_PMNI: printf("PMNI"); break;
        case NI710AE_FMU: printf("FMU"); break;
        case NI710AE_APU: printf("APU"); break;
        case NI710AE_SAM: printf("SAM"); break;
        case NI710AE_FCU: printf("FCU"); break;
        case NI710AE_IDM: printf("IDM"); break;
        case NI710AE_RAS: printf("RAS"); break;
    }
    printf("[%d] -> 0x%lx\r\n", (uint32_t)node->id, (uint32_t)node->address);

    print(node->child, tab+1);
}
#endif

/*
 * NI710AE Discovery
 */
enum ni710ae_err_t ni710ae_discovery(
                    struct ni710ae_discovery_tree_t *cfg_node,
                    const uint32_t periph_base,
                    uint8_t (*pruning_function)(uint16_t, uint16_t, uint16_t))
{
    uint16_t type = 0, id = 0;
    uint32_t address, child_count, c_idx = 0;
    uint32_t hdr_base = periph_base + cfg_node->address;

    struct ni710ae_discovery_tree_t* node;
    struct ni710ae_discovery_tree_t* sibling = NULL;

    enum ni710ae_err_t err;

    /* Fetch number of children */
    if (NI710AE_TYPE_IS_DOMAIN(cfg_node->type)) {
        child_count = ((ni710ae_domain_cfg_hdr *)(hdr_base))->child_node_info;
    } else if (NI710AE_TYPE_IS_COMPONENT(cfg_node->type)) {
        if (cfg_node == NI710AE_PMU) {
            /* Skipping because PMU doesn't have children */
            return NI710AE_SUCCESS;
        }
        child_count =
            ((ni710ae_component_cfg_hdr *)(hdr_base))->num_sub_features;
    } else if (NI710AE_TYPE_IS_SUBFEATURE(cfg_node->type)) {
        return NI710AE_SUCCESS;
    } else {
        return NI710AE_ERR_INVALID_ARG;
    }

    for(; c_idx < child_count; ++c_idx) {
        /* Skip discovering node based on the pruning function */
        if ((*pruning_function)(cfg_node->type, cfg_node->id, c_idx) != 0) {
            return NI710AE_SUCCESS;
        }

        if (NI710AE_TYPE_IS_DOMAIN(cfg_node->type)) {
            address = ((ni710ae_domain_cfg_hdr *)hdr_base)->x_pointers[c_idx];
            ni710ae_domain_cfg_hdr *child_hdr = periph_base + address;
            type = child_hdr->node_type & 0xFFFF;
            id = child_hdr->node_type >> 16;
        } else {
            ni710ae_component_cfg_hdr *child_hdr = periph_base +
                                                   cfg_node->address;
            address = child_hdr->sub_feature[c_idx].pointer;
            switch(child_hdr->sub_feature[c_idx].type) {
            case 0x0: type = NI710AE_APU; break;
            case 0x1: type = NI710AE_SAM; break;
            case 0x2: type = NI710AE_FCU; break;
            case 0x3: type = NI710AE_IDM; break;
            case 0x4: type = NI710AE_RAS; break;
            default: return NI710AE_ERR;
            }
            id = cfg_node->id;
        }
        node = calloc(sizeof(struct ni710ae_discovery_tree_t), 1);

        memcpy(node,
                &(struct ni710ae_discovery_tree_t) {
                .type = type,
                .id = id,
                .address = address,
                .sibling = sibling,
                }, sizeof(struct ni710ae_discovery_tree_t));
        cfg_node->child = node;
        cfg_node->children++;
        sibling = node;

        err = ni710ae_discovery(node, periph_base, pruning_function);
        if (err != NI710AE_SUCCESS) {
            return err;
        }
    }
#ifdef PRINT_NI710AE_TREE
    if(cfg_node->type == 0) print_ni710ae(cfg_node, 0);
#endif
    return NI710AE_SUCCESS;
}

uint32_t ni710ae_fetch_offset_address(struct ni710ae_discovery_tree_t *root,
                                      uint16_t component_type,
                                      uint16_t component_id,
                                      uint16_t sub_feature_type)
{
    struct ni710ae_discovery_tree_t *node;
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

    ret_addr = ni710ae_fetch_offset_address(root->child, component_type,
                                            component_id, sub_feature_type);
    if (ret_addr != -1) {
        return ret_addr;
    }

    return ni710ae_fetch_offset_address(root->sibling, component_type,
                                        component_id, sub_feature_type);
}

void free_ni710ae_discovery_tree(struct ni710ae_discovery_tree_t* tree) {
    if(tree == NULL)
        return;

    free_ni710ae_discovery_tree(tree->child);
    free_ni710ae_discovery_tree(tree->sibling);
    free(tree);
}

#undef APU_REGION_SELECT
