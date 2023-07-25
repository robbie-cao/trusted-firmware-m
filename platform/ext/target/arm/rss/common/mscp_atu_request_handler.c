/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "mscp_atu_request_handler.h"
#include "atu_rss_drv.h"
#include "mscp_mailbox.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Buffer used to store the contents of the shared mailbox */
struct shared_mailbox *mailbox_buffer;

/* Pointer to the mailbox info */
struct mailbox_info *mscp_mailbox_info;

enum mscp_atu_req_error_t atu_msg_respond(
        uint32_t msg_header, uint32_t *payload, size_t payload_size)
{
    enum mscp_mailbox_error_t status;
    /*
     * Set request_ack flag to false as acknowledgement is not required for
     * response messages.
     */
    bool request_ack = false;

    /* Write the response back to shared mailbox */
    status = mailbox_write(mscp_mailbox_info, msg_header, payload,
            payload_size, request_ack);

    /* Free the memory that was allocated for mailbox buffer */
    free(mailbox_buffer);

    if (status != MSCP_MAILBOX_ERR_NONE) {
        return MSCP_ATU_REQ_ERR_MAILBOX_WRITE_ERROR;
    }

    return MSCP_ATU_REQ_ERR_NONE;
}

enum mscp_atu_req_error_t set_atu_region_attributes(
        struct atu_dev_t *atu_dev, uint32_t attributes, uint8_t region_idx)
{
    enum atu_error_t status;
    enum atu_roba_t roba_value;

    roba_value = ATU_PAYLOAD_DECODE_AXNSE(attributes);
    status = set_axnsc(atu_dev, roba_value, region_idx);
    if (status != ATU_ERR_NONE) {
        return MSCP_ATU_REQ_ERR_INVALID_PARAM;
    }

    roba_value = ATU_PAYLOAD_DECODE_AXCACHE3(attributes);
    status = set_axcache3(atu_dev, roba_value, region_idx);
    if (status != ATU_ERR_NONE) {
        return MSCP_ATU_REQ_ERR_INVALID_PARAM;
    }

    roba_value = ATU_PAYLOAD_DECODE_AXCACHE2(attributes);
    status = set_axcache2(atu_dev, roba_value, region_idx);
    if (status != ATU_ERR_NONE) {
        return MSCP_ATU_REQ_ERR_INVALID_PARAM;
    }

    roba_value = ATU_PAYLOAD_DECODE_AXCACHE1(attributes);
    status = set_axcache1(atu_dev, roba_value, region_idx);
    if (status != ATU_ERR_NONE) {
        return MSCP_ATU_REQ_ERR_INVALID_PARAM;
    }

    roba_value = ATU_PAYLOAD_DECODE_AXCACHE0(attributes);
    status = set_axcache0(atu_dev, roba_value, region_idx);
    if (status != ATU_ERR_NONE) {
        return MSCP_ATU_REQ_ERR_INVALID_PARAM;
    }

    roba_value = ATU_PAYLOAD_DECODE_AXPROT2(attributes);
    status = set_axprot2(atu_dev, roba_value, region_idx);
    if (status != ATU_ERR_NONE) {
        return MSCP_ATU_REQ_ERR_INVALID_PARAM;
    }

    roba_value = ATU_PAYLOAD_DECODE_AXPROT1(attributes);
    status = set_axprot1(atu_dev, roba_value, region_idx);
    if (status != ATU_ERR_NONE) {
        return MSCP_ATU_REQ_ERR_INVALID_PARAM;
    }

    roba_value = ATU_PAYLOAD_DECODE_AXPROT0(attributes);
    status = set_axprot0(atu_dev, roba_value, region_idx);
    if (status != ATU_ERR_NONE) {
        return MSCP_ATU_REQ_ERR_INVALID_PARAM;
    }

    return MSCP_ATU_REQ_ERR_NONE;
}

