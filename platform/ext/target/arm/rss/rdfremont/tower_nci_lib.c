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

/* Interface ID of xSNI components */
#define SYSCTRL_APP_ASNI_ID     0x0
#define SYSCTRL_LCP_ASNI_ID     0x1
#define SYSCTRL_MCP_ASNI_ID     0x2
#define SYSCTRL_RSS_ASNI_ID     0x3
#define SYSCTRL_SCP_ASNI_ID     0x4

/* Interface ID of xMNI components */
#define SYSCTRL_CONFIG_AON_ID   0xF
#define SYSCTRL_APP_AMNI_ID     0x0
#define SYSCTRL_APP_MCP_AMNI_ID 0x1
#define SYSCTRL_APP_SCP_AMNI_ID 0x2
#define SYSCTRL_LCP_AMNI_ID     0x3
#define SYSCTRL_LCP_SCP_AMNI_ID 0x4
#define SYSCTRL_RSM_AMNI_ID     0x5
#define SYSCTRL_RSS_MCP_AMNI_ID 0x6
#define SYSCTRL_RSS_SCP_AMNI_ID 0x7
#define SYSCTRL_CMN_PMNI_ID     0x8
#define SYSCTRL_RSM_PMNI_ID     0x9
#define SYSCTRL_TCU_PMNI_ID     0xA

/* APU IDs of the initiator for filter access */
#define SYSCTRL_MCP_APU_ID      0x3C
#define SYSCTRL_SCP_APU_ID      0x3D
#define SYSCTRL_RSS_APU_ID      0x3E
#define SYSCTRL_DAP_APU_ID      0x3F

/* Interface ID of Peripheral xMNI components */
#define PERIPH_RAM_AMNI_ID               0x0
#define PERIPH_ECCREG_PMNI_ID            0x1
#define PERIPH_GTIMERCTRL_PMNI_ID        0x2
#define PERIPH_NSGENWDOG_PMNI_ID         0x3
#define PERIPH_NSGTIMER_PMNI_ID          0x4
#define PERIPH_NSUART0_PMNI_ID           0x5
#define PERIPH_NSUART1_PMNI_ID           0x6
#define PERIPH_ROOTGENWDOG_PMNI_ID       0x7
#define PERIPH_SECGENWDOG_PMNI_ID        0x8
#define PERIPH_SECGTIMER_PMNI_ID         0x9
#define PERIPH_SECUART_PMNI_ID           0xA

