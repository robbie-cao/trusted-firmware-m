/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __TOWER_NCI_DRV_H__
#define __TOWER_NCI_DRV_H__

#include "tower_nci_reg.h"
#include <stdint.h>

/**
 * \brief Tower NCI error enumeration types
 */
enum tower_nci_err_t {
    TOWER_NCI_SUCCESS,
    TOWER_NCI_ERR_INVALID_ARG,
    TOWER_NCI_ERR
};

/**
 * \brief Tower NCI PSAM device structure
 */
struct tower_nci_psam_dev_t {
    const uintptr_t base;
};

/**
 * \brief Tower NCI APU device structure
 */
struct tower_nci_apu_dev_t {
    const uintptr_t base;
};

/**
 * \brief Tower NCI device structure
 */
struct tower_nci_dev_t {
    void *const base;
};

/*  Tower NCI Discovery tree structure */
struct tower_nci_discovery_tree_t {
    uint16_t type;
    uint16_t id;
    uint32_t address;

    uint32_t children;
    struct tower_nci_discovery_tree_t *child;
    struct tower_nci_discovery_tree_t *sibling;
};

/**
 * \brief Tower NCI Discovery prune node structure
 */
struct tower_nci_prune_node_t {
    uint16_t type;
    uint16_t id;
};

/**
 * \brief Tower NCI node type enumerations
 */
enum tower_nci_node_type_t {
    // Domains
    TOWER_NCI_CFGNI,
    TOWER_NCI_VD,
    TOWER_NCI_PD,
    TOWER_NCI_CD,
    // Components
    TOWER_NCI_ASNI,
    TOWER_NCI_AMNI,
    TOWER_NCI_PMU,
    TOWER_NCI_HSNI,
    TOWER_NCI_HMNI,
    TOWER_NCI_PMNI,
    // Sub-features - Implementation defined
    TOWER_NCI_PSAM,
    TOWER_NCI_APU,
    TOWER_NCI_FCU,
    TOWER_NCI_IDM,
    // FMU - Implementation defined
    TOWER_NCI_FMU
};

/**
 * \brief Tower NCI APU background type enumerations
 */
enum tower_nci_apu_br_type_t {
    T_NCI_FOREGROUND = 0x0,
    T_NCI_BACKGROUND = 0x1
};

/**
 * \brief Tower NCI APU access permission type enumerations
 */
enum tower_nci_apu_access_perm_type_t {
    /* As per spec */
    T_NCI_N_SEC_W = 0b00000001,
    T_NCI_SEC_W   = 0b00000010,
    T_NCI_N_SEC_R = 0b00000100,
    T_NCI_SEC_R   = 0b00001000,
    T_NCI_REALM_W = 0b00010000,
    T_NCI_ROOT_W  = 0b00100000,
    T_NCI_REALM_R = 0b01000000,
    T_NCI_ROOT_R  = 0b10000000,

    T_NCI_N_SEC_RW = T_NCI_N_SEC_R | T_NCI_N_SEC_W,
    T_NCI_SEC_RW   = T_NCI_SEC_R | T_NCI_SEC_W,
    T_NCI_REALM_RW = T_NCI_REALM_R | T_NCI_REALM_W,
    T_NCI_ROOT_RW  = T_NCI_ROOT_R | T_NCI_ROOT_W,

    T_NCI_ALL_PERM = T_NCI_N_SEC_RW | T_NCI_SEC_RW | T_NCI_REALM_RW |
                     T_NCI_ROOT_RW
};

/**
 * \brief Tower NCI APU entity selection type enumerations
 */
enum tower_nci_apu_entity_type_t {
    /* program specific values */
    T_NCI_ID_0_SELECT = 0b0001,
    T_NCI_ID_1_SELECT = 0b0010,
    T_NCI_ID_2_SELECT = 0b0100,
    T_NCI_ID_3_SELECT = 0b1000,
    T_NCI_SELECT_ALL_IDS = T_NCI_ID_0_SELECT | T_NCI_ID_1_SELECT |
                           T_NCI_ID_2_SELECT | T_NCI_ID_3_SELECT
};

/**
 * \brief Tower NCI APU entity valid type enumerations
 */
