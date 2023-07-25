/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __MSCP_MAILBOX_H__
#define __MSCP_MAILBOX_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Macros related to mailbox status field and flags.
 */
#define MSCP_MAILBOX_STATUS_FREE_POS (0)
#define MSCP_MAILBOX_STATUS_FREE_MASK \
    (UINT32_C(0x1) << MSCP_MAILBOX_STATUS_FREE_POS)

#define MSCP_MAILBOX_FLAGS_IENABLED_POS (0)
#define MSCP_MAILBOX_FLAGS_IENABLED_MASK \
    (UINT32_C(0x1) << MSCP_MAILBOX_FLAGS_IENABLED_POS)

#define MSCP_MAILBOX_STATUS_ERROR_POS (1)
#define MSCP_MAILBOX_STATUS_ERROR_MASK \
    (UINT32_C(0x1) << MSCP_MAILBOX_STATUS_ERROR_POS)

/*
 * Shared mailbox area layout.
 *
 * Refer section 5.1.2 Shared memory area layout in the Arm System Control
 * and Management Interface Platform Design Document v3.2
 */
struct shared_mailbox {
    /* Reserved field, must be zero */
    uint32_t reserved0;
    /* Channel status */
    volatile uint32_t status;
    /* Implementation defined field */
    uint64_t reserved1;
    /* Channel flags */
    uint32_t flags;
    /* Length in bytes of the message header and payload */
    volatile uint32_t length;
    /* Message header field */
    uint32_t message_header;
    /* Message payload */
    uint32_t payload[];
};

/*
 * MSCP mailbox error enumeration types.
 */
enum mscp_mailbox_error_t {
    /* No error */
    MSCP_MAILBOX_ERR_NONE =  0,
    /* Mailbox is not available */
    MSCP_MAILBOX_ERR_BUSY,
    /* Invalid parameter */
    MSCP_MAILBOX_ERR_INVALID_PARAM,
    /* Invalid message/payload length */
    MSCP_MAILBOX_ERR_INVALID_MSG_LENGTH,
    /* Read buffer size is smaller than the received message */
    MSCP_MAILBOX_ERR_INVALID_BUFFER_SIZE,
};

/*
 * Mailbox type.
 */
enum mailbox_type_t {
    /*
     * Requester type mailbox. RSS has the mailbox ownership when the mailbox
     * status bit is set.
     */
    MAILBOX_TYPE_REQUESTER = 0,
    /*
     * Completer type mailbox. RSS has the mailbox ownership when the mailbox
     * status bit is zero.
     */
    MAILBOX_TYPE_COMPLETER,
};

/*
 * Structure used to describe the shared mailbox.
 */
struct mailbox_info {
    /* Base address of the shared mailbox */
    uintptr_t mailbox_base;
    /* Mailbox type \ref mailbox_type_t */
    enum mailbox_type_t type;
    /* Size of the shared mailbox in bytes */
    size_t mailbox_size;
};

/**
 * \brief Initializes the mailbox such that the requester has ownership.
 *
 * \param[in] mailbox_info Shared mailbox info struct \ref mailbox_info
 *
 * \return Returns error code as specified in \ref mscp_mailbox_error_t
 */
enum mscp_mailbox_error_t mailbox_init(struct mailbox_info *mscp_mailbox_info);

/**
 * \brief Checks if the mailbox is available.
 *
 * \param[in] mailbox_info Shared mailbox info struct \ref mailbox_info
 *
 * \return Returns error code as specified in \ref mscp_mailbox_error_t
 */
enum mscp_mailbox_error_t is_mailbox_available(
        struct mailbox_info *mscp_mailbox_info);

/**
 * \brief Reads the shared mailbox.
 *
 * \param[in]  mailbox_info Shared mailbox info struct \ref mailbox_info
 * \param[in]  buffer_size  Size of the read_buffer in bytes.
 * \param[out] read_buffer  Pointer to the variable that will store the mailbox
 *                          contents.
 *
 * \return Returns error code as specified in \ref mscp_mailbox_error_t
 */
enum mscp_mailbox_error_t mailbox_read(struct mailbox_info *mscp_mailbox_info,
    size_t buffer_size, struct shared_mailbox *read_buffer);

/**
 * \brief Write message to the shared mailbox.
 *
 * \param[in] mailbox_info Shared mailbox info struct \ref mailbox_info
 * \param[in] header Message header.
 * \param[in] payload Pointer to the payload.
 * \param[in] payload_size Size of the payload in bytes.
 * \param[in] request_ack_by_interrupt Flag to indicate if acknowledgement is
 *                                     required.
 *
 * \return Returns error code as specified in \ref mscp_mailbox_error_t
 */
enum mscp_mailbox_error_t mailbox_write(
    struct mailbox_info *mscp_mailbox_info,
    uint32_t header, uint32_t *payload, uint32_t payload_size,
    bool request_ack_by_interrupt);

#ifdef __cplusplus
}
#endif

#endif /* __MSCP_MAILBOX_H__ */
