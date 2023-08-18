/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __NI710AE_DRV_H__
#define __NI710AE_DRV_H__

#include "ni710ae_reg.h"
#include <stdint.h>

/**
 * \brief NI710AE error enumeration types
 */
enum ni710ae_err_t {
    NI710AE_SUCCESS,
    NI710AE_ERR_INVALID_ARG,
    NI710AE_ERR
};

/**
 * \brief NI710AE APU device structure
 */
struct ni710ae_apu_dev_t {
    const uintptr_t base;
};

/**
 * \brief NI710AE device structure
 */
struct ni710ae_dev_t {
    void *const base;
};

/**
 * \brief NI710AE Discovery tree structure
 */
struct ni710ae_discovery_tree_t {
    uint16_t type;
    uint16_t id;
    uint32_t address;

    uint32_t children;
    struct ni710ae_discovery_tree_t *child;
    struct ni710ae_discovery_tree_t *sibling;
};

/**
 * \brief NI710AE Discovery prune node structure
 */
struct ni710ae_prune_node_t {
    uint16_t type;
    uint16_t id;
};

/**
 * \brief NI710AE node type enumerations
 */
enum ni710ae_node_type_t {
    // Domains
    NI710AE_CFGNI,
    NI710AE_VD,
    NI710AE_PD,
    NI710AE_CD,
    // Components
    NI710AE_ASNI,
    NI710AE_AMNI,
    NI710AE_PMU,
    NI710AE_HSNI,
    NI710AE_HMNI,
    NI710AE_PMNI,
    NI710AE_FMU = 0x61,
    // Sub-features
    NI710AE_APU,
    NI710AE_SAM,
    NI710AE_FCU,
    NI710AE_IDM,
    NI710AE_RAS,
};

/**
 * \brief NI710AE APU background type enumerations
 */
enum ni710ae_apu_br_type_t {
    NCI_FOREGROUND = 0x0,
    NCI_BACKGROUND = 0x1
};

/**
 * \brief NI710AE APU access permission type enumerations
 */
enum ni710ae_apu_access_perm_type_t {
    /* As per spec */
    NCI_N_SEC_W = 0b00000001,
    NCI_SEC_W   = 0b00000010,
    NCI_N_SEC_R = 0b00000100,
    NCI_SEC_R   = 0b00001000,

    NCI_N_SEC_RW = NCI_N_SEC_R | NCI_N_SEC_W,
    NCI_SEC_RW   = NCI_SEC_R | NCI_SEC_W,

    NCI_ALL_PERM = NCI_N_SEC_RW | NCI_SEC_RW
};

/**
 * \brief NI710AE APU entity selection type enumerations
 */
enum ni710ae_apu_entity_type_t {
    /* program specific values */
    NCI_ID_0_SELECT = 0b0001,
    NCI_ID_1_SELECT = 0b0010,
    NCI_ID_2_SELECT = 0b0100,
    NCI_ID_3_SELECT = 0b1000,
    NCI_SELECT_ALL_IDS = NCI_ID_0_SELECT | NCI_ID_1_SELECT |
                         NCI_ID_2_SELECT | NCI_ID_3_SELECT
};

/**
 * \brief NI710AE APU entity valid type enumerations
 */
enum ni710ae_apu_entity_valid_type_t {
    /* program specific values */
    NCI_ID_0_VALID = 0b0001,
    NCI_ID_1_VALID = 0b0010,
    NCI_ID_2_VALID = 0b0100,
    NCI_ID_3_VALID = 0b1000,
    NCI_ID_VALID_ALL = NCI_ID_0_VALID | NCI_ID_1_VALID |
                       NCI_ID_2_VALID | NCI_ID_3_VALID,
    NCI_ID_VALID_NONE = 0,
};

/**
 * \brief NI710AE APU lcok type enumerations
 */
enum ni710ae_apu_lock_type_t {
    /* program specific values */
    NCI_UNLOCK = 0x0,
    NCI_LOCK = 0x1,
};

/**
 * \brief NI710AE APU region enable type enumerations
 */
enum ni710ae_apu_region_enable_type_t {
    /* program specific values */
    NCI_REGION_DISABLE = 0x0,
    NCI_REGION_ENABLE = 0x1,
};