enum tower_nci_apu_entity_valid_type_t {
    /* program specific values */
    T_NCI_ID_0_VALID = 0b0001,
    T_NCI_ID_1_VALID = 0b0010,
    T_NCI_ID_2_VALID = 0b0100,
    T_NCI_ID_3_VALID = 0b1000,
    T_NCI_ID_VALID_ALL = T_NCI_ID_0_VALID | T_NCI_ID_1_VALID |
                         T_NCI_ID_2_VALID | T_NCI_ID_3_VALID,
    T_NCI_ID_VALID_NONE = 0,
};

/**
 * \brief Tower NCI APU lock type enumerations
 */
enum tower_nci_apu_lock_type_t {
    /* program specific values */
    T_NCI_UNLOCK = 0x0,
    T_NCI_LOCK = 0x1,
};

/**
 * \brief Tower NCI APU region enable type enumerations
 */
enum tower_nci_apu_region_enable_type_t {
    /* program specific values */
    T_NCI_REGION_DISABLE = 0x0,
    T_NCI_REGION_ENABLE = 0x1,
};

/**
 * \brief Initialize non-stripped region.
 *
 * \param[in] dev           Tower NCI PSAM device struct \ref tower_nci_psam_dev_t
 * \param[in] base_addr     Base address of the region
 * \param[in] end_addr      End address of the region
 * \param[in] target_id     ID of the target interface or the strip group for
 *                          non-striped region.
 * \param[in] region        PSAM region number to be initialized
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_psam_nhregion_init(
                        struct tower_nci_psam_dev_t *dev, uint64_t base_addr,
                        uint64_t end_addr, uint64_t target_id, uint64_t region);

/**
 * \brief Sets ID for default target interface for the address map
 * \param[in] dev           Tower NCI PSAM device struct \ref tower_nci_psam_dev_t
 * \param[in] target_id     ID of the target interface
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_psam_set_default_tgtid(
                        struct tower_nci_psam_dev_t *dev, uint32_t tgtid);

/**
 * \brief Enables the PSAM device
 *
 * \param[in] dev           Tower NCI PSAM device struct \ref tower_nci_psam_dev_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_psam_enable(struct tower_nci_psam_dev_t *dev);

/**
 * \brief Disables the PSAM device
 *
 * \param[in] dev           Tower NCI PSAM device struct \ref tower_nci_psam_dev_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_psam_disable(struct tower_nci_psam_dev_t *dev);

/**
 * \brief Sets Tower NCI APU address range,
 *
 * \param[in] dev           Tower NCI APU device struct \ref tower_nci_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] base_addr     Base address of the region
 * \param[in] end_addr      End address of the region
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_apu_set_addr_range(
                        struct tower_nci_apu_dev_t *dev, uint32_t region,
                        uint64_t base_addr, uint64_t end_addr);

/**
 * \brief Sets Tower NCI APU access permissions
 *
 * \param[in] dev           Tower NCI APU device struct \ref tower_nci_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] permission    Or'ing of required access permissions
 *                          \ref tower_nci_apu_access_perm_type_t
 * \param[in] id_select     Or'ing of entity id's where these permission needs
 *                          to be applied \ref tower_nci_apu_entity_type_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_apu_set_access_perms(
                        struct tower_nci_apu_dev_t *dev, uint32_t region,
                        uint32_t permission, uint32_t id_select);

/**
 * \brief Sets Tower NCI APU lock
 *
 * \param[in] dev           Tower NCI APU device struct \ref tower_nci_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] lock          Lock or Unlock an APU region \ref tower_nci_apu_lock_type_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_apu_set_lock(struct tower_nci_apu_dev_t *dev,
                                            uint32_t region, uint32_t lock);

/**
 * \brief Sets Tower NCI APU background/foreground type for a region
 *
 * \param[in] dev           Tower NCI APU device struct \ref tower_nci_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] background    Specify if the region is backgroun or a foreground
 *                          region \ref tower_nci_apu_br_type_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_apu_set_br(struct tower_nci_apu_dev_t *dev,
                                          uint32_t region, uint32_t background);

/**
 * \brief Enables Tower NCI APU region
 *
 * \param[in] dev           Tower NCI APU device struct \ref tower_nci_apu_dev_t
 * \param[in] region        APU region number to be initialized
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_apu_set_region_enable(
                        struct tower_nci_apu_dev_t *dev, uint32_t region);

/**
 * \brief Sets Tower NCI APU id_valid for a region
 *
 * \param[in] dev           Tower NCI APU device struct \ref tower_nci_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] valid         Or'ing of entity id's where these ids are valid
 *                          \ref tower_nci_apu_entity_valid_type_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_apu_set_id_valid(struct tower_nci_apu_dev_t *dev,
                                                uint32_t region,
                                                uint32_t valid);

/**
 * \brief Enables Tower NCI APU
 *
 * \param[in] dev           Tower NCI APU device struct \ref tower_nci_apu_dev_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_apu_enable(struct tower_nci_apu_dev_t *dev);

/**
 * \brief Enables Tower NCI APU SLVERR response.
 *
 * \param[in] dev           Tower NCI APU device struct \ref tower_nci_apu_dev_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_apu_sync_err_enable(
                            struct tower_nci_apu_dev_t *dev);

/**
 * \brief Assign custom Tower NCI APU id values
 * \param[in] dev           Tower NCI APU device struct \ref tower_nci_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] id_value      New ID value
 * \param[in] id_select     Specify enitity id number \ref tower_nci_apu_entity_type_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_apu_assign_id(struct tower_nci_apu_dev_t *dev,
                        uint32_t region, uint32_t id_value, uint32_t id_select);

/**
 * \brief Initialize Tower NCI APU
 *
 * \param[in] dev           Tower NCI APU device struct \ref tower_nci_apu_dev_t
 * \param[in] region        APU region number to be initialized
 * \param[in] base_addr     Base address of the region
 * \param[in] end_addr      End address of the region
 * \param[in] background    Specify if the region is backgroun or a foreground
 *                          region \ref tower_nci_apu_br_type_t
 * \param[in] permissions   Array of Or'ed access permissions for all entity
 *                          \ref tower_nci_apu_access_perm_type_t
 * \param[in] entity_ids    Array of entity ids
 * \param[in] id_valid      Or'ing of entity id's where these ids are valid
 *                          \ref tower_nci_apu_entity_valid_type_t
 * \param[in] region_enable Specify if the region needs to be enabled or not.
 *                          \ref tower_nci_apu_region_enable_type_t
 * \param[in] lock          Lock or Unlock an APU region \ref tower_nci_apu_lock_type_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_apu_initialize_region(
                        struct tower_nci_apu_dev_t *dev, uint32_t region,
                        uint64_t base_addr, uint64_t end_addr,
                        enum tower_nci_apu_br_type_t background,
                        uint32_t permissions[4], uint8_t entity_ids[4],
                        uint32_t id_valid,
                        enum tower_nci_apu_region_enable_type_t region_enable,
                        enum tower_nci_apu_lock_type_t lock);

/**
 * \brief Executes Tower NCI discovery flow
 *
 * \param[in] cfg_node          Tower NCI Discovery struct \ref tower_nci_discovery_tree_t
 * \param[in] periph_base       Tower NCI base address, same as CFGNI0 address
 * \param[in] pruning_function  Pointer to pruning callback function. Takes node type and
 *                              node id respectively. Should return 1 to skip the node
 *                              discovery, else 0
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
enum tower_nci_err_t tower_nci_discovery(
                    struct tower_nci_discovery_tree_t *cfg_node,
                    const uint32_t periph_base,
                    uint8_t (*pruning_function)(uint16_t, uint16_t, uint16_t));

/**
 * \brief Fetches sub-feature base address based on the parent component ID and
 *        type.
 *
 * \param[in] root              Tower NCI Discovery struct root node
 *                              \ref tower_nci_discovery_tree_t
 * \param[in] component_type    Specify the parent component ID of the sub-feature
 *                              \ref tower_nci_node_type_t
 * \param[in] component_id      Specify the parent component type of the sub-feature
 * \param[in] sub_feature_type  Specify the type of the sub-feature
 *                              \ref tower_nci_node_type_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
uint32_t tower_nci_fetch_offset_address(struct tower_nci_discovery_tree_t *root,
                                        uint16_t component_type,
                                        uint16_t component_id,
                                        uint16_t sub_feature_type);

/**
 * \brief Free the discovery tree allocated memory
 *
 * \param[in] tree              Tower NCI Discovery struct root node
 *                              \ref tower_nci_discovery_tree_t
 *
 * \return Returns error code as specified in \ref tower_nci_err_t
 */
void free_discovery_tree(struct tower_nci_discovery_tree_t* tree);

#endif /* __TOWER_NCI_DRV_H__ */
