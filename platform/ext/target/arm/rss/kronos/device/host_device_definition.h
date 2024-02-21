/*
 * Copyright (c) 2023-2024 Arm Limited. All rights reserved.
 *
 * Licensed under the Apache License Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * \file host_device_definition.h
 * \brief The structure definitions in this file are exported based on the
 * peripheral definitions from device_cfg.h.
 * This file is meant to be used as a helper for baremetal
 * applications and/or as an example of how to configure the generic
 * driver structures.
 */

#ifndef __HOST_DEVICE_DEFINITION_H__
#define __HOST_DEVICE_DEFINITION_H__

#include "host_device_cfg.h"
#include "host_base_address.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Each chip has 64 GiB AP address range for peripheral as follows:
 * Chip 0: 0 - 64 GiB
 * Chip 1: 64 - 128 GiB
 * Chip 2: 128 - 192 GiB
 * Chip 3: 192 - 256 GiB
 */
#define HOST_REMOTE_CHIP_PERIPH_OFFSET(chip_id) \
    (64ULL * 1024 * 1024 * 1024 * chip_id)

#ifdef ATU_SCP
#include "atu_rss_drv.h"
extern struct atu_dev_t ATU_DEV_SCP;

/* Indices for SCP ATU regions */
enum SCP_ATU_REGIONS {
    CMN_CONFIG_SCP_ATU_REGION = 0,
    CLUSTER_UTIL_SCP_ATU_REGION,
    AP_SHARED_SRAM_SCP_ATU_REGION,
    RSM_SCP_ATU_REGION,
    AP_PERIPH_SCP_ATU_REGION,
    SCP_ATU_REGION_COUNT,
};

/* Indices for SCP ATU permission regions */
enum SCP_ATU_PERM_REGIONS {
    CMN_CONFIG_SCP_ATU_PERM_REGION = 0,
    CLUSTER_UTIL_SCP_ATU_PERM_REGION,
    AP_SHARED_SRAM_SCP_ATU_PERM_REGION,
    RSM_SCP_ATU_PERM_REGION,
	AP_PERIPH_SCP_ATU_PERM_REGION,
    NCI_GVP_ATU_PERM_REGION,
    ECAM_ATU_PERM_REGION,
    SCP_ATU_PERM_REGION_COUNT,
};

enum ATU_ACCESS_TYPE {
    /* Default Access */
    ATU_ACCESS_DEFAULT = 0,
    /* Root Access */
    ATU_ACCESS_ROOT = 2,
    /* Other access types currently not support by software */
};

/* Structure used to describe an ATU region for SCP ATU */
struct atu_map {
    /* Logical start address */
    uint32_t log_addr;
    /* Physical start address */
    uint64_t phy_addr;
    /* Size of the ATU region */
    uint32_t size;
    /* Access Type */
    enum ATU_ACCESS_TYPE access_type;
};

/* List of ATU regions configured by RSS in SCP ATU */
extern struct atu_map scp_atu_regions[SCP_ATU_REGION_COUNT];

#include "mscp_mailbox.h"
extern struct mailbox_info scp2rss_mbx_info;

/* SCP ATU Info */
#include "mscp_atu_request_handler.h"
extern struct atu_dev_info SCP_ATU_INFO;
#endif

/* Message Handling Units (MHU) */
#ifdef MHU_V3_AP_TO_RSS
#pragma message("Using mhuv3 ap to rss h")
#include "mhu_v3_x.h"
extern struct mhu_v3_x_dev_t MHU_AP_TO_RSS_DEV;
#endif

#ifdef MHU_V3_RSS_TO_AP
#pragma message("Using mhuv3 rss to ap h")
#include "mhu_v3_x.h"
extern struct mhu_v3_x_dev_t MHU_RSS_TO_AP_DEV;
#endif

#ifdef MHU_V3_SCP_TO_RSS
#include "mhu_v3_x.h"
extern struct mhu_v3_x_dev_t MHU_V3_SCP_TO_RSS_DEV;
#endif

#ifdef MHU_V3_RSS_TO_SCP
#include "mhu_v3_x.h"
extern struct mhu_v3_x_dev_t MHU_V3_RSS_TO_SCP_DEV;
#endif

#pragma message("Using mhuv3 si cl0 to rss h")
#include "mhu_v3_x.h"
extern struct mhu_v3_x_dev_t MHU_SI_CL0_TO_RSS_DEV;

#pragma message("Using mhuv3 rss to si cl0 h")
#include "mhu_v3_x.h"
extern struct mhu_v3_x_dev_t MHU_RSS_TO_SI_CL0_DEV;

#pragma message("Using mhuv3 si cl1 to rss h")
#include "mhu_v3_x.h"
extern struct mhu_v3_x_dev_t MHU_SI_CL1_TO_RSS_DEV;

#pragma message("Using mhuv3 rss to si cl1 h")
#include "mhu_v3_x.h"
extern struct mhu_v3_x_dev_t MHU_RSS_TO_SI_CL1_DEV;

#pragma message("Using mhuv3 si cl2 to rss h")
#include "mhu_v3_x.h"
extern struct mhu_v3_x_dev_t MHU_SI_CL2_TO_RSS_DEV;

#pragma message("Using mhuv3 rss to si cl2 h")
#include "mhu_v3_x.h"
extern struct mhu_v3_x_dev_t MHU_RSS_TO_SI_CL2_DEV;

#ifdef __cplusplus
}
#endif

#endif  /* __HOST_DEVICE_DEFINITION_H__ */
