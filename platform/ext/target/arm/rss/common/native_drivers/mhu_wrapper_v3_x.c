/*
 * Copyright (c) 2023 Arm Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mhu_v3_x.h"
#include "mhu.h"
#include <string.h>

static enum mhu_error_t error_mapping_to_mhu_error_t(enum mhu_v3_x_error_t err)
{
    switch (err) {
    case MHU_V_3_X_ERR_NONE:
        return MHU_ERR_NONE;
    case MHU_V_3_X_ERR_NOT_INIT:
        return MHU_ERR_NOT_INIT;
    case MHU_V_3_X_ERR_UNSUPPORTED_VERSION:
        return MHU_ERR_UNSUPPORTED_VERSION;
    case MHU_V_3_X_ERR_UNSUPPORTED:
        return MHU_ERR_UNSUPPORTED;
    case MHU_V_3_X_ERR_INVALID_PARAM:
        return MHU_ERR_INVALID_ARG;
    default:
        return MHU_ERR_GENERAL;
    }
}

enum mhu_error_t mhu_init_sender(void *mhu_sender_dev)
{
    enum mhu_v3_x_error_t mhu_err;
    struct mhu_v3_x_dev_t *dev = mhu_sender_dev;
    uint8_t num_ch;
    uint32_t ch;

    if (dev == NULL) {
        return error_mapping_to_mhu_error_t(MHU_V_3_X_ERR_INVALID_PARAM);
    }
    /* Initialize MHUv3 */
    mhu_err = mhu_v3_x_driver_init(dev);
    if (mhu_err != MHU_V_3_X_ERR_NONE) {
        return error_mapping_to_mhu_error_t(mhu_err);
    }

    /* This wrapper requires at least 1 channel to be implemented */
    mhu_err = mhu_v3_x_get_num_channel_implemented(dev,
            MHU_V3_X_CHANNEL_TYPE_DBCH, &num_ch);
    if (mhu_err != MHU_V_3_X_ERR_NONE) {
        goto fail;
    } else if (num_ch == 0) {
        mhu_err = MHU_V_3_X_ERR_UNSUPPORTED;
        goto fail;
    }

    /*
     * The sender polls the postbox doorbell channel window status register to
     * get notified about successful transfer. So, disable the doorbell
     * channel's contribution to postbox combined interrupt.
     *
     * Also, clear and disable the postbox doorbell channel transfer acknowledge
     * interrupt.
     */
    for (ch = 0; ch < num_ch; ++ch) {
        mhu_err = mhu_v3_x_channel_interrupt_disable(dev, ch,
                MHU_V3_X_CHANNEL_TYPE_DBCH);
        if (mhu_err != MHU_V_3_X_ERR_NONE) {
            goto fail;
        }
    }

    return MHU_ERR_NONE;

fail:
    return error_mapping_to_mhu_error_t(mhu_err);
}

enum mhu_error_t mhu_init_receiver(void *mhu_receiver_dev)
{
    enum mhu_v3_x_error_t err;
    struct mhu_v3_x_dev_t *dev = mhu_receiver_dev;
    uint32_t ch;
    uint8_t num_ch;

    if (dev == NULL) {
        return error_mapping_to_mhu_error_t(MHU_V_3_X_ERR_INVALID_PARAM);
    }

    /* Initialize MHUv3 */
    err = mhu_v3_x_driver_init(dev);
    if (err != MHU_V_3_X_ERR_NONE) {
        return error_mapping_to_mhu_error_t(err);
    }

    /* This wrapper requires at least 1 channel to be implemented */
    err = mhu_v3_x_get_num_channel_implemented(dev, MHU_V3_X_CHANNEL_TYPE_DBCH,
            &num_ch);
    if (err != MHU_V_3_X_ERR_NONE) {
        goto fail;
    } else if (num_ch == 0) {
        err = MHU_V_3_X_ERR_UNSUPPORTED;
        goto fail;
    }

    for (ch = 0; ch < num_ch; ++ch) {
        /* Clear the doorbell channel */
        err = mhu_v3_x_doorbell_clear(dev, ch, UINT32_MAX);
        if (err != MHU_V_3_X_ERR_NONE) {
            goto fail;
        }

        /* Unmask doorbell channel interrupt */
        err = mhu_v3_x_doorbell_mask_clear(dev, ch, UINT32_MAX);
        if (err != MHU_V_3_X_ERR_NONE) {
            goto fail;
        }

        /*
         * Enable the doorbell channel's contribution to mailbox combined
         * interrupt.
         */
        err = mhu_v3_x_channel_interrupt_enable(dev, ch,
                MHU_V3_X_CHANNEL_TYPE_DBCH);
        if (err != MHU_V_3_X_ERR_NONE) {
            goto fail;
        }
    }

    return MHU_ERR_NONE;

fail:
    return error_mapping_to_mhu_error_t(err);
}

size_t mhu_get_max_message_size(void)
{
    /*
     * First 4 bytes represent the MHU msg header. Rest of buffer
     * holds the actual message.
     */
    return (MHU3_OUTBAND_BUF_SIZE - MHU3_OUTBAND_BUF_HEADER_SIZE);
}

static int validate_buffer_params(uintptr_t buf_addr, size_t buf_size)
{
    if ((buf_addr == 0) || (!IS_ALIGNED(buf_addr, 4)) ||
            buf_size > mhu_get_max_message_size()) {
        return MHU_V_3_X_ERR_INVALID_PARAM;
    }

    return MHU_V_3_X_ERR_NONE;
}

