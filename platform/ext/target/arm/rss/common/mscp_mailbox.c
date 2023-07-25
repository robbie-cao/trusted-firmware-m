/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "mscp_mailbox.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void _set_mailbox_status(struct shared_mailbox *mailbox)
{
    mailbox->status = (1U << MSCP_MAILBOX_STATUS_FREE_POS);
}

void _clear_mailbox_status(struct shared_mailbox *mailbox)
{
    mailbox->status &= ~MSCP_MAILBOX_STATUS_FREE_MASK;
}

enum mscp_mailbox_error_t mailbox_init(struct mailbox_info *mscp_mailbox_info)
{
    struct shared_mailbox *mailbox =
        (struct shared_mailbox *)mscp_mailbox_info->mailbox_base;

    /* Only the completer should initialize the shared mailbox. */
    if ((mailbox == NULL) || (mscp_mailbox_info->type !=
        MAILBOX_TYPE_COMPLETER)) {
        return MSCP_MAILBOX_ERR_INVALID_PARAM;
    }

    /* Clear the shared mailbox */
    memset((void*)mailbox, 0, mscp_mailbox_info->mailbox_size);

    /*
     * Initialize mailbox such that the requester has ownership.
     * The requester has the mailbox ownership when the status bit is set.
     */
    _set_mailbox_status(mailbox);

    return MSCP_MAILBOX_ERR_NONE;
}

enum mscp_mailbox_error_t is_mailbox_available(
     struct mailbox_info *mscp_mailbox_info)
{
    struct shared_mailbox *mailbox =
        (struct shared_mailbox *)mscp_mailbox_info->mailbox_base;

    switch (mscp_mailbox_info->type) {
    case (MAILBOX_TYPE_REQUESTER):
        /* The mailbox is available to the requester if the status bit is 1 */
        if ((mailbox->status & MSCP_MAILBOX_STATUS_FREE_MASK) == 0) {
            return MSCP_MAILBOX_ERR_BUSY;
        }
        break;

    case (MAILBOX_TYPE_COMPLETER):
        /* The mailbox is available to the completer if the status bit is 0 */
        if (mailbox->status & MSCP_MAILBOX_STATUS_FREE_MASK) {
            return MSCP_MAILBOX_ERR_BUSY;
        }
        break;

    default:
        return MSCP_MAILBOX_ERR_INVALID_PARAM;
    }

    return MSCP_MAILBOX_ERR_NONE;
}

enum mscp_mailbox_error_t mailbox_read(struct mailbox_info *mscp_mailbox_info,
    size_t buffer_size, struct shared_mailbox *read_buffer)
{
    enum mscp_mailbox_error_t status;
    uint32_t payload_size;
    struct shared_mailbox *mailbox =
        (struct shared_mailbox *)mscp_mailbox_info->mailbox_base;

    /* Check if the mailbox is free */
    status = is_mailbox_available(mscp_mailbox_info);

    if (status != MSCP_MAILBOX_ERR_NONE) {
        return status;
    }

    /* Check if the read buffer is large enough */
    if (buffer_size < mscp_mailbox_info->mailbox_size) {
        return MSCP_MAILBOX_ERR_INVALID_BUFFER_SIZE;
    }

    /*
     * Verify that the length is at least as large as the message header.
     *
     * Note: the payload size is permitted to be of size zero.
     */
    if (mailbox->length < sizeof(mailbox->message_header)) {
        mailbox->status |= MSCP_MAILBOX_STATUS_ERROR_MASK;
        return MSCP_MAILBOX_ERR_INVALID_MSG_LENGTH;
    }

    if (read_buffer == NULL || read_buffer->payload == NULL) {
        return MSCP_MAILBOX_ERR_INVALID_PARAM;
    }

    /* Copy the mailbox contents to the read buffer excluding the payload */
    memcpy(read_buffer, mailbox, sizeof(struct shared_mailbox));

    /* Calculate payload size */
    payload_size = mailbox->length - sizeof(mailbox->message_header);

    /* Check if there's payload to be read */
    if (payload_size != 0) {
        /* Check read buffer payload size */
        size_t read_buffer_payload_size = (buffer_size -
            sizeof(struct shared_mailbox));

        if (payload_size > read_buffer_payload_size) {
            return MSCP_MAILBOX_ERR_INVALID_BUFFER_SIZE;
        }

        /* Copy payload from shared memory to read buffer */
        memcpy(read_buffer->payload, mailbox->payload, payload_size);
    }

    return MSCP_MAILBOX_ERR_NONE;
}

enum mscp_mailbox_error_t mailbox_write(
    struct mailbox_info *mscp_mailbox_info,
    uint32_t header, uint32_t *payload,
    uint32_t payload_size, bool request_ack_by_interrupt)
{
    enum mscp_mailbox_error_t status;
    size_t mailbox_size, mailbox_payload_size;
    struct shared_mailbox *mailbox =
        (struct shared_mailbox *)mscp_mailbox_info->mailbox_base;

    /* Check if the mailbox is free */
    status = is_mailbox_available(mscp_mailbox_info);

    if (status != MSCP_MAILBOX_ERR_NONE) {
        return status;
    }

    /* Read the shared mailbox size */
    mailbox_size = mscp_mailbox_info->mailbox_size;

    /* Clear the mailbox */
    memset((void*)mailbox, 0, mscp_mailbox_info->mailbox_size);

    /* Copy message header */
    mailbox->message_header = header;

    /*
     * If acknowledgement is required for this message, set the
     * corresponding flag.
     */
    if (request_ack_by_interrupt) {
        mailbox->flags |= MSCP_MAILBOX_FLAGS_IENABLED_MASK;
    }

    /* Copy the payload */
    if ((payload != NULL) && (payload_size != 0)) {
        mailbox_payload_size =
            (mailbox_size - sizeof(struct shared_mailbox));

        /* Payload size exceeds the mailbox payload size */
        if (payload_size > mailbox_payload_size) {
            return MSCP_MAILBOX_ERR_INVALID_MSG_LENGTH;
        }

        memcpy(mailbox->payload, payload, payload_size);
    }

    /* Write the total length of the message header and the payload */
    mailbox->length = (volatile uint32_t)(sizeof(mailbox->message_header) +
        payload_size);

    if (mscp_mailbox_info->type == MAILBOX_TYPE_REQUESTER) {
        /*
         * Clear the mailbox status bit so that the completer can read
         * the message.
         */
        _clear_mailbox_status(mailbox);
    } else {
        /*
         * Set the mailbox status bit so that the requester can read
         * the response.
         */
        _set_mailbox_status(mailbox);
    }

    return MSCP_MAILBOX_ERR_NONE;
}