enum mscp_atu_req_error_t mscp_check_atu_perm(struct atu_dev_info *atu_info,
    uint64_t start_addr, uint64_t end_addr, enum atu_region_pas requested_pas)
{
    enum atu_region_pas allowed_pas;

    /* Iterate through the allowed regions list */
    for (uint8_t idx = 0; idx < atu_info->allowed_regions_count; idx++) {
        /* Check if the start address falls within an allowed region */
        if (start_addr >= atu_info->allowed_regions_list[idx].start_addr &&
            (start_addr <= atu_info->allowed_regions_list[idx].end_addr)) {
                /* Check if the end address also falls within the range */
                if (end_addr > atu_info->allowed_regions_list[idx].end_addr) {
                    /* Requested region overlaps mutliple regions */
                    return MSCP_ATU_REQ_ERR_PERM_ERROR;
                }
                /* Get the allowed PAS for this region */
                allowed_pas = atu_info->allowed_regions_list[idx].allowed_pas;

                /* Check if the requested PAS is allowed */
                if (requested_pas != allowed_pas) {
                    return  MSCP_ATU_REQ_ERR_PERM_ERROR;
                }

                /* The requested region is allowed */
                return MSCP_ATU_REQ_ERR_NONE;
            }
    }

    /* Requested region is not allowed */
    return MSCP_ATU_REQ_ERR_PERM_ERROR;
}

enum mscp_atu_req_error_t atu_check_addr_overlap(
    uint64_t region_start,
    uint64_t region_end,
    uint64_t active_region_start,
    uint64_t active_region_end)
{
    /* Check if start address falls within the active region address range */
    if ((region_start >= active_region_start) &&
        (region_start <= active_region_end)) {
        return MSCP_ATU_REQ_ERR_OVERLAP_ERROR;
    }
    /*
     * The requested tranlation region does not overlap the active
     * translation region.
     */
    return MSCP_ATU_REQ_ERR_NONE;
}

enum mscp_atu_req_error_t validate_atu_region(
        struct atu_region_map *region_info,
        struct atu_dev_info *atu_info)
{
    struct active_atu_region *active_region;
    uint8_t i = 0, j = 0;
    enum mscp_atu_req_error_t status;

    /*
     * Iterate through active translation regions to check if there's any
     * overlap.
     */
    while (i < atu_info->active_regions_count &&
            j < atu_info->max_regions_count) {
        if (atu_info->active_regions_list[j].is_enabled) {
            active_region = &atu_info->active_regions_list[j];

            /* Check if the logical address range overlap */
            status = atu_check_addr_overlap(region_info->log_addr_base,
                    (region_info->log_addr_base + region_info->region_size - 1),
                    active_region->log_addr_start, active_region->log_addr_end);

            /*
             * Return error if the logical address overlaps an existing
             * translation region.
             */
            if (status != MSCP_ATU_REQ_ERR_NONE) {
                return status;
            }
            i++;
        }
        j++;
    }

    return MSCP_ATU_REQ_ERR_NONE;
}

enum mscp_atu_req_error_t process_atu_request_set_region(uint32_t *payload_base,
        struct atu_dev_info *atu_info)
{
    enum atu_error_t atu_drv_status;
    struct atu_region_map *atu_region_info;
    uint8_t region_idx;
    enum atu_region_pas requested_pas;
    unsigned int roba_count;
    struct atu_req_add_region_response response;
    enum mscp_atu_req_error_t status;

    /* Get the translation region info */
    atu_region_info = (struct atu_region_map *)payload_base;

    /*
     * Check if the logical address range overlaps with existing ATU regions.
     */
    atu_drv_status = validate_atu_region(atu_region_info, atu_info);
    if (atu_drv_status != ATU_ERR_NONE) {
        response.status = ATU_MSG_REQ_REGION_OVERLAP_ERROR;
        /* Respond with error */
        atu_msg_respond(mailbox_buffer->message_header, (uint32_t *)&response,
                        sizeof(response));
        return ATU_ERR_NONE;
    }

    requested_pas |=
        (ATU_PAYLOAD_DECODE_AXNSE(atu_region_info->attributes) << 0x2);
    requested_pas |= ATU_PAYLOAD_DECODE_AXPROT1(atu_region_info->attributes);