/**
 * \brief Sets NI710AE APU address range,
 *
 * \param[in] dev           NI710AE APU device struct \ref ni710ae_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] base_addr     Base address of the region
 * \param[in] end_addr      End address of the region
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t ni710ae_apu_set_addr_range(struct ni710ae_apu_dev_t *dev,
                                              uint32_t region,
                                              uint64_t base_addr,
                                              uint64_t end_addr);

/**
 * \brief Sets NI710AE APU access permissions
 *
 * \param[in] dev           NI710AE APU device struct \ref ni710ae_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] permission    Or'ing of required access permissions
 *                          \ref ni710ae_apu_access_perm_type_t
 * \param[in] id_select     Or'ing of entity id's where these permission needs
 *                          to be applied \ref ni710ae_apu_entity_type_t
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t ni710ae_apu_set_access_perms(struct ni710ae_apu_dev_t *dev,
                                                uint32_t region,
                                                uint32_t permission,
                                                uint32_t id_select);

/**
 * \brief Sets NI710AE APU lock
 *
 * \param[in] dev           NI710AE APU device struct \ref ni710ae_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] lock          Lock or Unlock an APU region \ref ni710ae_apu_lock_type_t
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t ni710ae_apu_set_lock(struct ni710ae_apu_dev_t *dev,
                                        uint32_t region, uint32_t lock);

/**
 * \brief Sets NI710AE APU background/foreground type for a region
 *
 * \param[in] dev           NI710AE APU device struct \ref ni710ae_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] background    Specify if the region is backgroun or a foreground
 *                          region \ref ni710ae_apu_br_type_t
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t ni710ae_apu_set_br(struct ni710ae_apu_dev_t *dev,
                                      uint32_t region, uint32_t background);

/**
 * \brief Enables NI710AE APU region
 *
 * \param[in] dev           NI710AE APU device struct \ref ni710ae_apu_dev_t
 * \param[in] region        APU region number to be initialized
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t ni710ae_apu_set_region_enable(struct ni710ae_apu_dev_t *dev,
                                                 uint32_t region);

/**
 * \brief Sets NI710AE APU id_valid for a region
 *
 * \param[in] dev           NI710AE APU device struct \ref ni710ae_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] valid         Or'ing of entity id's where these ids are valid
 *                          \ref ni710ae_apu_entity_valid_type_t
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t ni710ae_apu_set_id_valid(struct ni710ae_apu_dev_t *dev,
                                            uint32_t region, uint32_t valid);

/**
 * \brief Enables NI710AE APU
 *
 * \param[in] dev           NI710AE APU device struct \ref ni710ae_apu_dev_t
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t ni710ae_apu_enable(struct ni710ae_apu_dev_t *dev);

/**
 * \brief Enables NI710AE APU SLVERR response.
 *
 * \param[in] dev           NI710AE APU device struct \ref ni710ae_apu_dev_t
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t ni710ae_apu_sync_err_enable(struct ni710ae_apu_dev_t *dev);

/**
 * \brief Assign custom NI710AE APU id values
 * \param[in] dev           NI710AE APU device struct \ref ni710ae_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] id_value      New ID value
 * \param[in] id_select     Specify enitity id number \ref ni710ae_apu_entity_type_t
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t ni710ae_apu_assign_id(struct ni710ae_apu_dev_t *dev,
                                         uint32_t region, uint32_t id_value,
                                         uint32_t id_select);

/**
 * \brief Initialize NI710AE APU
 *
 * \param[in] dev           NI710AE APU device struct \ref ni710ae_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] base_addr     Base address of the region
 * \param[in] end_addr      End address of the region
 * \param[in] background    Specify if the region is backgroun or a foreground
 *                          region \ref ni710ae_apu_br_type_t
 * \param[in] permissions   Array of Or'ed access permissions for all entity
 *                          \ref ni710ae_apu_access_perm_type_t
 * \param[in] entity_ids    Array of entity ids
 * \param[in] id_valid      Or'ing of entity id's where these ids are valid
 *                          \ref ni710ae_apu_entity_valid_type_t
 * \param[in] region_enable Specify if the region needs to be enabled or not.
 *                          \ref ni710ae_apu_region_enable_type_t
 * \param[in] lock          Lock or Unlock an APU region \ref ni710ae_apu_lock_type_t
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t ni710ae_apu_initialize_region(
                        struct ni710ae_apu_dev_t *dev, uint32_t region,
                        uint64_t base_addr, uint64_t end_addr,
                        enum ni710ae_apu_br_type_t background,
                        uint32_t permissions[4], uint8_t entity_ids[4],
                        uint32_t id_valid,
                        enum ni710ae_apu_region_enable_type_t region_enable,
                        enum ni710ae_apu_lock_type_t lock);

/**
 * \brief Executes NI710AE discovery flow
 *
 * \param[in] cfg_node      NI710AE Discovery struct \ref ni710ae_discovery_tree_t
 * \param[in] periph_base   NI710AE base address, same as CFGNI0 address
 * \param[in] pruning_function  Pointer to pruning callback function. Takes node type and
 *                              node id respectively. Should return 1 to skip the node
 *                              discovery, else 0
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
enum ni710ae_err_t
ni710ae_discovery(struct ni710ae_discovery_tree_t *cfg_node,
                  const uint32_t periph_base,
                  uint8_t (*pruning_function)(uint16_t, uint16_t, uint16_t));

/**
 * \brief Fetches sub-feature base address based on the parent component ID and
 *        type.
 *
 * \param[in] root              NI710AE Discovery struct root node
 *                              \ref ni710ae_discovery_tree_t
 * \param[in] component_type    Specify the parent component ID of the sub-feature
 *                              \ref ni710ae_node_type_t
 * \param[in] component_id      Specify the parent component type of the sub-feature
 * \param[in] sub_feature_type  Specify the type of the sub-feature
 *                              \ref ni710ae_node_type_t
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
uint32_t ni710ae_fetch_offset_address(struct ni710ae_discovery_tree_t *root,
                                      uint16_t component_type,
                                      uint16_t component_id,
                                      uint16_t sub_feature_type);

/**
 * \brief Free the discovery tree allocated memory
 *
 * \param[in] tree              NI710AE Discovery struct root node
 *                              \ref ni710ae_discovery_tree_t
 *
 * \return Returns error code as specified in \ref ni710ae_err_t
 */
void free_ni710ae_discovery_tree(struct ni710ae_discovery_tree_t* tree);

#endif /* __NI710AE_DRV_H__ */
