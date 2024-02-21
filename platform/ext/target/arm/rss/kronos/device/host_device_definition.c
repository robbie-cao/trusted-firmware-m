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
 * \file host_device_definition.c
 * \brief This file defines exports the structures based on the peripheral
 * definitions from device_cfg.h.
 * This file is meant to be used as a helper for baremetal
 * applications and/or as an example of how to configure the generic
 * driver structures.
 */

#include "host_device_definition.h"
#include "platform_base_address.h"
#include "host_base_address.h"

#ifdef ATU_SCP

#define CHIP_ADDRESS(addr, idx) (addr + HOST_REMOTE_CHIP_PERIPH_OFFSET(idx))
#define IOVB_NCI_GVP_START_ADDRESS(idx) (CHIP_ADDRESS(0x280000000, idx))
#define IOVB_NCI_GVP_END_ADDRESS(idx) (CHIP_ADDRESS(0x2DFFFFFFF, idx))
#define ECAM_START_ADDRESS(idx) (CHIP_ADDRESS(0x4000000000, idx))
#define ECAM_END_ADDRESS(idx) (CHIP_ADDRESS(0x403FFFFFFF, idx))

static const struct atu_dev_cfg_t ATU_DEV_SCP_CFG = {
    .base = HOST_SCP_ATU_BASE_S};
struct atu_dev_t ATU_DEV_SCP = {&ATU_DEV_SCP_CFG};

/* List of ATU regions allowed to be mapped in the SCP ATU */
static const struct atu_region scp_regions[] = {
    [CMN_CONFIG_SCP_ATU_PERM_REGION] = {
        /* CMN Config region */
        .start_addr = (uint64_t)0x100000000,
        .end_addr = (uint64_t)0x13FFFFFFF,
        .allowed_pas = ATU_REGION_PAS_SECURE,
    },
    [CLUSTER_UTIL_SCP_ATU_PERM_REGION] = {
        /* Cluster Utility region */
        .start_addr = (uint64_t)0x200000000,
        .end_addr = (uint64_t)0x20FFFFFFF,
        .allowed_pas = ATU_REGION_PAS_SECURE,
    },
    [AP_SHARED_SRAM_SCP_ATU_PERM_REGION] = {
        /* AP shared RAM region */
        .start_addr = (uint64_t)0x00000000,
        .end_addr = (uint64_t)0x07FFFFFF,
        .allowed_pas = ATU_REGION_PAS_SECURE,
    },
    [RSM_SCP_ATU_PERM_REGION] = {
        /* RSM region */
        .start_addr = (uint64_t)0x2F000000,
        .end_addr = (uint64_t)0x2F3FFFFF,
        .allowed_pas = ATU_REGION_PAS_SECURE,
    },
    [AP_PERIPH_SCP_ATU_PERM_REGION] = {
        /* AP Peripherals region */
        .start_addr = (uint64_t)0x20000000,
        .end_addr = (uint64_t)0x2FFFFFFF,
        .allowed_pas = ATU_REGION_PAS_SECURE,
    },
    [NCI_GVP_ATU_PERM_REGION] = {
        /* GVP registers region */
        .start_addr = (uint64_t)IOVB_NCI_GVP_START_ADDRESS(0),
        .end_addr = (uint64_t)IOVB_NCI_GVP_END_ADDRESS(0),
        .allowed_pas = ATU_REGION_PAS_SECURE,
    },
    [ECAM_ATU_PERM_REGION] = {
        /* ECAM region */
        .start_addr = (uint64_t)ECAM_START_ADDRESS(0),
        .end_addr = (uint64_t)ECAM_END_ADDRESS(0),
        .allowed_pas = ATU_REGION_PAS_NON_SECURE,
    },
};

/* List of ATU regions to be configured in SCP's ATU */
struct atu_map scp_atu_regions[] = {
    [CMN_CONFIG_SCP_ATU_REGION] = {
        .log_addr = 0xA0000000,
        /* CMN Config space */
        .phy_addr = 0x100000000,
        /* 1 GB */
        .size = 0x40000000,
        .access_type = ATU_ACCESS_ROOT,
    },
    [CLUSTER_UTIL_SCP_ATU_REGION] = {
        .log_addr = 0x60000000,
        /* Cluster Utility region */
        .phy_addr = 0x200000000,
        /* 256 MB */
        .size = 0x10000000,
        .access_type = ATU_ACCESS_ROOT,
    },
    [AP_SHARED_SRAM_SCP_ATU_REGION] = {
        .log_addr = 0x70000000,
        /* AP shared SRAM */
        .phy_addr = 0x00000000,
        /* 128 MB */
        .size = 0x08000000,
        .access_type = ATU_ACCESS_ROOT,
    },
    [RSM_SCP_ATU_REGION] = {
        .log_addr = 0x78000000,
        /* RSM shared SRAM */
        .phy_addr = 0x002F000000,
        /* 4 MB */
        .size = 0x400000,
    },
    [AP_PERIPH_SCP_ATU_REGION] = {
        .log_addr = 0x90000000,
        /* AP peripherals */
        .phy_addr = 0x20000000,
        /* 256 MB */
        .size = 0x10000000,
    },
};

/* Struct to access the shared mailbox between SCP and RSS for ATU requests */
struct mailbox_info scp2rss_mbx_info = {
    .mailbox_base =
        (struct mailbox_msg*)(HOST_SCP_RSS_MAILBOX_BASE_S),
    .type = MAILBOX_TYPE_COMPLETER,
    .mailbox_size = 128
};

