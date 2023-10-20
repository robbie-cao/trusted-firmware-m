/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef FWU_AGENT_H
#define FWU_AGENT_H

#ifdef ENABLE_FWU_AGENT_DEBUG_LOGS
    #include <stdio.h>
    #define FWU_LOG_MSG(f_, ...) printf((f_), ##__VA_ARGS__)
#else
    #define FWU_LOG_MSG(f_, ...)
#endif

enum fwu_agent_error_t {
        FWU_AGENT_SUCCESS = 0,
        FWU_AGENT_ERROR = (-1)
};

#define FWU_ASSERT(_c_)                                                        \
                if (!(_c_)) {                                                  \
                    FWU_LOG_MSG("%s:%d assert hit\n\r", __func__, __LINE__);   \
                    while(1) {};                                               \
                }                                                              \

/* Version used for the very first image of the device. */
#define FWU_IMAGE_INITIAL_VERSION 0

enum fwu_agent_error_t fwu_metadata_provision(void);
enum fwu_agent_error_t fwu_metadata_init(void);

void bl1_get_active_bl2_image(uint32_t *bank_offset);

#endif /* FWU_AGENT_H */
