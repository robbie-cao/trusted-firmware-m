/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __MSCP_ATU_REQUEST_HANDLER_H__
#define __MSCP_ATU_REQUEST_HANDLER_H__

#include "atu_rss_drv.h"
#include "mscp_mailbox.h"

#include <stdint.h>

/* Mask for reading an attribute from ATU ROBA register */
#define ATU_ROBA_MASK (0x3u)

/*! ATU ROBA Register AxNSE bit field offset */
#define ATU_ATUROBA_AXNSE_OFFSET (14u)

/*! ATU ROBA Register AxCACHE3 bit field offset */
#define ATU_ATUROBA_AXCACHE3_OFFSET (12u)

/*! ATU ROBA Register AxCACHE2 bit field offset */
#define ATU_ATUROBA_AXCACHE2_OFFSET (10u)

/*! ATU ROBA Register AxCACHE1 bit field offset */
#define ATU_ATUROBA_AXCACHE1_OFFSET (8u)

/*! ATU ROBA Register AxCACHE0 bit field offset */
#define ATU_ATUROBA_AXCACHE0_OFFSET (6u)

/*! ATU ROBA Register AxPROT2 bit field offset */
#define ATU_ATUROBA_AXPROT2_OFFSET (4u)

/*! ATU ROBA Register AxPROT1 bit field offset */
#define ATU_ATUROBA_AXPROT1_OFFSET (2u)

/*! ATU ROBA Register AxPROT0 bit field offset */
#define ATU_ATUROBA_AXPROT0_OFFSET (0u)

#define ATU_PAYLOAD_DECODE_AXNSE(attribute) \
    ((attribute >> ATU_ATUROBA_AXNSE_OFFSET) & ATU_ROBA_MASK)

#define ATU_PAYLOAD_DECODE_AXCACHE3(attribute) \
    ((attribute >> ATU_ATUROBA_AXCACHE3_OFFSET) & ATU_ROBA_MASK)

#define ATU_PAYLOAD_DECODE_AXCACHE2(attribute) \
    ((attribute >> ATU_ATUROBA_AXCACHE2_OFFSET) & ATU_ROBA_MASK)

#define ATU_PAYLOAD_DECODE_AXCACHE1(attribute) \
    ((attribute >> ATU_ATUROBA_AXCACHE1_OFFSET) & ATU_ROBA_MASK)

#define ATU_PAYLOAD_DECODE_AXCACHE0(attribute) \
    ((attribute >> ATU_ATUROBA_AXCACHE0_OFFSET) & ATU_ROBA_MASK)

#define ATU_PAYLOAD_DECODE_AXPROT2(attribute) \
    ((attribute >> ATU_ATUROBA_AXPROT2_OFFSET) & ATU_ROBA_MASK)

#define ATU_PAYLOAD_DECODE_AXPROT1(attribute) \
    ((attribute >> ATU_ATUROBA_AXPROT1_OFFSET) & ATU_ROBA_MASK)

#define ATU_PAYLOAD_DECODE_AXPROT0(attribute) \
    ((attribute >> ATU_ATUROBA_AXPROT0_OFFSET) & ATU_ROBA_MASK)

/* Macro to denote that the RSS owns the ATU region */
#define ATU_REGION_OWNER_ID_RSS (UINT32_MAX)

/* MSCP ATU request handler error enumeration types */
enum mscp_atu_req_error_t {
    /* No error */
    MSCP_ATU_REQ_ERR_NONE = 0,
    /* Invalid parameter */
    MSCP_ATU_REQ_ERR_INVALID_PARAM,
    /* Shared mailbox read error */
    MSCP_ATU_REQ_ERR_MAILBOX_READ_ERROR,
    /* Shared mailbox write error */
    MSCP_ATU_REQ_ERR_MAILBOX_WRITE_ERROR,
    /* Invalid message payload */
    MSCP_ATU_REQ_ERR_INVALID_PAYLOAD,
    /* ATU permissions error */
    MSCP_ATU_REQ_ERR_PERM_ERROR,
    /* Logical address overlap with existng ATU region */
    MSCP_ATU_REQ_ERR_OVERLAP_ERROR,
};

/* ATU request message identifiers */
enum atu_req_msg_id {
    /* Request to map a translation region */
    ATU_MSG_ID_REQ_ADD_REGION = 0x1,
    /* Request to remove a translation region */
    ATU_MSG_ID_REQ_REMOVE_REGION,
    /* ATU message ID count */
    ATU_MSG_ID_REQ_COUNT,
};

/* ATU message response */
enum atu_msg_response {
    /* Requested operation successful */
    ATU_MSG_REQ_SUCCESS = 0x1,
    /* Requested operation failed */
    ATU_MSG_REQ_GENERAL_ERROR,
    /* Invalid ATU region number */
    ATU_MSG_REQ_INVALID_REGION_ERROR,
    /* Region not authorized */
    ATU_MSG_REQ_REGION_NOT_ALLOWED_ERROR,
    /* Requested translation region has invalid address range */
    ATU_MSG_REQ_REGION_INVALID_ADDRESS_ERROR,
    /* Invalid request */
    ATU_MSG_REQ_INVALID_REQUEST_ERROR,
    /* Invalid message payload */
    ATU_MSG_REQ_INVALID_PAYLOAD_ERROR,
    /* Region ownership error */
    ATU_MSG_REQ_REGION_OWNERSHIP_ERROR,
    /* Region overlaps with an existing ATU region */
    ATU_MSG_REQ_REGION_OVERLAP_ERROR,
    /* ATU message response count */
    ATU_MSG_REQ_COUNT,
};