/* Struct to manage SCP ATU */
struct atu_dev_info SCP_ATU_INFO = {
    .atu_dev = &ATU_DEV_SCP,
    .allowed_regions_list = &scp_regions,
    .allowed_regions_count = SCP_ATU_PERM_REGION_COUNT,
    .mailbox_info = &scp2rss_mbx_info,
};
#endif

/* Message Handling Units (MHU) */
#ifdef MHU_V3_AP_TO_RSS
struct mhu_v3_x_dev_t MHU_AP_TO_RSS_DEV = {
    .base = MHU2_RECEIVER_BASE_S,
    .frame = MHU_V3_X_MBX_FRAME,
    .outband_buf_base = MHU3_AP_RSS_OUTBAND_BUF_BASE,
    .outband_buf_size = MHU3_AP_RSS_OUTBAND_BUF_SIZE,
    .pbx_dbch_flag = MHU3_PBX_DBCH_FLAG_AP_COMMS,
    .subversion = 0};
#endif

#ifdef MHU_V3_RSS_TO_AP
struct mhu_v3_x_dev_t MHU_RSS_TO_AP_DEV = {
    .base = MHU2_SENDER_BASE_S,
    .frame = MHU_V3_X_PBX_FRAME,
    .outband_buf_base = MHU3_AP_RSS_OUTBAND_BUF_BASE,
    .outband_buf_size = MHU3_AP_RSS_OUTBAND_BUF_SIZE,
    .pbx_dbch_flag = MHU3_PBX_DBCH_FLAG_AP_COMMS,
    .subversion = 0};
#endif

#ifdef MHU_V3_SCP_TO_RSS
struct mhu_v3_x_dev_t MHU_V3_SCP_TO_RSS_DEV = {
    .base = MHU4_RECEIVER_BASE_NS,
    .frame = MHU_V3_X_MBX_FRAME,
    .subversion = 0};
#endif

#ifdef MHU_V3_RSS_TO_SCP
struct mhu_v3_x_dev_t MHU_V3_RSS_TO_SCP_DEV = {
    .base = MHU4_SENDER_BASE_NS,
    .frame = MHU_V3_X_PBX_FRAME,
    .subversion = 0};
#endif

struct mhu_v3_x_dev_t MHU_SI_CL0_TO_RSS_DEV = {
    .base = MHU6_RECEIVER_BASE_S,
    .frame = MHU_V3_X_MBX_FRAME,
    .outband_buf_base = MHU3_SI_CL0_RSS_OUTBAND_BUF_BASE,
    .outband_buf_size = MHU3_SI_CL0_RSS_OUTBAND_BUF_SIZE,
    .pbx_dbch_flag = MHU3_PBX_DBCH_FLAG_SI_COMMS,
    .subversion = 0};

struct mhu_v3_x_dev_t MHU_RSS_TO_SI_CL0_DEV = {
    .base = MHU6_SENDER_BASE_S,
    .frame = MHU_V3_X_PBX_FRAME,
    .outband_buf_base = MHU3_SI_CL0_RSS_OUTBAND_BUF_BASE,
    .outband_buf_size = MHU3_SI_CL0_RSS_OUTBAND_BUF_SIZE,
    .pbx_dbch_flag = MHU3_PBX_DBCH_FLAG_SI_COMMS,
    .subversion = 0};

struct mhu_v3_x_dev_t MHU_SI_CL1_TO_RSS_DEV = {
    .base = MHU7_RECEIVER_BASE_S,
    .frame = MHU_V3_X_MBX_FRAME,
    .outband_buf_base = MHU3_SI_CL1_RSS_OUTBAND_BUF_BASE,
    .outband_buf_size = MHU3_SI_CL1_RSS_OUTBAND_BUF_SIZE,
    .pbx_dbch_flag = MHU3_PBX_DBCH_FLAG_SI_COMMS,
    .subversion = 0};

struct mhu_v3_x_dev_t MHU_RSS_TO_SI_CL1_DEV = {
    .base = MHU7_SENDER_BASE_S,
    .frame = MHU_V3_X_PBX_FRAME,
    .outband_buf_base = MHU3_SI_CL1_RSS_OUTBAND_BUF_BASE,
    .outband_buf_size = MHU3_SI_CL1_RSS_OUTBAND_BUF_SIZE,
    .pbx_dbch_flag = MHU3_PBX_DBCH_FLAG_SI_COMMS,
    .subversion = 0};

struct mhu_v3_x_dev_t MHU_SI_CL2_TO_RSS_DEV = {
    .base = MHU8_RECEIVER_BASE_S,
    .frame = MHU_V3_X_MBX_FRAME,
    .outband_buf_base = MHU3_SI_CL2_RSS_OUTBAND_BUF_BASE,
    .outband_buf_size = MHU3_SI_CL2_RSS_OUTBAND_BUF_SIZE,
    .pbx_dbch_flag = MHU3_PBX_DBCH_FLAG_SI_COMMS,
    .subversion = 0};

struct mhu_v3_x_dev_t MHU_RSS_TO_SI_CL2_DEV = {
    .base = MHU8_SENDER_BASE_S,
    .frame = MHU_V3_X_PBX_FRAME,
    .outband_buf_base = MHU3_SI_CL2_RSS_OUTBAND_BUF_BASE,
    .outband_buf_size = MHU3_SI_CL2_RSS_OUTBAND_BUF_SIZE,
    .pbx_dbch_flag = MHU3_PBX_DBCH_FLAG_SI_COMMS,
    .subversion = 0};