    /*
     * Get available ATU region index where the requested region can be mapped
     */
    atu_drv_status = get_available_region_index(atu_info->atu_dev, &region_idx);
    if (atu_drv_status != ATU_ERR_NONE) {
        response.status = ATU_MSG_REQ_GENERAL_ERROR;
        /* Respond with error */
        atu_msg_respond(mailbox_buffer->message_header, (uint32_t *)&response,
                        sizeof(response));
        return ATU_ERR_NONE;
    }

    /* Check if the requested region is allowed to be mapped */
    status = mscp_check_atu_perm(atu_info, atu_region_info->phy_addr_base,
            (atu_region_info->phy_addr_base + atu_region_info->region_size - 1),
            requested_pas);
    if (status != MSCP_ATU_REQ_ERR_NONE) {
        /* Respond with error */
        response.status = ATU_MSG_REQ_REGION_NOT_ALLOWED_ERROR;
        atu_msg_respond(mailbox_buffer->message_header, (uint32_t *)&response,
                        sizeof(response));
        return ATU_ERR_NONE;
    }

    /* Configure the ATU region output bus attributes */
    set_atu_region_attributes(
            atu_info->atu_dev, atu_region_info->attributes, region_idx);

    /* Invoke the ATU driver to initialize the requested region */
    atu_drv_status = atu_initialize_region(atu_info->atu_dev, region_idx,
            atu_region_info->log_addr_base,
            atu_region_info->phy_addr_base,
            atu_region_info->region_size);

    if (atu_drv_status == ATU_ERR_INVALID_ADDRESS) {
        /* Respond with error */
        response.status = ATU_MSG_REQ_REGION_INVALID_ADDRESS_ERROR;
        atu_msg_respond(mailbox_buffer->message_header, (uint32_t *)&response,
                        sizeof(response));
        return ATU_ERR_NONE;
    } else if (atu_drv_status != ATU_ERR_NONE) {
        /* Respond with error */
        response.status = ATU_MSG_REQ_GENERAL_ERROR;
        atu_msg_respond(mailbox_buffer->message_header, (uint32_t *)&response,
                        sizeof(response));
        return ATU_ERR_NONE;
    }

    /* Store the region info in the context */
    atu_info->active_regions_list[region_idx].region_owner_id =
        atu_region_info->region_owner_id;

    atu_info->active_regions_list[region_idx].log_addr_start =
        atu_region_info->log_addr_base;

    atu_info->active_regions_list[region_idx].log_addr_end =
        ((atu_region_info->log_addr_base + atu_region_info->region_size) - 1);

    atu_info->active_regions_list[region_idx].phy_addr_start =
        atu_region_info->phy_addr_base;

    atu_info->active_regions_list[region_idx].phy_addr_end =
        ((atu_region_info->phy_addr_base + atu_region_info->region_size) - 1);

    atu_info->active_regions_list[region_idx].is_enabled = true;

    atu_info->active_regions_count++;

    response.status = ATU_MSG_REQ_SUCCESS;
    response.region_idx = region_idx;

    return atu_msg_respond(mailbox_buffer->message_header,
            (uint32_t *)&response, sizeof(response));
}

enum mscp_atu_req_error_t process_atu_request_remove_region(
        uint32_t *msg_payload,
        struct atu_dev_info *atu_info)
{
    enum atu_error_t status;
    struct atu_req_remove_region_payload *payload =
        (struct atu_req_remove_region_payload *)msg_payload;
    struct atu_req_remove_region_response response;

    if (payload->region_idx >= atu_info->max_regions_count) {
        /* Respond with error */
        response.status = ATU_MSG_REQ_INVALID_REGION_ERROR;
        atu_msg_respond(mailbox_buffer->message_header, (uint32_t *)&response,
                        sizeof(response));
        return MSCP_ATU_REQ_ERR_NONE;
    }

    /* Check if the requester has ownership of the ATU region */
    if (atu_info->active_regions_list[payload->region_idx].region_owner_id !=
            payload->requester_id) {
        /* Respond with error */
        response.status = ATU_MSG_REQ_REGION_OWNERSHIP_ERROR;
        atu_msg_respond(mailbox_buffer->message_header, (uint32_t *)&response,
                        sizeof(response));
        return MSCP_ATU_REQ_ERR_NONE;
    }

