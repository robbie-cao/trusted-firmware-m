/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "bootutil/bootutil_log.h"
#include "tower_nci_drv.h"
#include "tower_nci_lib.h"
#include "host_device_definition.h"

#include <stddef.h>

#define PSAM_TUPLE(cfg, type, id)                                           \
    {                                                                       \
        .sam = {.base = nci_addr + tower_nci_fetch_offset_address(dis_tree, \
                                            type, id, TOWER_NCI_PSAM) },    \
        .nh_region_count = sizeof(cfg) / sizeof(struct psam_region_cfgs),   \
        .regions = cfg,                                                     \
    }

#define APU_TUPLE(cfg, type, id)                                            \
    {                                                                       \
        .apu = {.base = nci_addr + tower_nci_fetch_offset_address(dis_tree, \
                                            type, id, TOWER_NCI_APU) },     \
        .region_count = sizeof(cfg) / sizeof(struct apu_region_cfgs),       \
        .regions = cfg,                                                     \
    }

/* Interface ID of xSNI components */
enum sysctrl_xSNI_ids {
    SYSCTRL_APP_ASNI_ID = 0x0,
    SYSCTRL_LCP_ASNI_ID,
    SYSCTRL_MCP_ASNI_ID,
    SYSCTRL_RSS_MAIN_ASNI_ID,
    SYSCTRL_RSS_SCP_ASNI_ID,
    SYSCTRL_SCP_ASNI_ID
};

/* Interface ID of xMNI components */
enum sysctrl_xMNI_ids {
    SYSCTRL_APP_AMNI_ID = 0x0,
    SYSCTRL_APP_MCP_AMNI_ID,
    SYSCTRL_APP_SCP_AMNI_ID,
    SYSCTRL_LCP_AMNI_ID,
    SYSCTRL_LCP_SCP_AMNI_ID,
    SYSCTRL_RSM_AMNI_ID,
    SYSCTRL_RSS_MCP_AMNI_ID,
    SYSCTRL_RSS_SCP_AMNI_ID,
    SYSCTRL_CMN_PMNI_ID,
    SYSCTRL_RSM_PMNI_ID,
    SYSCTRL_TCU_PMNI_ID,

    SYSCTRL_CONFIG_SPACE_ID = 0xF
};

/* APU IDs of the initiator for filter access */
enum sysctrl_apu_filter_ids {
    SYSCTRL_MCP_APU_ID = 0x3C,
    SYSCTRL_SCP_APU_ID = 0x3D,
    SYSCTRL_RSS_APU_ID = 0x3E,
    SYSCTRL_DAP_APU_ID = 0x3F,
};

/* Interface ID of Peripheral xMNI components */
enum periph_xMNI_ids {
    PERIPH_RAM_AMNI_ID = 0x0,
    PERIPH_ECCREG_PMNI_ID,
    PERIPH_GTIMERCTRL_PMNI_ID,
    PERIPH_NSGENWDOG_PMNI_ID,
    PERIPH_NSGTIMER_PMNI_ID,
    PERIPH_NSUART0_PMNI_ID,
    PERIPH_NSUART1_PMNI_ID,
    PERIPH_ROOTGENWDOG_PMNI_ID,
    PERIPH_SECGENWDOG_PMNI_ID,
    PERIPH_SECGTIMER_PMNI_ID,
    PERIPH_SECUART_PMNI_ID
};

/* PSAM region config structure */
struct psam_region_cfgs {
    uint64_t base_addr;
    uint64_t end_addr;
    uint64_t tgt_id;
    uint64_t region;
};

/* APU region config structure */
struct apu_region_cfgs {
    uint64_t base_addr;
    uint64_t end_addr;
    uint32_t br;
    uint32_t perms;
    uint32_t region;
};

/* PSAM config structure */
struct psam_cfgs {
    struct tower_nci_psam_dev_t sam;
    int nh_region_count;
    struct psam_region_cfgs* regions;
};