static enum mhu_v3_x_error_t signal_and_wait_for_clear(
        struct mhu_v3_x_dev_t *dev, uint32_t ch, uint32_t value)
{
    enum mhu_v3_x_error_t err = MHU_V_3_X_ERR_NONE;
    uint32_t read_val;

    /* Wait for any pending acknowledgement from transmitter side */
    do {
        err = mhu_v3_x_doorbell_read(dev, ch, &read_val);
        if (err != MHU_V_3_X_ERR_NONE) {
            goto fail;
        }
    } while ((read_val & value) == value);

    /* Trigger the doorbell */
    err = mhu_v3_x_doorbell_write(dev, ch, value);
    if (err != MHU_V_3_X_ERR_NONE) {
        goto fail;
    }

    /* Wait until transmitter side acknowledges the transfer */
    do {
        err = mhu_v3_x_doorbell_read(dev, ch, &read_val);
        if (err != MHU_V_3_X_ERR_NONE) {
            goto fail;
        }
    } while ((read_val & value) == value);

fail:
    return err;
}

static enum mhu_v3_x_error_t wait_for_signal_and_clear(
        struct mhu_v3_x_dev_t *dev, uint32_t ch, uint32_t value)
{
    enum mhu_v3_x_error_t err = MHU_V_3_X_ERR_NONE;
    uint32_t read_val;

    /* Wait on status register for transmitter side to send data */
    do {
        err = mhu_v3_x_doorbell_read(dev, ch, &read_val);
        if (err != MHU_V_3_X_ERR_NONE) {
            goto fail;
        }
    } while ((read_val & value) != value);

    /* Acknowledge the transfer and clear the doorbell register */
    err = mhu_v3_x_doorbell_clear(dev, ch, value);
    if (err != MHU_V_3_X_ERR_NONE) {
        goto fail;
    }

fail:
    return err;
}

enum mhu_error_t mhu_send_data(void *mhu_sender_dev,
        const uint8_t *send_buffer, size_t size)
{
    struct mhu_v3_x_dev_t *dev = mhu_sender_dev;
    uint32_t msg_len = (uint32_t)size;
    enum mhu_v3_x_error_t err;

    if (size == 0) {
        return error_mapping_to_mhu_error_t(MHU_V_3_X_ERR_NONE);
    }

    err = validate_buffer_params((uintptr_t)send_buffer, size);
    if (err != MHU_V_3_X_ERR_NONE) {
        goto fail;
    }

    /*
     * First 4 bytes represents the header which currently stores the size
     * of msg. Rest of buffer holds the actual message.
     */
    (void)memcpy((void *)MHU3_OUTBAND_BUF_BASE, (void *)&msg_len,
            MHU3_OUTBAND_BUF_HEADER_SIZE);

    /* Copy the message */
    (void)memcpy((void *)(MHU3_OUTBAND_BUF_BASE + MHU3_OUTBAND_BUF_HEADER_SIZE),
            (void *)send_buffer, msg_len);
    /*
     * Use only one channel 0 to ring the doorbell on receiver for
     * attestation report and measurements, while the actual data is
     * shared through a common memory region in Secure/Root PAS. After
     * receiver is done with this transaction, it needs to clear the flags
     * bits on the same channel or else transmitter will continue waiting
     * on the status bit indefinitely.
     */
    err = signal_and_wait_for_clear(dev, 0, MHU3_PBX_DBCH_FLAG_AP_COMMS);
    if (err != MHU_V_3_X_ERR_NONE)
        goto fail;

    return MHU_V_3_X_ERR_NONE;

fail:

    return error_mapping_to_mhu_error_t(err);
}

enum mhu_error_t mhu_receive_data(void *mhu_receiver_dev,
        uint8_t *receive_buffer, size_t *size)
{
    struct mhu_v3_x_dev_t *dev = mhu_receiver_dev;
    uint32_t msg_len;
    enum mhu_v3_x_error_t err;

    err = validate_buffer_params((uintptr_t)receive_buffer, *size);
    if (err != MHU_V_3_X_ERR_NONE) {
        goto fail;
    }

    /*
     * Use only one channel 0 to receive reply from transmitter for
     * attestation report and measurements, while the actual data is shared
     * through a common memory region in Secure/Root PAS.
     */
    err = wait_for_signal_and_clear(dev, 0, MHU3_PBX_DBCH_FLAG_AP_COMMS);
    if (err != MHU_V_3_X_ERR_NONE) {
        goto fail;
    }

    /*
     * First 4 bytes represents the header which currently stores the size
     * of msg. Rest of buffer holds the actual message.
     */
    (void)memcpy((void *)&msg_len, (void *)MHU3_OUTBAND_BUF_BASE,
            MHU3_OUTBAND_BUF_HEADER_SIZE);
    if (*size < msg_len) {
        return MHU_V_3_X_ERR_UNSUPPORTED;
    }

    *size = msg_len;

    /* Copy the message */
    (void)memcpy((void *)receive_buffer, (void *)(MHU3_OUTBAND_BUF_BASE +
                MHU3_OUTBAND_BUF_HEADER_SIZE), msg_len);

    return MHU_V_3_X_ERR_NONE;

fail:

    return error_mapping_to_mhu_error_t(err);
}