static volatile uint8_t chip_id;
/* Read Chip ID and store it in a global variable */
static void read_chip_id()
{
    uint32_t *chip_id_reg = (uint32_t *)RSS_INTEG_REG_BASE;

    chip_id = (((*chip_id_reg) >> 0x8) & 0xFF);

    BOOT_LOG_INF("[TOWER] CHIP ID: %u", chip_id);
}


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
    uint32_t entities;
    uint32_t region;
    uint8_t id[4];
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

    for (a_idx = 0; a_idx < apu_table_count; ++a_idx) {
        for (r_idx = 0; r_idx < apu_table[a_idx].region_count; ++r_idx) {
            r_map = &apu_table[a_idx].regions[r_idx];
            err = tower_nci_apu_initialize_region(&apu_table[a_idx].apu,
                                                  r_map->region,
                                                  r_map->base_addr,
                                                  r_map->end_addr, r_map->br,
                                                  r_map->perms, r_map->entities,
                                                  T_NCI_VALID_ALL_IDS);
            if (err != TOWER_NCI_SUCCESS) {
                return 1;
            }

            for (id_idx = 0; id_idx < 4; ++id_idx) {
                uint32_t id_select;
                switch (id_idx)
                {
                case 0: id_select = T_NCI_ID_0_SELECT; break;
                case 1: id_select = T_NCI_ID_1_SELECT; break;
                case 2: id_select = T_NCI_ID_2_SELECT; break;
                case 3: id_select = T_NCI_ID_3_SELECT; break;
                }
                err = tower_nci_apu_assign_id(&apu_table[a_idx].apu,
                                              r_map->region, r_map->id[id_idx],
                                              id_select);
                if (err != TOWER_NCI_SUCCESS) {
                    return 1;
                }
            }
            err = tower_nci_apu_set_region_enable(&apu_table[a_idx].apu,
                                                  r_map->region);
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
        case 0 :
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
        case 0x0001000000000 :
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
        case 0x0002000000000 :
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
        case 0x0003000000000 :
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

     struct psam_region_cfgs rss_asni[] = {
        { 0x0000000000000ULL + chip_addr_offset, 0x000001FFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 0  },
        { 0x0000020000000ULL + chip_addr_offset, 0x0000020FFFFFFULL + chip_addr_offset, SYSCTRL_CONFIG_AON_ID  , 1  },
        { 0x0000021000000ULL + chip_addr_offset, 0x000002A5AFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 2  },
        { 0x000002A5B0000ULL + chip_addr_offset, 0x000002A60FFFFULL + chip_addr_offset, SYSCTRL_RSM_PMNI_ID    , 3  },
        { 0x000002A610000ULL + chip_addr_offset, 0x000002ACBFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 4  },
        { 0x000002ACF0000ULL + chip_addr_offset, 0x000002EFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 5  },
        { 0x000002F000000ULL + chip_addr_offset, 0x000002F3FFFFFULL + chip_addr_offset, SYSCTRL_RSM_AMNI_ID    , 6  },
        { 0x000002F400000ULL + chip_addr_offset, 0x00000FFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 7  },
        { 0x0000100000000ULL + chip_addr_offset, 0x000017FFFFFFFULL + chip_addr_offset, SYSCTRL_CMN_PMNI_ID    , 8  },
        { 0x0000180000000ULL + chip_addr_offset, 0x00001BFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 9  },
        { 0x00001C0000000ULL + chip_addr_offset, 0x00001FFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 10 },
        { 0x0000200000000ULL + chip_addr_offset, 0x000023FFFFFFFULL + chip_addr_offset, SYSCTRL_LCP_AMNI_ID    , 11 },
        { 0x0000240000000ULL + chip_addr_offset, 0x00002FFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 12 },
        { 0x0000300000000ULL + chip_addr_offset, 0x0000307FFFFFFULL + chip_addr_offset, SYSCTRL_TCU_PMNI_ID    , 13 },
        { 0x0000308000000ULL + chip_addr_offset, 0x0000FFFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID    , 14 },
        { mhu_send_frame_rss.base_0,             mhu_send_frame_rss.end_0,              SYSCTRL_APP_AMNI_ID    , 15 },
        { mhu_send_frame_rss.base_1,             mhu_send_frame_rss.end_1,              SYSCTRL_APP_AMNI_ID    , 16 },
        { mhu_send_frame_rss.base_2,             mhu_send_frame_rss.end_2,              SYSCTRL_APP_AMNI_ID    , 17 },
        { 0x0004000000000ULL,                    0x0FFFFFFFFFFFFULL,                    SYSCTRL_APP_AMNI_ID    , 18 },
        { 0x1000000000000ULL,                    0x10000FFFFFFFFULL,                    SYSCTRL_RSS_SCP_AMNI_ID, 19 },
        { 0x2000000000000ULL,                    0x20000FFFFFFFFULL,                    SYSCTRL_RSS_MCP_AMNI_ID, 20 }
     };

     struct psam_region_cfgs scp_asni[] = {
        { 0x0000000000000ULL + chip_addr_offset, 0x000002A5AFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    0  },
        { 0x000002A5B0000ULL + chip_addr_offset, 0x000002A60FFFFULL + chip_addr_offset, SYSCTRL_RSM_PMNI_ID,    1  },
        { 0x000002A610000ULL + chip_addr_offset, 0x000002ABFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    2  },
        { 0x000002AC30000ULL + chip_addr_offset, 0x000002EFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    3  },
        { 0x000002F000000ULL + chip_addr_offset, 0x000002F3FFFFFULL + chip_addr_offset, SYSCTRL_RSM_AMNI_ID,    4  },
        { 0x000002F400000ULL + chip_addr_offset, 0x00000FFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    5  },
        { 0x0000100000000ULL + chip_addr_offset, 0x000017FFFFFFFULL + chip_addr_offset, SYSCTRL_CMN_PMNI_ID,    6  },
        { 0x0000180000000ULL + chip_addr_offset, 0x00001BFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    7  },
        { 0x00001C0000000ULL + chip_addr_offset, 0x00001FFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    8  },
        { 0x0000200000000ULL + chip_addr_offset, 0x000023FFFFFFFULL + chip_addr_offset, SYSCTRL_LCP_AMNI_ID,    9  },
        { 0x0000240000000ULL + chip_addr_offset, 0x00002FFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    10 },
        { 0x0000300000000ULL + chip_addr_offset, 0x0000FFFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,    11 },
        { mhu_send_frame_scp.base_0,             mhu_send_frame_scp.end_0,              SYSCTRL_APP_AMNI_ID,    12 },
        { mhu_send_frame_scp.base_1,             mhu_send_frame_scp.end_1,              SYSCTRL_APP_AMNI_ID,    13 },
        { mhu_send_frame_scp.base_2,             mhu_send_frame_scp.end_2,              SYSCTRL_APP_AMNI_ID,    14 },
        { 0x0004000000000ULL,                    0x0FFFFFFFFFFFFULL,                    SYSCTRL_APP_AMNI_ID,    15 },
     };

     struct psam_region_cfgs mcp_asni[] = {
        { 0x0000000000000ULL + chip_addr_offset, 0x000002A42FFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     0  },
        { 0x000002A430000ULL + chip_addr_offset, 0x000002A43FFFFULL + chip_addr_offset, SYSCTRL_APP_SCP_AMNI_ID, 1  },
        { 0x000002A440000ULL + chip_addr_offset, 0x000002A49FFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     2  },
        { 0x000002A4A0000ULL + chip_addr_offset, 0x000002A4AFFFFULL + chip_addr_offset, SYSCTRL_APP_SCP_AMNI_ID, 3  },
        { 0x000002A4B0000ULL + chip_addr_offset, 0x000002A5AFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     4  },
        { 0x000002A5B0000ULL + chip_addr_offset, 0x000002A60FFFFULL + chip_addr_offset, SYSCTRL_RSM_PMNI_ID,     5  },
        { 0x000002A610000ULL + chip_addr_offset, 0x000002A7FFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     6  },
        { 0x000002A800000ULL + chip_addr_offset, 0x000002A80FFFFULL + chip_addr_offset, SYSCTRL_APP_SCP_AMNI_ID, 7  },
        { 0x000002A810000ULL + chip_addr_offset, 0x000002AC5FFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     8  },
        { 0x000002AC90000ULL + chip_addr_offset, 0x000002EFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     9  },
        { 0x000002F000000ULL + chip_addr_offset, 0x000002F3FFFFFULL + chip_addr_offset, SYSCTRL_RSM_AMNI_ID,     10 },
        { 0x000002F400000ULL + chip_addr_offset, 0x00000FFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     11 },
        { 0x0000100000000ULL + chip_addr_offset, 0x000017FFFFFFFULL + chip_addr_offset, SYSCTRL_CMN_PMNI_ID,     12 },
        { 0x0000180000000ULL + chip_addr_offset, 0x00001BFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     13 },
        { 0x00001C0000000ULL + chip_addr_offset, 0x00001FFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     14 },
        { 0x0000240000000ULL + chip_addr_offset, 0x00002FFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     15 },
        { 0x0000308000000ULL + chip_addr_offset, 0x0000FFFFFFFFFULL + chip_addr_offset, SYSCTRL_APP_AMNI_ID,     16 },
        { mhu_send_frame_mcp.base_0,             mhu_send_frame_mcp.end_0,              SYSCTRL_APP_AMNI_ID,     17 },
        { mhu_send_frame_mcp.base_1,             mhu_send_frame_mcp.end_1,              SYSCTRL_APP_AMNI_ID,     18 },
        { mhu_send_frame_mcp.base_2,             mhu_send_frame_mcp.end_2,              SYSCTRL_APP_AMNI_ID,     19 },
        { 0x0004000000000ULL,                    0x0FFFFFFFFFFFFULL,                    SYSCTRL_APP_AMNI_ID,     20 },
     };

    struct psam_region_cfgs app_asni[] = {
        { 0x00002A430000, 0x00002A95FFFF, SYSCTRL_APP_SCP_AMNI_ID, 0 },
        { 0x00002AA00000, 0x00002AA5FFFF, SYSCTRL_APP_MCP_AMNI_ID, 1 },
        { 0x00002AB00000, 0x00002AC2FFFF, SYSCTRL_APP_SCP_AMNI_ID, 2 },
        { 0x00002AC60000, 0x00002AC8FFFF, SYSCTRL_APP_MCP_AMNI_ID, 3 },
        { 0x00002ACC0000, 0x00002B12FFFF, SYSCTRL_APP_SCP_AMNI_ID, 4 },
    };

    struct psam_region_cfgs lcp_asni[] = {
        { 0x000000000000, 0x0000FFFFFFFF, SYSCTRL_LCP_SCP_AMNI_ID, 0 },
    };

    struct psam_cfgs psam_table[] = {
        {
            .sam = {.base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                        TOWER_NCI_ASNI, SYSCTRL_RSS_ASNI_ID, TOWER_NCI_PSAM) },
            .nh_region_count = sizeof(rss_asni) /
                               sizeof(struct psam_region_cfgs),
            .regions = rss_asni,
        },
        {
            .sam = {.base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                        TOWER_NCI_ASNI, SYSCTRL_SCP_ASNI_ID, TOWER_NCI_PSAM) },
            .nh_region_count = sizeof(scp_asni) /
                               sizeof(struct psam_region_cfgs),
            .regions = scp_asni,
        },
        {
            .sam = {.base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                        TOWER_NCI_ASNI, SYSCTRL_MCP_ASNI_ID, TOWER_NCI_PSAM) },
            .nh_region_count = sizeof(mcp_asni) /
                               sizeof(struct psam_region_cfgs),
            .regions = mcp_asni,
        },
        {
            .sam = {.base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                        TOWER_NCI_ASNI, SYSCTRL_APP_ASNI_ID, TOWER_NCI_PSAM) },
            .nh_region_count = sizeof(app_asni) /
                               sizeof(struct psam_region_cfgs),
            .regions = app_asni,
        },
        {
            .sam = {.base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                        TOWER_NCI_ASNI, SYSCTRL_LCP_ASNI_ID, TOWER_NCI_PSAM) },
            .nh_region_count = sizeof(lcp_asni) /
                               sizeof(struct psam_region_cfgs),
            .regions = lcp_asni,
        },
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
    struct apu_region_cfgs rsm_pmni[] = {
        { 0x00002A5B0000 + chip_addr_offset, 0x00002A60FFFF + chip_addr_offset, T_NCI_BACKGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
        { 0x00002A5B0000 + chip_addr_offset, 0x00002A5BFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_SEC_RW,   T_NCI_SELECT_ALL_IDS, 1, { 0, 0, 0, 0 } },
        { 0x00002A5D0000 + chip_addr_offset, 0x00002A5DFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_SEC_RW,   T_NCI_SELECT_ALL_IDS, 2, { 0, 0, 0, 0 } },
        { 0x00002A5F0000 + chip_addr_offset, 0x00002A5FFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_SEC_RW,   T_NCI_SELECT_ALL_IDS, 3, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs rsm_amni[] = {
        { 0x00002F000000 + chip_addr_offset, 0x00002F0FFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
        { 0x00002F100000 + chip_addr_offset, 0x00002F1FFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_SEC_RW,   T_NCI_SELECT_ALL_IDS, 1, { 0, 0, 0, 0 } },
        { 0x00002F200000 + chip_addr_offset, 0x00002F2FFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_REALM_RW, T_NCI_SELECT_ALL_IDS, 2, { 0, 0, 0, 0 } },
        { 0x00002F300000 + chip_addr_offset, 0x00002F3FFFFF + chip_addr_offset, T_NCI_FOREGROUND, T_NCI_ROOT_RW,  T_NCI_SELECT_ALL_IDS, 3, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs lcp_asni[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs app_asni[] = {
        { 0x00002A430000, 0x00002B12FFFF, T_NCI_BACKGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
        { 0x00002A940000, 0x00002A95FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,  T_NCI_SELECT_ALL_IDS, 1, { 0, 0, 0, 0 } },
        { 0x00002AA40000, 0x00002AA5FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,  T_NCI_SELECT_ALL_IDS, 2, { 0, 0, 0, 0 } },
        { 0x00002AB40000, 0x00002AB5FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,  T_NCI_SELECT_ALL_IDS, 3, { 0, 0, 0, 0 } },
        { 0x00002AB60000, 0x00002AB7FFFF, T_NCI_FOREGROUND, T_NCI_REALM_RW, T_NCI_SELECT_ALL_IDS, 4, { 0, 0, 0, 0 } },
        { 0x00002AC00000, 0x00002ACEFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,  T_NCI_SELECT_ALL_IDS, 5, { 0, 0, 0, 0 } },
        { 0x00002B100000, 0x00002B12FFFF, T_NCI_FOREGROUND, T_NCI_SEC_RW,   T_NCI_SELECT_ALL_IDS, 6, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs rss_scp_amni[] = {
        { 0x1000000000000, 0x10000FFFFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs rss_mcp_amni[] = {
        { 0x2000000000000, 0x20000FFFFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs app_amni[] = {
        { 0x000180002000, 0x000183FFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { SYSCTRL_RSS_APU_ID, SYSCTRL_SCP_APU_ID, SYSCTRL_MCP_APU_ID, SYSCTRL_DAP_APU_ID } },
    };

    struct apu_cfgs apu_table[] = {
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                        TOWER_NCI_PMNI, SYSCTRL_RSM_PMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(rsm_pmni) / sizeof(struct apu_region_cfgs),
            .regions = rsm_pmni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                        TOWER_NCI_AMNI, SYSCTRL_RSM_AMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(rsm_amni) / sizeof(struct apu_region_cfgs),
            .regions = rsm_amni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                        TOWER_NCI_ASNI, SYSCTRL_LCP_ASNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(lcp_asni) / sizeof(struct apu_region_cfgs),
            .regions = lcp_asni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                        TOWER_NCI_ASNI, SYSCTRL_APP_ASNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(app_asni) / sizeof(struct apu_region_cfgs),
            .regions = app_asni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                      TOWER_NCI_AMNI, SYSCTRL_RSS_SCP_AMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(rss_scp_amni) /
                            sizeof(struct apu_region_cfgs),
            .regions = rss_scp_amni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(dis_tree,
                      TOWER_NCI_AMNI, SYSCTRL_RSS_MCP_AMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(rss_mcp_amni) /
                            sizeof(struct apu_region_cfgs),
            .regions = rss_mcp_amni
        },
    };

    if (program_apu_table(apu_table, sizeof(apu_table) /
                                     sizeof(struct apu_cfgs)) != 0) {
        return 1;
    }

    BOOT_LOG_INF("System Control Block APU programming done");

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

    tower_nci_discovery(&dis_tree, nci_addr);

    program_sysctrl_psam(&dis_tree, nci_addr, chip_addr_offset);
    program_sysctrl_apu(&dis_tree, nci_addr, chip_addr_offset);

    free_discovery_tree(dis_tree.child);

    return 0;
}

uint32_t program_periph_tower_nci(uint32_t nci_addr, uint64_t chip_addr_offset)
{
    struct tower_nci_discovery_tree_t dis_tree = {
        .type = TOWER_NCI_CFGNI,
        .id = 0,
        .address = 0x0,
        .sibling = NULL,
    };

    tower_nci_discovery(&dis_tree, nci_addr);

    enum tower_nci_err_t err;

    // RAM APU
    struct apu_region_cfgs ram_amni[] = {
        { 0x0000000000, 0x0007FFFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
    };

    // UART APUs
    struct apu_region_cfgs nsuart0_pmni[] = {
        { 0x002A400000, 0x002A40FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs secuart_pmni[] = {
        { 0x002A410000, 0x002A41FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs nsuart1_pmni[] = {
        { 0x002A420000, 0x002A42FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
    };

    // WDOG APUs
    struct apu_region_cfgs nsgenwdog_pmni[] = {
        { 0x002A440000, 0x002A44FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
        { 0x002A450000, 0x002A45FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 1, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs rootgenwdog_pmni[] = {
        { 0x002A460000, 0x002A46FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
        { 0x002A470000, 0x002A47FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW, T_NCI_SELECT_ALL_IDS, 1, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs secgenwdog_pmni[] = {
        { 0x002A480000, 0x002A48FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
        { 0x002A490000, 0x002A49FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW, T_NCI_SELECT_ALL_IDS, 1, { 0, 0, 0, 0 } },
    };

    // ECCREG APU
    struct apu_region_cfgs eccreg_pmni[] = {
        { 0x002A4B0000, 0x002A4BFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW,   T_NCI_SELECT_ALL_IDS, 0 , { 0, 0, 0, 0 } },
        { 0x002A4C0000, 0x002A4CFFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 T_NCI_SELECT_ALL_IDS, 1 , { 0, 0, 0, 0 } },
        { 0x002A4D0000, 0x002A4DFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  T_NCI_SELECT_ALL_IDS, 2 , { 0, 0, 0, 0 } },
        { 0x002A4E0000, 0x002A4EFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_REALM_RW, T_NCI_SELECT_ALL_IDS, 3 , { 0, 0, 0, 0 } },
        { 0x002A4F0000, 0x002A4FFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW,   T_NCI_SELECT_ALL_IDS, 4 , { 0, 0, 0, 0 } },
        { 0x002A500000, 0x002A50FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 T_NCI_SELECT_ALL_IDS, 5 , { 0, 0, 0, 0 } },
        { 0x002A510000, 0x002A51FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  T_NCI_SELECT_ALL_IDS, 6 , { 0, 0, 0, 0 } },
        { 0x002A520000, 0x002A52FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_REALM_RW, T_NCI_SELECT_ALL_IDS, 7 , { 0, 0, 0, 0 } },
        { 0x002A530000, 0x002A53FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW,   T_NCI_SELECT_ALL_IDS, 8 , { 0, 0, 0, 0 } },
        { 0x002A540000, 0x002A54FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 T_NCI_SELECT_ALL_IDS, 9 , { 0, 0, 0, 0 } },
        { 0x002A550000, 0x002A55FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  T_NCI_SELECT_ALL_IDS, 10, { 0, 0, 0, 0 } },
        { 0x002A560000, 0x002A56FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_REALM_RW, T_NCI_SELECT_ALL_IDS, 11, { 0, 0, 0, 0 } },
        { 0x002A570000, 0x002A57FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW,   T_NCI_SELECT_ALL_IDS, 12, { 0, 0, 0, 0 } },
        { 0x002A580000, 0x002A58FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM,                 T_NCI_SELECT_ALL_IDS, 13, { 0, 0, 0, 0 } },
        { 0x002A590000, 0x002A59FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW,                  T_NCI_SELECT_ALL_IDS, 14, { 0, 0, 0, 0 } },
        { 0x002A5A0000, 0x002A5AFFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_REALM_RW, T_NCI_SELECT_ALL_IDS, 15, { 0, 0, 0, 0 } },
    };

    // GTIMER APUs
    struct apu_region_cfgs gtimerctrl_pmni[] = {
        { 0x002A810000, 0x002A81FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs secgtimer_pmni[] = {
        { 0x002A820000, 0x002A82FFFF, T_NCI_FOREGROUND, T_NCI_ROOT_RW | T_NCI_SEC_RW, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
    };

    struct apu_region_cfgs nsgtimer_pmni[] = {
        { 0x002A830000, 0x002A83FFFF, T_NCI_FOREGROUND, T_NCI_ALL_PERM, T_NCI_SELECT_ALL_IDS, 0, { 0, 0, 0, 0 } },
    };

    struct apu_cfgs apu_table[] = {
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                      TOWER_NCI_AMNI, PERIPH_RAM_AMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(ram_amni) / sizeof(struct apu_region_cfgs),
            .regions = ram_amni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                      TOWER_NCI_PMNI, PERIPH_NSUART0_PMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(nsuart0_pmni) /
                                sizeof(struct apu_region_cfgs),
            .regions = nsuart0_pmni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                      TOWER_NCI_PMNI, PERIPH_SECUART_PMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(secuart_pmni) /
                                sizeof(struct apu_region_cfgs),
            .regions = secuart_pmni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                      TOWER_NCI_PMNI, PERIPH_NSUART1_PMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(nsuart1_pmni) /
                                sizeof(struct apu_region_cfgs),
            .regions = nsuart1_pmni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                     TOWER_NCI_PMNI, PERIPH_NSGENWDOG_PMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(nsgenwdog_pmni) /
                                sizeof(struct apu_region_cfgs),
            .regions = nsgenwdog_pmni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                      TOWER_NCI_PMNI, PERIPH_ROOTGENWDOG_PMNI_ID,
                      TOWER_NCI_APU) },
            .region_count = sizeof(rootgenwdog_pmni) /
                                sizeof(struct apu_region_cfgs),
            .regions = rootgenwdog_pmni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                      TOWER_NCI_PMNI, PERIPH_SECGENWDOG_PMNI_ID,
                      TOWER_NCI_APU) },
            .region_count = sizeof(secgenwdog_pmni) /
                                sizeof(struct apu_region_cfgs),
            .regions = secgenwdog_pmni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                      TOWER_NCI_PMNI, PERIPH_ECCREG_PMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(eccreg_pmni) /
                                sizeof(struct apu_region_cfgs),
            .regions = eccreg_pmni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                      TOWER_NCI_PMNI, PERIPH_GTIMERCTRL_PMNI_ID,
                      TOWER_NCI_APU) },
            .region_count = sizeof(gtimerctrl_pmni) /
                                sizeof(struct apu_region_cfgs),
            .regions = gtimerctrl_pmni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                      TOWER_NCI_PMNI, PERIPH_SECGTIMER_PMNI_ID,
                      TOWER_NCI_APU) },
            .region_count = sizeof(secgtimer_pmni) /
                                sizeof(struct apu_region_cfgs),
            .regions = secgtimer_pmni
        },
        {
            .apu = { .base = nci_addr + tower_nci_fetch_offset_address(&dis_tree,
                      TOWER_NCI_PMNI, PERIPH_NSGTIMER_PMNI_ID, TOWER_NCI_APU) },
            .region_count = sizeof(nsgtimer_pmni) /
                                sizeof(struct apu_region_cfgs),
            .regions = nsgtimer_pmni
        },
    };

    if (program_apu_table(apu_table, sizeof(apu_table) /
                                     sizeof(struct apu_cfgs)) != 0) {
        return 1;
    }

    BOOT_LOG_INF("Peripheral Block APU programming done");

    free_discovery_tree(dis_tree.child);

    return 0;
}