    /* Invoke the ATU driver to remove the requested region */
    status = atu_uninitialize_region(atu_info->atu_dev, payload->region_idx);

    /* Fill the response payload based on the status */
    if (status != ATU_ERR_NONE) {
        /* Respond with error */
        response.status = ATU_MSG_REQ_GENERAL_ERROR;
        atu_msg_respond(mailbox_buffer->message_header, (uint32_t *)&response,
                        sizeof(response));
        return MSCP_ATU_REQ_ERR_NONE;
    }

    /* Clear the entry in the active atu regions list */
    memset((void *)&atu_info->active_regions_list[payload->region_idx], 0,
            sizeof(struct active_atu_region));

    if (atu_info->active_regions_count != 0) {
        atu_info->active_regions_count--;
    }

    response.status = ATU_MSG_REQ_SUCCESS;

    return atu_msg_respond(
            mailbox_buffer->message_header,
            (uint32_t *)&response,
            sizeof(response));
}

static int (*process_atu_request_vector[ATU_MSG_ID_REQ_COUNT])(
    uint32_t *, struct atu_dev_info *) = {
    [ATU_MSG_ID_REQ_ADD_REGION] = process_atu_request_set_region,
    [ATU_MSG_ID_REQ_REMOVE_REGION] = process_atu_request_remove_region,
};

enum mscp_atu_req_error_t atu_request_handler(struct atu_dev_info *atu_info)
{
    int32_t status;
    size_t mailbox_buffer_size, payload_size;
    struct atu_req_remove_region_response response;

    /* Check if the atu info structure has been initialized */
    if (!(atu_info->is_initialized)) {
        status = init_atu_request_handler(atu_info);
        if (status != MSCP_ATU_REQ_ERR_NONE) {
            return status;
        }
    }

    /* Store the mailbox info pointer in a global variable */
    mscp_mailbox_info = atu_info->mailbox_info;

    /* Allocate buffer based on the shared mailbox size */
    mailbox_buffer_size = mscp_mailbox_info->mailbox_size;
    mailbox_buffer = calloc(1, mailbox_buffer_size);

    /* Copy the message from shared mailbox to the buffer */
    status = mailbox_read(mscp_mailbox_info, mailbox_buffer_size,
            mailbox_buffer);

    /* Free the memory that was allocated in case of an error and return */
    if (status != MSCP_MAILBOX_ERR_NONE) {
        free(mailbox_buffer);
        return MSCP_ATU_REQ_ERR_MAILBOX_READ_ERROR;
    }

    /* Check payload size */
    payload_size = (mailbox_buffer->length -
            sizeof(mailbox_buffer->message_header));

    /* Payload size cannot be zero */
    if (payload_size == 0) {
        /* Respond with error */
        response.status = MSCP_ATU_REQ_ERR_INVALID_PAYLOAD;

        atu_msg_respond(
                mailbox_buffer->message_header,
                (uint32_t *)&response,
                sizeof(response));

        return MSCP_ATU_REQ_ERR_NONE;
    }

    /* Handle the ATU request */
    return process_atu_request_vector[mailbox_buffer->message_header](
            mailbox_buffer->payload,
            atu_info);
}

enum mscp_atu_req_error_t init_atu_request_handler(
        struct atu_dev_info *atu_info)
{
    uint8_t regions_count;

    if ((atu_info->allowed_regions_list == NULL) ||
            (atu_info->allowed_regions_count == 0) ||
            (atu_info->atu_dev == NULL)) {
        return MSCP_ATU_REQ_ERR_INVALID_PARAM;
    }

    if (atu_info->is_initialized) {
        return MSCP_MAILBOX_ERR_NONE;
    }

    /* Get the number of ATU regions supported by the ATU */
    regions_count = get_supported_region_count(atu_info->atu_dev);

    atu_info->max_regions_count = regions_count;

    /* Allocate memory for the active atu regions list */
    atu_info->active_regions_list =
        calloc(1, (regions_count * sizeof(struct active_atu_region)));

    atu_info->is_initialized = true;

    return MSCP_MAILBOX_ERR_NONE;
}
