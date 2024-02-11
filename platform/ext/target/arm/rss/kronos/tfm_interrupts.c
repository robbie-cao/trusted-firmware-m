/*
 * Copyright (c) 2023-2024, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "cmsis.h"
#include "host_base_address.h"
#include "platform_irq.h"
#include "mhu_v3_x.h"
#include "device_definition.h"
#include "mscp_mailbox.h"
#include "mscp_atu_request_handler.h"

#include <inttypes.h>
#include <tfm_hal_platform.h>
#include <tfm_hal_interrupt.h>
#include "tfm_hal_spm_logdev.h"

#define SPMLOG(x) tfm_hal_output_spm_log((x), sizeof(x))

volatile bool scp_doorbell = false;

int mhu_scp_rss_atu_request_doorbell_handler(uint32_t value) {
#ifdef ATU_SCP
    enum mscp_atu_req_error_t status;

    /* Only flag 0 is used to indicate ATU request */
    if ((value & 0x1) != 0x1){
        return 1;
    }

    /* Handle the ATU requests from MSCP */
    status = atu_request_handler(&SCP_ATU_INFO);
    if (status != MSCP_ATU_REQ_ERR_NONE) {
        return 1;
    }

    /* Raise RSS->SCP MHUv3 doorbell to notify SCP about the response */
    status = mhu_v3_x_doorbell_write(&MHU_V3_RSS_TO_SCP_DEV, 0, 0x1);
    if (status != MHU_V_3_X_ERR_NONE) {
        return 1;
    }

#endif
    return 0;
}

static int mhu_scp_rss_systop_on_doorbell_handler(uint32_t value) {
    enum mscp_atu_req_error_t status;

    /* Only flag 0 is used to indicate SYSTOP ON */
    if ((value & 0x1) != 0x1){
        /* SPMLOG_ERRMSG("[CMU_MHU4_R] Undefined doorbell\r\n"); */
        return 1;
    }

    scp_doorbell = true;

    return 0;
}

static int mhu_scp_rss_sys_reset_doorbell_handler(uint32_t value) {
	tfm_hal_system_reset();

	/* Not expected to return from this function call */
	return -1;
}

static int mhu_scp_rss_sys_shutdown_doorbell_handler(uint32_t value) {

	SPMLOG("System shutdown complete\r\n");

	/* 
	 * Not expected to reach here in case of FVP,
	 * but this is required for FPGA as we are unable to shutdown.
	 */
	while(1) {
		__WFI();
	}

	/* Not expected to return from this function call */
	return -1;
}

/* Array of function pointers to call if a message is received on a channel */
static int (*mhu_scp_rss_doorbell_vector[MHU_SCP_RSS_CHANNEL_COUNT]) (uint32_t) = {
    [MHU_SCP_RSS_ATU_REQUEST_CHANNEL_ID]  = mhu_scp_rss_atu_request_doorbell_handler,
    [MHU_SCP_RSS_SYSTOP_ON_CHANNEL_ID]    = mhu_scp_rss_systop_on_doorbell_handler,
    [MHU_SCP_RSS_SYS_RESET_CHANNEL_ID]    = mhu_scp_rss_sys_reset_doorbell_handler,
    [MHU_SCP_RSS_SYS_SHUTDOWN_CHANNEL_ID] = mhu_scp_rss_sys_shutdown_doorbell_handler,
};

/* Function to handle the SCP-->RSS MHUv3 combined MBX interrupt */
void CMU_MHU4_Receiver_Handler(void)
{
    enum mhu_v3_x_channel_type_t ch_type = MHU_V3_X_CHANNEL_TYPE_DBCH;
    uint32_t ch = 0, value = 0, mask = 0x1;
    enum mhu_v3_x_error_t status;

    for (ch = 0; ch < MHU_SCP_RSS_CHANNEL_COUNT; ch++) {
        /* Read the doorbell channel value */
        status = mhu_v3_x_doorbell_read(&MHU_V3_SCP_TO_RSS_DEV, ch, &value);
        if (status != MHU_V_3_X_ERR_NONE) {
            break;
        }

        if (value != 0){
            /* Handle the ATU requests from MSCP */
            status = mhu_scp_rss_doorbell_vector[ch](value);
            if (status != 0) {
                /* Update mask value to clear the doorbell */
                mask = value;
                break;
            }
            break;
        }
    }

    /* Clear the pending interrupt */
    status = mhu_v3_x_doorbell_clear(&MHU_V3_SCP_TO_RSS_DEV, ch, mask);
    if (status != MHU_V_3_X_ERR_NONE) {
    }

    NVIC_ClearPendingIRQ(CMU_MHU4_Receiver_IRQn);
}