/* PSAM config structure */
struct apu_cfgs {
    struct tower_nci_apu_dev_t apu;
    int region_count;
    struct apu_region_cfgs* regions;
};

uint32_t program_psam_table(struct psam_cfgs psam_table[], int psam_table_count)
{
    enum tower_nci_err_t err = 0;
    struct psam_region_cfgs* r_map;
    uint32_t p_idx, r_idx;

    for (p_idx = 0; p_idx < psam_table_count; ++p_idx) {
        for (r_idx = 0; r_idx < psam_table[p_idx].nh_region_count; ++r_idx) {
            r_map = &psam_table[p_idx].regions[r_idx];
            err = tower_nci_psam_nhregion_init(&psam_table[p_idx].sam,
                                               r_map->base_addr,
                                               r_map->end_addr, r_map->tgt_id,
                                               r_map->region);
            if (err != TOWER_NCI_SUCCESS) {
                return 1;
            }
        }
    }

    for (p_idx = 0; p_idx < psam_table_count; ++p_idx) {
        err = tower_nci_psam_enable(&psam_table[p_idx].sam);
        if (err != TOWER_NCI_SUCCESS) {
            return 1;
        }
    }

    return err;
}

uint32_t program_apu_table(struct apu_cfgs apu_table[], int apu_table_count)
{
    enum tower_nci_err_t err = 0;
    struct apu_region_cfgs* r_map;
    uint32_t a_idx, r_idx, id_idx;
    uint8_t entity_ids[4] = { 0, 0, 0, 0 };

    for (a_idx = 0; a_idx < apu_table_count; ++a_idx) {
        for (r_idx = 0; r_idx < apu_table[a_idx].region_count; ++r_idx) {
            r_map = &apu_table[a_idx].regions[r_idx];
            uint32_t permissions[4] = { r_map->perms, 0, 0, 0 };
            err = tower_nci_apu_initialize_region(&apu_table[a_idx].apu,
                                                  r_map->region,
                                                  r_map->base_addr,
                                                  r_map->end_addr, r_map->br,
                                                  permissions, entity_ids,
                                                  T_NCI_ID_VALID_NONE,
                                                  T_NCI_REGION_ENABLE,
                                                  T_NCI_UNLOCK);
            if (err != TOWER_NCI_SUCCESS) {
                return 1;
            }
        }

        err = tower_nci_apu_sync_err_enable(&apu_table[a_idx].apu);
        if (err != TOWER_NCI_SUCCESS) {
            return 1;
        }

        err = tower_nci_apu_enable(&apu_table[a_idx].apu);
        if (err != TOWER_NCI_SUCCESS) {
            return 1;
        }
    }

    return err;
}