/*
 * Address translation region info.
 *
 * Note: This structure is sent as the payload with or without bus attributes
 * by the MSCP firmware.
 */
struct __attribute((packed)) atu_region_map {
    uint32_t region_owner_id;
    /*! Start logical address of the region to be mapped */
    uint32_t log_addr_base;
    /*! Start physical address of the region to be mapped */
    uint64_t phy_addr_base;
    /*! Size of the region to be mapped */
    size_t region_size;
    /*! Output bus attributes for the ATU region */
    uint32_t attributes;
};

struct atu_req_remove_region_payload {
    uint32_t region_idx;
    uint32_t requester_id;
};

/*!
 * Response to be sent for \ref ATU_MSG_ID_REQ_ADD_REGION message
 */
struct atu_req_add_region_response {
    /*! Status of the requested operation */
    int32_t status;
    /*!
     * Index of the ATU region.
     * Note: Only valid when status is ATU_MSG_REQ_SUCCESS
     */
    uint32_t region_idx;
};

/*!
 * Response to be sent for \ref ATU_MSG_ID_REQ_REMOVE_REGION message
 */
struct atu_req_remove_region_response {
    /*! Status of the requested operation */
    int32_t status;
};

/*
 * ATU Region Output Bus Attributes(ROBA)
 */
enum mod_atu_roba {
    /*! AxPROT[0] attribute */
    MOD_ATU_ROBA_AXPROT0,
    /*! AxPROT[1] attribute */
    MOD_ATU_ROBA_AXPROT1,
    /*! AxPROT[2] attribute */
    MOD_ATU_ROBA_AXPROT2,
    /*! AxCACHE[0] attribute*/
    MOD_ATU_ROBA_AXCACHE0,
    /*! AxCACHE[1] attribute*/
    MOD_ATU_ROBA_AXCACHE1,
    /*! AxCACHE[2] attribute*/
    MOD_ATU_ROBA_AXCACHE2,
    /*! AxCACHE[3] attribute*/
    MOD_ATU_ROBA_AXCACHE3,
    /*! AxNSE attribute */
    MOD_ATU_ROBA_AXNSE,
    /*! Output bus attribute count */
    MOD_ATU_ROBA_COUNT,
};

/*
 * Physical Address Space(PAS) of the ATU region
 */
enum atu_region_pas {
    /* Secure PAS - AxNSE = 2b10 and AxPROT[1] = 2b10 */
    ATU_REGION_PAS_SECURE = 10,
    /* Non-Secure PAS - AxNSE = 2b10 and AxPROT[1] = 2b11 */
    ATU_REGION_PAS_NON_SECURE = 11,
    /* Root PAS - AxNSE = 2b11 and AxPROT[1] = 2b00 */
    ATU_REGION_PAS_ROOT = 14,
    /* Realm PAS - AxNSE = 2b11 and AxPROT[1] = 2b11 */
    ATU_REGION_PAS_REALM = 15,
};

/*
 * ATU region permissions. This structure is used to define a ATU region with
 * permissions corresponding to a physical address range.
 */
struct atu_region {
    /* Start physical address */
    uint64_t start_addr;
    /* End physical address */
    uint64_t end_addr;
    /* Allowed PAS for the region */
    enum atu_region_pas allowed_pas;
};

struct active_atu_region {
    uint32_t region_owner_id;
    uint32_t log_addr_start;
    uint32_t log_addr_end;
    uint64_t phy_addr_start;
    uint64_t phy_addr_end;
    bool is_enabled;
};

struct atu_dev_info {
    /* Pointer to the ATU device structure */
    struct atu_dev_t *atu_dev;
    /* List of allowed ATU regions */
    struct atu_region *allowed_regions_list;
    /* Number of regions in the allowed_regions_list */
    uint8_t allowed_regions_count;
    /* List of active ATU regions */
    struct active_atu_region *active_regions_list;
    /* Number of regions in the active_regions_list */
    uint8_t active_regions_count;
    /* Pointer to the mailbox info struct */
    struct mailbox_info *mailbox_info;
    /* Maximum regions supported by the ATU */
    uint8_t max_regions_count;
    /* Flag to indicate if the atu device info structure is initialized */
    bool is_initialized;
};

/**
 * \brief Initialize the ATU device info context structure.
 *
 * \param[in] atu_info ATU device info struct \ref atu_dev_info
 *
 * \return Returns error code as specified in \ref mscp_atu_req_error_t
 */
enum mscp_atu_req_error_t init_atu_request_handler(
        struct atu_dev_info *atu_info);

/**
 * \brief Process ATU request from MSCP.
 *
 * \param[in] atu_info ATU device info struct \ref atu_dev_info
 *
 * \return Returns error code as specified in \ref mscp_atu_req_error_t
 */
enum mscp_atu_req_error_t atu_request_handler(struct atu_dev_info *atu_info);

#endif /* __MSCP_ATU_REQUEST_HANDLER_H__ */