/* Program PSAM for the platform System Control Tower NCI */
uint32_t program_sysctrl_psam(struct tower_nci_discovery_tree_t* dis_tree,
                              uint32_t nci_addr, uint64_t chip_addr_offset)
{
    struct mhu_send_frame_t {
        uint64_t base_0, end_0;
        uint64_t base_1, end_1;
        uint64_t base_2, end_2;
    } mhu_send_frame_rss, mhu_send_frame_scp, mhu_send_frame_mcp;

    switch (chip_addr_offset) {
        case 0x0000000000 :
                mhu_send_frame_rss = (struct mhu_send_frame_t)
                                  {0x000102ACC0000ULL, 0x000102ACCFFFFULL,
                                   0x000202ACC0000ULL, 0x000202ACCFFFFULL,
                                   0x000302ACC0000ULL, 0x000302ACCFFFFULL};
                mhu_send_frame_scp = (struct mhu_send_frame_t)
                                  {0x000102AC00000ULL, 0x000102AC0FFFFULL,
                                   0x000202AC00000ULL, 0x000202AC0FFFFULL,
                                   0x000302AC00000ULL, 0x000302AC0FFFFULL};
                mhu_send_frame_mcp = (struct mhu_send_frame_t)
                                  {0x000102A600000ULL, 0x000102AC6FFFFULL,
                                   0x000202A600000ULL, 0x000202AC6FFFFULL,
                                   0x000302A600000ULL, 0x000302AC6FFFFULL};
                break;
        case 0x1000000000 :
                mhu_send_frame_rss = (struct mhu_send_frame_t)
                                  {0x000002ACC0000ULL, 0x000002ACCFFFFULL,
                                   0x000202ACD0000ULL, 0x000202ACDFFFFULL,
                                   0x000302ACD0000ULL, 0x000302ACDFFFFULL};
                mhu_send_frame_scp = (struct mhu_send_frame_t)
                                  {0x000002AC00000ULL, 0x000002AC0FFFFULL,
                                   0x000202AC10000ULL, 0x000202AC1FFFFULL,
                                   0x000302AC10000ULL, 0x000302AC1FFFFULL};
                mhu_send_frame_mcp = (struct mhu_send_frame_t)
                                  {0x000002AC60000ULL, 0x000002AC6FFFFULL,
                                   0x000202AC60000ULL, 0x000202AC6FFFFULL,
                                   0x000302AC60000ULL, 0x000302AC6FFFFULL};
                break;
        case 0x2000000000 :
                mhu_send_frame_rss = (struct mhu_send_frame_t)
                                  {0x000002AC20000ULL, 0x000002AC2FFFFULL,
                                   0x000102AC20000ULL, 0x000102AC2FFFFULL,
                                   0x000302ACE0000ULL, 0x000302ACEFFFFULL};
                mhu_send_frame_scp = (struct mhu_send_frame_t)
                                  {0x000002AC10000ULL, 0x000002AC1FFFFULL,
                                   0x000102AC10000ULL, 0x000102AC1FFFFULL,
                                   0x000302AC20000ULL, 0x000302AC2FFFFULL};
                mhu_send_frame_mcp = (struct mhu_send_frame_t)
                                  {0x000002AC60000ULL, 0x000002AC6FFFFULL,
                                   0x000102AC60000ULL, 0x000102AC6FFFFULL,
                                   0x000302AC60000ULL, 0x000302AC6FFFFULL};
                break;
        case 0x3000000000 :
                mhu_send_frame_rss = (struct mhu_send_frame_t)
                                  {0x000002ACE0000ULL, 0x000002ACEFFFFULL,
                                   0x000102ACE0000ULL, 0x000102ACEFFFFULL,
                                   0x000202ACE0000ULL, 0x000202ACEFFFFULL};
                mhu_send_frame_scp = (struct mhu_send_frame_t)
                                  {0x000002AC20000ULL, 0x000002AC2FFFFULL,
                                   0x000102AC20000ULL, 0x000102AC2FFFFULL,
                                   0x000202AC20000ULL, 0x000202AC2FFFFULL};
                mhu_send_frame_mcp = (struct mhu_send_frame_t)
                                  {0x000002AC60000ULL, 0x000002AC6FFFFULL,
                                   0x000102AC60000ULL, 0x000102AC6FFFFULL,
                                   0x000202AC60000ULL, 0x000202AC6FFFFULL};
                break;
        default:
                BOOT_LOG_ERR("Chip Address invalid!");
                return 1;
    }
    struct psam_region_cfgs rss_main_axis[] = {
        { 0x0000000000000 + chip_addr_offset, 0x000001FFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 0  },
        { 0x0000020000000 + chip_addr_offset, 0x0000020FFFFFF + chip_addr_offset, SYSCTRL_CONFIG_SPACE_ID, 1  },
        { 0x0000021000000 + chip_addr_offset, 0x000002A5AFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 2  },
        { 0x000002A5B0000 + chip_addr_offset, 0x000002A60FFFF + chip_addr_offset, SYSCTRL_RSM_PMNI_ID    , 3  },
        { 0x000002A610000 + chip_addr_offset, 0x000002ACBFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 4  },
        { 0x000002ACF0000 + chip_addr_offset, 0x000002EFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 5  },
        { 0x000002F000000 + chip_addr_offset, 0x000002F3FFFFF + chip_addr_offset, SYSCTRL_RSM_AMNI_ID    , 6  },
        { 0x000002F400000 + chip_addr_offset, 0x00000FFFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 7  },
        { 0x0000100000000 + chip_addr_offset, 0x000017FFFFFFF + chip_addr_offset, SYSCTRL_CMN_PMNI_ID    , 8  },
        { 0x0000180000000 + chip_addr_offset, 0x00002FFFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 9  },
        { 0x0000300000000 + chip_addr_offset, 0x0000307FFFFFF + chip_addr_offset, SYSCTRL_TCU_PMNI_ID    , 10 },
        { 0x0000308000000 + chip_addr_offset, 0x0000FFFFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 11 },
        { mhu_send_frame_rss.base_0,          mhu_send_frame_rss.end_0,           SYSCTRL_APP_AMNI_ID    , 12 },
        { mhu_send_frame_rss.base_1,          mhu_send_frame_rss.end_1,           SYSCTRL_APP_AMNI_ID    , 13 },
        { mhu_send_frame_rss.base_2,          mhu_send_frame_rss.end_2,           SYSCTRL_APP_AMNI_ID    , 14 },
        { 0x0004000000000,                    0x0FFFFFFFFFFFF,                    SYSCTRL_APP_AMNI_ID    , 15 },
        { 0x1000000000000,                    0x10000FFFFFFFF,                    SYSCTRL_RSS_SCP_AMNI_ID, 16 },
        { 0x2000000000000,                    0x20000FFFFFFFF,                    SYSCTRL_RSS_MCP_AMNI_ID, 17 }
    };

    struct psam_region_cfgs scp_axis[] = {
        { 0x0000000000000 + chip_addr_offset, 0x000002A5AFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    0  },
        { 0x000002A5B0000 + chip_addr_offset, 0x000002A60FFFF + chip_addr_offset, SYSCTRL_RSM_PMNI_ID,    1  },
        { 0x000002A610000 + chip_addr_offset, 0x000002ABFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    2  },
        { 0x000002AC30000 + chip_addr_offset, 0x000002EFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    3  },
        { 0x000002F000000 + chip_addr_offset, 0x000002F3FFFFF + chip_addr_offset, SYSCTRL_RSM_AMNI_ID,    4  },
        { 0x000002F400000 + chip_addr_offset, 0x00000FFFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    5  },
        { 0x0000100000000 + chip_addr_offset, 0x000017FFFFFFF + chip_addr_offset, SYSCTRL_CMN_PMNI_ID,    6  },
        { 0x0000180000000 + chip_addr_offset, 0x0000FFFFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    7  },
        { mhu_send_frame_scp.base_0,          mhu_send_frame_scp.end_0,           SYSCTRL_APP_AMNI_ID,    8  },
        { mhu_send_frame_scp.base_1,          mhu_send_frame_scp.end_1,           SYSCTRL_APP_AMNI_ID,    9  },
        { mhu_send_frame_scp.base_2,          mhu_send_frame_scp.end_2,           SYSCTRL_APP_AMNI_ID,    10 },
        { 0x0004000000000,                    0x0FFFFFFFFFFFF,                    SYSCTRL_APP_AMNI_ID,    11 },
    };

    struct psam_region_cfgs mcp_axis[] = {
        { 0x0000000000000 + chip_addr_offset, 0x000002A42FFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     0  },
        { 0x000002A430000 + chip_addr_offset, 0x000002A43FFFF + chip_addr_offset, SYSCTRL_APP_SCP_AMNI_ID, 1  },
        { 0x000002A440000 + chip_addr_offset, 0x000002A49FFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     2  },
        { 0x000002A4A0000 + chip_addr_offset, 0x000002A4AFFFF + chip_addr_offset, SYSCTRL_APP_SCP_AMNI_ID, 3  },
        { 0x000002A4B0000 + chip_addr_offset, 0x000002A5AFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     4  },
        { 0x000002A5B0000 + chip_addr_offset, 0x000002A60FFFF + chip_addr_offset, SYSCTRL_RSM_PMNI_ID,     5  },
        { 0x000002A610000 + chip_addr_offset, 0x000002A7FFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     6  },
        { 0x000002A800000 + chip_addr_offset, 0x000002A80FFFF + chip_addr_offset, SYSCTRL_APP_SCP_AMNI_ID, 7  },
        { 0x000002A810000 + chip_addr_offset, 0x000002AC5FFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     8  },
        { 0x000002AC90000 + chip_addr_offset, 0x000002EFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     9  },
        { 0x000002F000000 + chip_addr_offset, 0x000002F3FFFFF + chip_addr_offset, SYSCTRL_RSM_AMNI_ID,     10 },
        { 0x000002F400000 + chip_addr_offset, 0x00000FFFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     11 },
        { 0x0000100000000 + chip_addr_offset, 0x000017FFFFFFF + chip_addr_offset, SYSCTRL_CMN_PMNI_ID,     12 },
        { 0x0000180000000 + chip_addr_offset, 0x00001BFFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     13 },
        { 0x00001C0000000 + chip_addr_offset, 0x00001FFFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     14 },
        { 0x0000240000000 + chip_addr_offset, 0x00002FFFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     15 },
        { 0x0000308000000 + chip_addr_offset, 0x0000FFFFFFFFF + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     16 },
        { mhu_send_frame_mcp.base_0,          mhu_send_frame_mcp.end_0,           SYSCTRL_APP_AMNI_ID,     17 },
        { mhu_send_frame_mcp.base_1,          mhu_send_frame_mcp.end_1,           SYSCTRL_APP_AMNI_ID,     18 },
        { mhu_send_frame_mcp.base_2,          mhu_send_frame_mcp.end_2,           SYSCTRL_APP_AMNI_ID,     19 },
        { 0x0004000000000,                    0x0FFFFFFFFFFFF,                    SYSCTRL_APP_AMNI_ID,     20 },
    };

    struct psam_region_cfgs app_axis[] = {
        { 0x000002A430000, 0x000002A95FFFF, SYSCTRL_APP_SCP_AMNI_ID, 0 },
        { 0x000002AA00000, 0x000002AA5FFFF, SYSCTRL_APP_MCP_AMNI_ID, 1 },
        { 0x000002AB00000, 0x000002AC2FFFF, SYSCTRL_APP_SCP_AMNI_ID, 2 },
        { 0x000002AC60000, 0x000002AC8FFFF, SYSCTRL_APP_MCP_AMNI_ID, 3 },
        { 0x000002ACC0000, 0x000002B12FFFF, SYSCTRL_APP_SCP_AMNI_ID, 4 },
    };

    struct psam_region_cfgs lcp_axis[] = {
        { 0x000000000000, 0x0000FFFFFFFF, SYSCTRL_LCP_SCP_AMNI_ID, 0 },
    };

    struct psam_region_cfgs rss_scp_axis[] = {
        { 0x000000000000, 0x0000001FFFFF, SYSCTRL_LCP_AMNI_ID, 0 },
    };

    struct psam_cfgs psam_table[] = {
        PSAM_TUPLE(rss_main_axis, TOWER_NCI_ASNI, SYSCTRL_RSS_MAIN_ASNI_ID),
        PSAM_TUPLE(scp_axis,      TOWER_NCI_ASNI, SYSCTRL_SCP_ASNI_ID),
        PSAM_TUPLE(mcp_axis,      TOWER_NCI_ASNI, SYSCTRL_MCP_ASNI_ID),
        PSAM_TUPLE(app_axis,      TOWER_NCI_ASNI, SYSCTRL_APP_ASNI_ID),
        PSAM_TUPLE(lcp_axis,      TOWER_NCI_ASNI, SYSCTRL_LCP_ASNI_ID),
        PSAM_TUPLE(rss_scp_axis,  TOWER_NCI_ASNI, SYSCTRL_RSS_SCP_ASNI_ID),
    };

    if (program_psam_table(psam_table, sizeof(psam_table) /
                                      sizeof(struct psam_cfgs)) != 0) {
        return 1;
    }

    BOOT_LOG_INF("System Control Block PSAM programming done");

    return 0;
}

/* Program APU for the platform System Control Tower NCI */
uint32_t program_sysctrl_apu(struct tower_nci_discovery_tree_t* dis_tree,
                             uint64_t nci_addr, uint64_t chip_addr_offset)
{
    struct apu_region_cfgs mcp_axis[] = {
        { 0x000000000000, 0x00002A42FFFF,  T_NCI_FOREGROUND, T_NCI_ALL_PERM, 0 },
        { 0x00002A430000, 0x00002A43FFFF,  T_NCI_FOREGROUND, T_NCI_ROOT_RW,  1 },
        { 0x00002A440000, 0x0FFFFFFFFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 2 },
    };

    struct apu_region_cfgs app_axis[] = {
        { 0x00002A430000, 0x00002A43FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  0  },
        { 0x00002A440000, 0x00002A93FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 1 },
        { 0x00002A940000, 0x00002A95FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  2 },
        { 0x00002A960000, 0x00002AA3FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 3 },
        { 0x00002AA40000, 0x00002AA5FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  4 },
        { 0x00002AA60000, 0x00002AB3FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 5 },
        { 0x00002AB40000, 0x00002AB5FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  6 },
        { 0x00002AB60000, 0x00002AB7FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_REALM_RW, 7 },
        { 0x00002AB80000, 0x00002ABFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 8 },
        { 0x00002AC00000, 0x00002ACEFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  9 },
        { 0x00002ACF0000, 0x00002B09FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 10 },
        { 0x00002B100000, 0x00002B12FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  11 },
    };

    struct apu_region_cfgs rsm_axim[] = {
        { 0x00002F000000 + chip_addr_offset, 0x00002F1FFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_ALL_PERM,  1 },
        { 0x00002F200000 + chip_addr_offset, 0x00002F3FFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_ALL_PERM,  2 },
    };

    struct apu_region_cfgs rsm_apbm[] = {
        { 0x00002A5B0000 + chip_addr_offset, 0x00002A5BFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_SEC_RW | T_NCI_ROOT_RW, 0 },
        { 0x00002A5C0000 + chip_addr_offset, 0x00002A5CFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_ALL_PERM,               1 },
        { 0x00002A5D0000 + chip_addr_offset, 0x00002A5DFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_SEC_RW | T_NCI_ROOT_RW, 2 },
        { 0x00002A5E0000 + chip_addr_offset, 0x00002A5EFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_ALL_PERM,               3 },
        { 0x00002A5F0000 + chip_addr_offset, 0x00002A5FFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_SEC_RW | T_NCI_ROOT_RW, 4 },
        { 0x00002A600000 + chip_addr_offset, 0x00002A60FFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_ALL_PERM,               5 },
    };

    struct apu_region_cfgs lcp_axim[] = {
        { 0x000000, 0x02FFFF, T_NCI_FOREGROUND, T_NCI_SEC_RW | T_NCI_ROOT_RW,                  0 },
        { 0x030000, 0x05FFFF, T_NCI_FOREGROUND, T_NCI_SEC_RW,                                  1 },
        { 0x060000, 0x18FFFF, T_NCI_FOREGROUND, T_NCI_SEC_RW | T_NCI_ROOT_RW,                  2 },
        { 0x190000, 0x19FFFF, T_NCI_FOREGROUND, T_NCI_N_SEC_RW | T_NCI_SEC_RW | T_NCI_ROOT_RW, 3 },
        { 0x1A0000, 0x1FFFFF, T_NCI_FOREGROUND, T_NCI_SEC_RW | T_NCI_ROOT_RW,                  4 },
    };

    struct apu_region_cfgs lcp_axis[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 0, },
    };

    struct apu_region_cfgs rss_scp_axim[] = {
        { 0x1000000000000, 0x10000FFFFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 0, },
    };

    struct apu_region_cfgs rss_mcp_axim[] = {
        { 0x2000000000000, 0x20000FFFFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 0, },
    };

    struct apu_cfgs apu_table[] = {
        APU_TUPLE(mcp_axis,     TOWER_NCI_ASNI, SYSCTRL_MCP_ASNI_ID),
        APU_TUPLE(app_axis,     TOWER_NCI_ASNI, SYSCTRL_APP_ASNI_ID),
        APU_TUPLE(rsm_axim,     TOWER_NCI_AMNI, SYSCTRL_RSM_AMNI_ID),
        APU_TUPLE(rsm_apbm,     TOWER_NCI_PMNI, SYSCTRL_RSM_PMNI_ID),
        APU_TUPLE(lcp_axim,     TOWER_NCI_AMNI, SYSCTRL_LCP_AMNI_ID),
        APU_TUPLE(lcp_axis,     TOWER_NCI_ASNI, SYSCTRL_LCP_ASNI_ID),
        APU_TUPLE(rss_scp_axim, TOWER_NCI_AMNI, SYSCTRL_RSS_SCP_AMNI_ID),
        APU_TUPLE(rss_mcp_axim, TOWER_NCI_AMNI, SYSCTRL_RSS_MCP_AMNI_ID),
    };

    if (program_apu_table(apu_table, sizeof(apu_table) /
                                     sizeof(struct apu_cfgs)) != 0) {
        return 1;
    }

    BOOT_LOG_INF("System Control Block APU programming done");

    return 0;
}

uint32_t program_periph_apu(struct tower_nci_discovery_tree_t* dis_tree,
                            uint64_t nci_addr)
{
    // RAM APU
    struct apu_region_cfgs ram_axim[] = {
        { 0x0000000000, 0x0007FFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 0 },
    };

    // UART APUs
    struct apu_region_cfgs nsuart0_apbm[] = {
        { 0x002A400000, 0x002A40FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 0 },
    };

    struct apu_region_cfgs secuart_apbm[] = {
        { 0x002A410000, 0x002A41FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW, 0 },
    };

    struct apu_region_cfgs nsuart1_apbm[] = {
        { 0x002A420000, 0x002A42FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 0 },
    };

    // WDOG APUs
    struct apu_region_cfgs nsgenwdog_apbm[] = {
        { 0x002A440000, 0x002A44FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 0 },
        { 0x002A450000, 0x002A45FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 1 },
    };

    struct apu_region_cfgs rootgenwdog_apbm[] = {
        { 0x002A460000, 0x002A46FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW, 0 },
        { 0x002A470000, 0x002A47FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW, 1 },
    };

    struct apu_region_cfgs secgenwdog_apbm[] = {
        { 0x002A480000, 0x002A48FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW, 0 },
        { 0x002A490000, 0x002A49FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW, 1 },
    };

    // ECCREG APU
    struct apu_region_cfgs eccreg_apbm[] = {
        { 0x002A4B0000, 0x002A4BFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW,   0  },
        { 0x002A4C0000, 0x002A4CFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 1  },
        { 0x002A4D0000, 0x002A4DFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  2  },
        { 0x002A4E0000, 0x002A4EFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_REALM_RW, 3  },
        { 0x002A4F0000, 0x002A4FFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW,   4  },
        { 0x002A500000, 0x002A50FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 5  },
        { 0x002A510000, 0x002A51FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  6  },
        { 0x002A520000, 0x002A52FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_REALM_RW, 7  },
        { 0x002A530000, 0x002A53FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW,   8  },
        { 0x002A540000, 0x002A54FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 9  },
        { 0x002A550000, 0x002A55FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  10 },
        { 0x002A560000, 0x002A56FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_REALM_RW, 11 },
        { 0x002A570000, 0x002A57FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW,   12 },
        { 0x002A580000, 0x002A58FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 13 },
        { 0x002A590000, 0x002A59FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  14 },
        { 0x002A5A0000, 0x002A5AFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_REALM_RW, 15 },
    };

    // GTIMER APUs
    struct apu_region_cfgs gtimerctrl_apbm[] = {
        { 0x002A810000, 0x002A81FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 0 },
    };

    struct apu_region_cfgs secgtimer_apbm[] = {
        { 0x002A820000, 0x002A82FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW, 0 },
    };

    struct apu_region_cfgs nsgtimer_apbm[] = {
        { 0x002A830000, 0x002A83FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, 0 },
    };

    struct apu_cfgs apu_table[] = {
        APU_TUPLE(ram_axim,         TOWER_NCI_AMNI, PERIPH_RAM_AMNI_ID        ),
        APU_TUPLE(nsuart0_apbm,     TOWER_NCI_PMNI, PERIPH_NSUART0_PMNI_ID    ),
        APU_TUPLE(secuart_apbm,     TOWER_NCI_PMNI, PERIPH_SECUART_PMNI_ID    ),
        APU_TUPLE(nsuart1_apbm,     TOWER_NCI_PMNI, PERIPH_NSUART1_PMNI_ID    ),
        APU_TUPLE(nsgenwdog_apbm,   TOWER_NCI_PMNI, PERIPH_NSGENWDOG_PMNI_ID  ),
        APU_TUPLE(rootgenwdog_apbm, TOWER_NCI_PMNI, PERIPH_ROOTGENWDOG_PMNI_ID),
        APU_TUPLE(secgenwdog_apbm,  TOWER_NCI_PMNI, PERIPH_SECGENWDOG_PMNI_ID ),
        APU_TUPLE(eccreg_apbm,      TOWER_NCI_PMNI, PERIPH_ECCREG_PMNI_ID     ),
        APU_TUPLE(gtimerctrl_apbm,  TOWER_NCI_PMNI, PERIPH_GTIMERCTRL_PMNI_ID ),
        APU_TUPLE(secgtimer_apbm,   TOWER_NCI_PMNI, PERIPH_SECGTIMER_PMNI_ID  ),
        APU_TUPLE(nsgtimer_apbm,    TOWER_NCI_PMNI, PERIPH_NSGTIMER_PMNI_ID   ),
    };

    if (program_apu_table(apu_table, sizeof(apu_table) /
                                     sizeof(struct apu_cfgs)) != 0) {
        return 1;
    }

    BOOT_LOG_INF("Peripheral Block APU programming done");

    return 0;
}


uint8_t prune_node_cb(uint16_t type, uint16_t id, uint16_t child_idx)
{
    return 0;
}

uint32_t program_sysctrl_tower_nci(uint32_t nci_addr, uint64_t chip_addr_offset)
{
    struct tower_nci_discovery_tree_t dis_tree = {
        .type = TOWER_NCI_CFGNI,
        .id = 0,
        .address = 0x0,
        .sibling = NULL,
    };

    tower_nci_discovery(&dis_tree, nci_addr, &prune_node_cb);

    if (program_sysctrl_psam(&dis_tree, nci_addr, chip_addr_offset) ||
        program_sysctrl_apu(&dis_tree, nci_addr, chip_addr_offset))
    {
        free_discovery_tree(dis_tree.child);
        return 1;
    }

    free_discovery_tree(dis_tree.child);

    return 0;
}

uint32_t program_periph_tower_nci(uint32_t nci_addr) {
    struct tower_nci_discovery_tree_t dis_tree = {
        .type = TOWER_NCI_CFGNI,
        .id = 0,
        .address = 0x0,
        .sibling = NULL,
    };

    tower_nci_discovery(&dis_tree, nci_addr, &prune_node_cb);

    if (program_periph_apu(&dis_tree, nci_addr)) {
        free_discovery_tree(dis_tree.child);
        return 1;
    }

    free_discovery_tree(dis_tree.child);

    return 0;
}

#undef PSAM_TUPLE
#undef APU_TUPLE
