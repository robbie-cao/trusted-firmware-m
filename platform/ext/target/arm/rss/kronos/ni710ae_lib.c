/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "bootutil/bootutil_log.h"
#include "ni710ae_drv.h"
#include "ni710ae_lib.h"
#include "host_device_definition.h"

#include <stddef.h>

#define APU_TUPLE(cfg, type, id)                                              \
    {                                                                         \
        .apu = {.base = nci_addr + ni710ae_fetch_offset_address(dis_tree, \
                                            type, id, NI710AE_APU) },     \
        .region_count = sizeof(cfg) / sizeof(struct apu_region_cfgs),         \
        .regions = cfg,                                                       \
    }

/* Interface ID of xSNI components */
enum si_xSNI_ids {
    SI_ASNI_CLUSTER0_MM_ID,
    SI_ASNI_CLUSTER0_SPP_ID,
    SI_ASNI_CLUSTER1_MM_ID,
    SI_ASNI_CLUSTER1_SPP_ID,
    SI_ASNI_CLUSTER2_MM_ID,
    SI_ASNI_CLUSTER2_SPP_ID,
    SI_ASNI_DMA_IO0_ID,
    SI_ASNI_DMA_IO1_ID,
    SI_ASNI_DMA_IO2_ID,
    SI_ASNI_ETHERNET_DATA0_ID,
    SI_ASNI_ETHERNET_DATA1_ID,
    SI_ASNI_IO_EXTENSION_ID,
    SI_ASNI_PC_SHRD_ID,
    SI_ASNI_RSS_MM_ID,
    SI_ASNI_RSS_PP_ID,
};

/* Interface ID of xMNI components */
enum si_xMNI_ids {
    SI_AMNI_CLUSTER0_ACEL_ID,
    SI_AMNI_CLUSTER0_UTILITY_ID,
    SI_AMNI_CLUSTER1_ACEL_ID,
    SI_AMNI_CLUSTER1_UTILITY_ID,
    SI_AMNI_CLUSTER2_ACEL_ID,
    SI_AMNI_CLUSTER2_UTILITY_ID,
    SI_AMNI_ETHERNET_CTRL0_ID,
    SI_AMNI_ETHERNET_CTRL1_ID,
    SI_AMNI_GIC_SI_ID,
    SI_AMNI_PCMA0_ID,
    SI_AMNI_PCMA1_ID,
    SI_AMNI_PCPA0_ID,
    SI_AMNI_PCPA1_ID,
    SI_AMNI_QSPI_ID,
    SI_AMNI_SRAM0_ID,
    SI_AMNI_SRAM1_ID,
    SI_AMNI_SRAM_SHRD_ID,
};

/* APU region config structure */
struct apu_region_cfgs {
    uint64_t base_addr;
    uint64_t end_addr;
    uint32_t br;
    uint32_t perms;
    uint32_t region;
};

/* APU config structure */
struct apu_cfgs {
    struct ni710ae_apu_dev_t apu;
    int region_count;
    struct apu_region_cfgs* regions;
};

uint32_t program_ni710ae_apu_table(struct apu_cfgs apu_table[],
                                   int apu_table_count)
{
    enum ni710ae_err_t err = 0;
    struct apu_region_cfgs* r_map;
    uint32_t a_idx, r_idx, id_idx;
    uint8_t entity_ids[4] = { 0, 0, 0, 0 };

    for (a_idx = 0; a_idx < apu_table_count; ++a_idx) {
        for (r_idx = 0; r_idx < apu_table[a_idx].region_count; ++r_idx) {
            r_map = &apu_table[a_idx].regions[r_idx];
            uint32_t permissions[4] = { r_map->perms, 0, 0, 0 };
            err = ni710ae_apu_initialize_region(&apu_table[a_idx].apu,
                                                r_map->region,
                                                r_map->base_addr,
                                                r_map->end_addr, r_map->br,
                                                permissions, entity_ids,
                                                NCI_ID_VALID_NONE,
                                                NCI_REGION_ENABLE,
                                                NCI_UNLOCK);
            if (err != NI710AE_SUCCESS) {
                return 1;
            }
        }

        err = ni710ae_apu_sync_err_enable(&apu_table[a_idx].apu);
        if (err != NI710AE_SUCCESS) {
            return 1;
        }
        err = ni710ae_apu_enable(&apu_table[a_idx].apu);
        if (err != NI710AE_SUCCESS) {
            return 1;
        }
    }

    return err;
}

/* Program boot time APU for the platform System Control NI710AE */
static uint32_t
program_si_apu_boot_time(struct ni710ae_discovery_tree_t* dis_tree,
                         uint64_t nci_addr, uint64_t chip_addr_offset)
{
    struct apu_region_cfgs asni_rss_mm[] = {
        /* GIC View 0 */
        { 0x30000000, 0x303FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
        /* NCI CFG space */
        { 0x2A000000, 0x2A0FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 1, },
        /* cluster0_acel */
        { 0x110000000, 0x12FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 2, },
        /* cluster1_acel */
        { 0x130000000, 0x14FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 3, },
        /* cluster2_acel */
        { 0x150000000, 0x16FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 4, },
        /* cluster0_utility */
        { 0x28000000, 0x283FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 5, },
        /* cluster1_utility */
        { 0x28400000, 0x287FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 6, },
        /* cluster2_utility */
        { 0x28800000, 0x28BFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 7, },
        /* qspi */
        { 0x60000000, 0x7FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 8, },
        /* base_periphs */
        /* sysctrl */
        { 0x2A6A0000, 0x2A6CFFFF, NCI_FOREGROUND, NCI_SEC_RW, 11, },
        /* gcounter_ctrl */
        { 0x2A6D0000, 0x2A6DFFFF, NCI_FOREGROUND, NCI_SEC_RW, 12, },
        /* gcounter_read */
        { 0x2A6E0000, 0x2A6EFFFF, NCI_FOREGROUND, NCI_SEC_RW, 13, },
        /* timer_ctrl */
        { 0x2A6F0000, 0x2A6FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 14, },
        /* si_fmu */
        { 0x2A510000, 0x2A51FFFF, NCI_FOREGROUND, NCI_SEC_RW, 15, },
        /* nci_fmu */
        { 0x2A520000, 0x2A52FFFF, NCI_FOREGROUND, NCI_SEC_RW, 16, },
        /* gic_fmu */
        { 0x2A530000, 0x2A53FFFF, NCI_FOREGROUND, NCI_SEC_RW, 17, },
        /* pc_gic_fmu */
        { 0x2A570000, 0x2A57FFFF, NCI_FOREGROUND, NCI_SEC_RW, 18, },
        /* ssu */
        { 0x2A500000, 0x2A50FFFF, NCI_FOREGROUND, NCI_SEC_RW, 19, },
        /* pc_bist0 */
        { 0x2B200000, 0x2B20FFFF, NCI_FOREGROUND, NCI_SEC_RW, 20, },
        /* pc_bist1 */
        { 0x2B210000, 0x2B21FFFF, NCI_FOREGROUND, NCI_SEC_RW, 21, },
        /* pc_pik */
        { 0x2A640000, 0x2A64FFFF, NCI_FOREGROUND, NCI_SEC_RW, 22, },
    };

    struct apu_region_cfgs asni_rss_pp[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster0_acel[] = {
        { 0x110000000, 0x12FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster0_utility[] = {
        { 0x28000000, 0x283FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster1_acel[] = {
        { 0x130000000, 0x14FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster1_utility[] = {
        { 0x28400000, 0x287FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster2_acel[] = {
        { 0x150000000, 0x16FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster2_utility[] = {
        { 0x28800000, 0x28BFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_gic_si[] = {
        { 0x30000000, 0x30FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_qspi[] = {
        { 0x60000000, 0x7FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_cfgs apu_table[] = {
        APU_TUPLE(asni_rss_mm, NI710AE_ASNI, SI_ASNI_RSS_MM_ID),
        APU_TUPLE(asni_rss_pp, NI710AE_ASNI, SI_ASNI_RSS_PP_ID),
        APU_TUPLE(amni_cluster0_acel, NI710AE_AMNI, SI_AMNI_CLUSTER0_ACEL_ID),
        APU_TUPLE(amni_cluster0_utility, NI710AE_AMNI, SI_AMNI_CLUSTER0_UTILITY_ID),
        APU_TUPLE(amni_cluster1_acel, NI710AE_AMNI, SI_AMNI_CLUSTER1_ACEL_ID),
        APU_TUPLE(amni_cluster1_utility, NI710AE_AMNI, SI_AMNI_CLUSTER1_UTILITY_ID),
        APU_TUPLE(amni_cluster2_acel, NI710AE_AMNI, SI_AMNI_CLUSTER2_ACEL_ID),
        APU_TUPLE(amni_cluster2_utility, NI710AE_AMNI, SI_AMNI_CLUSTER2_UTILITY_ID),
        APU_TUPLE(amni_gic_si, NI710AE_AMNI, SI_AMNI_GIC_SI_ID),
        APU_TUPLE(amni_qspi, NI710AE_AMNI, SI_AMNI_QSPI_ID),
    };

    if (program_ni710ae_apu_table(apu_table, sizeof(apu_table) /
                                             sizeof(struct apu_cfgs)) != 0) {
        return 1;
    }

    BOOT_LOG_INF("Safety Island boot time APU programming done");

    return 0;
}

/* Program runtime APU for the platform System Control NI710AE */
static uint32_t
program_si_apu_runtime(struct ni710ae_discovery_tree_t* dis_tree,
                       uint64_t nci_addr, uint64_t chip_addr_offset)
{
    struct apu_region_cfgs asni_cluster0_mm[] = {
        /* GIC View 1 (Cluster 0) */
        { 0x30400000, 0x307FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
        /* PCMA<n> */
        { 0x10000000, 0x17FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 1, },
        { 0x80000000, 0x80FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 2, },
        /* PCPA<n> */
        { 0x18000000, 0x1FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 3, },
        { 0x20000000, 0x27FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 4, },
        /* SRAM0 */
        { 0x40000000, 0x47FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 5, },
        /* SRAM1 */
        { 0x48000000, 0x4FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 6, },
        /* ethernet_ctrl_0 */
        { 0x8100000, 0x81FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 7, },
        /* ethernet_ctrl_1 */
        { 0x8200000, 0x82FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 8, },
        /* pc_shrd */
        { 0x200000000, 0x2000FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 9, },
        /* base_periphs */
        /* sysctrl */
        { 0x2A6A0000, 0x2A6CFFFF, NCI_FOREGROUND, NCI_SEC_RW, 10, },
        /* gcounter_ctrl */
        { 0x2A6D0000, 0x2A6DFFFF, NCI_FOREGROUND, NCI_SEC_RW, 11, },
        /* gcounter_read */
        { 0x2A6E0000, 0x2A6EFFFF, NCI_FOREGROUND, NCI_SEC_RW, 12, },
        /* timer_ctrl */
        { 0x2A6F0000, 0x2A6FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 13, },
        /* si_fmu */
        { 0x2A510000, 0x2A51FFFF, NCI_FOREGROUND, NCI_SEC_RW, 14, },
        /* nci_fmu */
        { 0x2A520000, 0x2A52FFFF, NCI_FOREGROUND, NCI_SEC_RW, 15, },
        /* gic_fmu */
        { 0x2A530000, 0x2A53FFFF, NCI_FOREGROUND, NCI_SEC_RW, 16, },
        /* pc_gic_fmu */
        { 0x2A570000, 0x2A57FFFF, NCI_FOREGROUND, NCI_SEC_RW, 17, },
        /* ssu */
        { 0x2A500000, 0x2A50FFFF, NCI_FOREGROUND, NCI_SEC_RW, 18, },
        /* pc_bist0 */
        { 0x2B200000, 0x2B20FFFF, NCI_FOREGROUND, NCI_SEC_RW, 19, },
        /* pc_bist1 */
        { 0x2B210000, 0x2B21FFFF, NCI_FOREGROUND, NCI_SEC_RW, 20, },
        /* pc_pik */
        { 0x2A640000, 0x2A64FFFF, NCI_FOREGROUND, NCI_SEC_RW, 21, },
        /* cl0_exclusive_mh */
        /* mh_{cl0_pc_send, pc_cl0_rcv}_ns */
        { 0x2A920000, 0x2A93FFFF, NCI_FOREGROUND, NCI_SEC_RW, 22, },
        /* mh_{cl0_rss_send, rss_cl0_rcv}_s */
        { 0x2AA00000, 0x2AA1FFFF, NCI_FOREGROUND, NCI_SEC_RW, 23, },
        /* mh_{cl0_pc_send, pc_cl0_rcv}_s */
        { 0x2AA20000, 0x2AA3FFFF, NCI_FOREGROUND, NCI_SEC_RW, 24, },
        /* mh_{cl0_cl1_send, cl1_cl0_rcv}_s */
        { 0x2AF00000, 0x2AF1FFFF, NCI_FOREGROUND, NCI_SEC_RW, 25, },
        /* mh_{cl0_cl2_send, cl2_cl0_rcv}_s */
        { 0x2AF20000, 0x2AF3FFFF, NCI_FOREGROUND, NCI_SEC_RW, 26, },
        /* cl0_exclusive_others */
        /* timer0 */
        { 0x2A720000, 0x2A72FFFF, NCI_FOREGROUND, NCI_SEC_RW, 27, },
        /* wdog0 */
        { 0x2A700000, 0x2A71FFFF, NCI_FOREGROUND, NCI_SEC_RW, 28, },
        /* uart0 */
        { 0x2A400000, 0x2A40FFFF, NCI_FOREGROUND, NCI_SEC_RW, 29, },
        /* dma_request0 */
        { 0x8000000, 0x800FFFF, NCI_FOREGROUND, NCI_SEC_RW, 30, },
        /* mh_{cl0_scp_send, scp_cl0_rcv}_s */
        { 0x2B000000, 0x2B01FFFF, NCI_FOREGROUND, NCI_SEC_RW, 31, },
    };

    struct apu_region_cfgs asni_cluster0_spp[] = {
        { 0x170000000, 0x177FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_cluster1_mm[] = {
        /* GIC View 2 (Cluster 1) */
        { 0x30800000, 0x30BFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
        /* PCMA<n> */
        { 0x10000000, 0x17FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 1, },
        { 0x80000000, 0x80FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 2, },
        /* PCPA<n> */
        { 0x18000000, 0x1FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 3, },
        { 0x20000000, 0x27FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 4, },
        /* SRAM0 */
        { 0x40000000, 0x47FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 5, },
        /* SRAM1 */
        { 0x48000000, 0x4FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 6, },
        /* pc_shrd */
        { 0x200000000, 0x2000FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 7, },
        /* base_periphs */
        /* sysctrl */
        { 0x2A6A0000, 0x2A6CFFFF, NCI_FOREGROUND, NCI_SEC_RW, 8, },
        /* gcounter_ctrl */
        { 0x2A6D0000, 0x2A6DFFFF, NCI_FOREGROUND, NCI_SEC_RW, 9, },
        /* gcounter_read */
        { 0x2A6E0000, 0x2A6EFFFF, NCI_FOREGROUND, NCI_SEC_RW, 10, },
        /* timer_ctrl */
        { 0x2A6F0000, 0x2A6FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 11, },
        /* si_fmu */
        { 0x2A510000, 0x2A51FFFF, NCI_FOREGROUND, NCI_SEC_RW, 12, },
        /* nci_fmu */
        { 0x2A520000, 0x2A52FFFF, NCI_FOREGROUND, NCI_SEC_RW, 13, },
        /* gic_fmu */
        { 0x2A530000, 0x2A53FFFF, NCI_FOREGROUND, NCI_SEC_RW, 14, },
        /* pc_gic_fmu */
        { 0x2A570000, 0x2A57FFFF, NCI_FOREGROUND, NCI_SEC_RW, 15, },
        /* ssu */
        { 0x2A500000, 0x2A50FFFF, NCI_FOREGROUND, NCI_SEC_RW, 16, },
        /* pc_bist0 */
        { 0x2B200000, 0x2B20FFFF, NCI_FOREGROUND, NCI_SEC_RW, 17, },
        /* pc_bist1 */
        { 0x2B210000, 0x2B21FFFF, NCI_FOREGROUND, NCI_SEC_RW, 18, },
        /* pc_pik */
        { 0x2A640000, 0x2A64FFFF, NCI_FOREGROUND, NCI_SEC_RW, 19, },
        /* cl1_exclusive_mh */
        /* mh_{cl1_pc_send, pc_cl1_rcv}_ns */
        { 0x2AB20000, 0x2AB3FFFF, NCI_FOREGROUND, NCI_SEC_RW, 20, },
        /* mh_{cl1_rss_send, rss_cl1_rcv}_s */
        { 0x2AC00000, 0x2AC1FFFF, NCI_FOREGROUND, NCI_SEC_RW, 21, },
        /* mh_{cl1_pc_send, pc_cl1_rcv}_s */
        { 0x2AC20000, 0x2AC3FFFF, NCI_FOREGROUND, NCI_SEC_RW, 22, },
        /* mh_{cl1_cl0_send, cl0_cl1_rcv}_s */
        { 0x2AF40000, 0x2AF5FFFF, NCI_FOREGROUND, NCI_SEC_RW, 23, },
        /* mh_{cl1_cl2_send, cl2_cl1_rcv}_s */
        { 0x2AF60000, 0x2AF7FFFF, NCI_FOREGROUND, NCI_SEC_RW, 24, },
        /* cl1_exclusive_others */
        /* timer1 */
        { 0x2A7A0000, 0x2A7AFFFF, NCI_FOREGROUND, NCI_SEC_RW, 25, },
        /* wdog1 */
        { 0x2A780000, 0x2A79FFFF, NCI_FOREGROUND, NCI_SEC_RW, 26, },
        /* uart1 */
        { 0x2A410000, 0x2A41FFFF, NCI_FOREGROUND, NCI_SEC_RW, 27, },
        /* dma_request1 */
        { 0x8010000, 0x801FFFF, NCI_FOREGROUND, NCI_SEC_RW, 28, },
        /* mh_{cl1_scp_send, scp_cl1_rcv}_s */
        { 0x2B020000, 0x2B03FFFF, NCI_FOREGROUND, NCI_SEC_RW, 29, },
    };

    struct apu_region_cfgs asni_cluster1_spp[] = {
        { 0x178000000, 0x17FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_cluster2_mm[] = {
        /* GIC View 3 (Cluster 2) */
        { 0x30C00000, 0x30FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
        /* PCMA<n> */
        { 0x10000000, 0x17FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 1, },
        { 0x80000000, 0x80FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 2, },
        /* PCPA<n> */
        { 0x18000000, 0x1FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 3, },
        { 0x20000000, 0x27FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 4, },
        /* SRAM0 */
        { 0x40000000, 0x47FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 5, },
        /* SRAM1 */
        { 0x48000000, 0x4FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 6, },
        /* pc_shrd */
        { 0x200000000, 0x2000FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 7, },
        /* base_periphs */
        /* sysctrl */
        { 0x2A6A0000, 0x2A6CFFFF, NCI_FOREGROUND, NCI_SEC_RW, 8, },
        /* gcounter_ctrl */
        { 0x2A6D0000, 0x2A6DFFFF, NCI_FOREGROUND, NCI_SEC_RW, 9, },
        /* gcounter_read */
        { 0x2A6E0000, 0x2A6EFFFF, NCI_FOREGROUND, NCI_SEC_RW, 10, },
        /* timer_ctrl */
        { 0x2A6F0000, 0x2A6FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 11, },
        /* si_fmu */
        { 0x2A510000, 0x2A51FFFF, NCI_FOREGROUND, NCI_SEC_RW, 12, },
        /* nci_fmu */
        { 0x2A520000, 0x2A52FFFF, NCI_FOREGROUND, NCI_SEC_RW, 13, },
        /* gic_fmu */
        { 0x2A530000, 0x2A53FFFF, NCI_FOREGROUND, NCI_SEC_RW, 14, },
        /* pc_gic_fmu */
        { 0x2A570000, 0x2A57FFFF, NCI_FOREGROUND, NCI_SEC_RW, 15, },
        /* ssu */
        { 0x2A500000, 0x2A50FFFF, NCI_FOREGROUND, NCI_SEC_RW, 16, },
        /* pc_bist0 */
        { 0x2B200000, 0x2B20FFFF, NCI_FOREGROUND, NCI_SEC_RW, 17, },
        /* pc_bist1 */
        { 0x2B210000, 0x2B21FFFF, NCI_FOREGROUND, NCI_SEC_RW, 18, },
        /* pc_pik */
        { 0x2A640000, 0x2A64FFFF, NCI_FOREGROUND, NCI_SEC_RW, 19, },
        /* cl2_exclusive_mh */
        /* mh_{cl2_pc_send, pc_cl2_rcv}_ns */
        { 0x2AD20000, 0x2AD3FFFF, NCI_FOREGROUND, NCI_SEC_RW, 20, },
        /* mh_{cl2_rss_send, rss_cl2_rcv}_s */
        { 0x2AE00000, 0x2AE1FFFF, NCI_FOREGROUND, NCI_SEC_RW, 21, },
        /* mh_{cl2_pc_send, pc_cl2_rcv}_s */
        { 0x2AE20000, 0x2AE3FFFF, NCI_FOREGROUND, NCI_SEC_RW, 22, },
        /* mh_{cl2_cl0_send, cl0_cl2_rcv}_s */
        { 0x2AF80000, 0x2AF9FFFF, NCI_FOREGROUND, NCI_SEC_RW, 23, },
        /* mh_{cl2_cl1_send, cl1_cl2_rcv}_s */
        { 0x2AFA0000, 0x2AFBFFFF, NCI_FOREGROUND, NCI_SEC_RW, 24, },
        /* cl2_exclusive_others */
        /* timer2 */
        { 0x2A820000, 0x2A82FFFF, NCI_FOREGROUND, NCI_SEC_RW, 25, },
        /* wdog2 */
        { 0x2A800000, 0x2A81FFFF, NCI_FOREGROUND, NCI_SEC_RW, 26, },
        /* uart2 */
        { 0x2A420000, 0x2A42FFFF, NCI_FOREGROUND, NCI_SEC_RW, 27, },
        /* dma_request2 */
        { 0x8020000, 0x802FFFF, NCI_FOREGROUND, NCI_SEC_RW, 28, },
        /* mh_{cl2_scp_send, scp_cl2_rcv}_s */
        { 0x2B040000, 0x2B05FFFF, NCI_FOREGROUND, NCI_SEC_RW, 29, },
    };

    struct apu_region_cfgs asni_cluster2_spp[] = {
        { 0x180000000, 0x187FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_dma_io0[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_dma_io1[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_dma_io2[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_ethernet_data0[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_ethernet_data1[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_io_extension[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_pc_shrd[] = {
        { 0x200000000, 0x2000FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_rss_mm[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs asni_rss_pp[] = {
        { 0x000000000000, 0xFFFFFFFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster0_acel[] = {
        { 0x110000000, 0x12FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster0_utility[] = {
        { 0x28000000, 0x283FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster1_acel[] = {
        { 0x130000000, 0x14FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster1_utility[] = {
        { 0x28400000, 0x287FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster2_acel[] = {
        { 0x150000000, 0x16FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_cluster2_utility[] = {
        { 0x28800000, 0x28BFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_ethernet_ctrl0[] = {
        { 0x8100000, 0x81FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_ethernet_ctrl1[] = {
        { 0x8200000, 0x82FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_gic_si[] = {
        { 0x30000000, 0x30FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_pcma0[] = {
        { 0x10000000, 0x17FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_pcma1[] = {
        { 0x80000000, 0x80FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_pcpa0[] = {
        { 0x18000000, 0x1FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_pcpa1[] = {
        { 0x20000000, 0x27FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_qspi[] = {
        { 0x60000000, 0x7FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_sram0[] = {
        { 0x40000000, 0x47FFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_sram1[] = {
        { 0x48000000, 0x4FFFFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_region_cfgs amni_sram_shrd[] = {
        { 0x200000000, 0x2000FFFFF, NCI_FOREGROUND, NCI_SEC_RW, 0, },
    };

    struct apu_cfgs apu_table[] = {
        APU_TUPLE(asni_cluster0_mm, NI710AE_ASNI, SI_ASNI_CLUSTER0_MM_ID),
        APU_TUPLE(asni_cluster0_spp, NI710AE_ASNI, SI_ASNI_CLUSTER0_SPP_ID),
        APU_TUPLE(asni_cluster1_mm, NI710AE_ASNI, SI_ASNI_CLUSTER1_MM_ID),
        APU_TUPLE(asni_cluster1_spp, NI710AE_ASNI, SI_ASNI_CLUSTER1_SPP_ID),
        APU_TUPLE(asni_cluster2_mm, NI710AE_ASNI, SI_ASNI_CLUSTER2_MM_ID),
        APU_TUPLE(asni_cluster2_spp, NI710AE_ASNI, SI_ASNI_CLUSTER2_SPP_ID),
        APU_TUPLE(asni_dma_io0, NI710AE_ASNI, SI_ASNI_DMA_IO0_ID),
        APU_TUPLE(asni_dma_io1, NI710AE_ASNI, SI_ASNI_DMA_IO1_ID),
        APU_TUPLE(asni_dma_io2, NI710AE_ASNI, SI_ASNI_DMA_IO2_ID),
        APU_TUPLE(asni_ethernet_data0, NI710AE_ASNI, SI_ASNI_ETHERNET_DATA0_ID),
        APU_TUPLE(asni_ethernet_data1, NI710AE_ASNI, SI_ASNI_ETHERNET_DATA1_ID),
        APU_TUPLE(asni_io_extension, NI710AE_ASNI, SI_ASNI_IO_EXTENSION_ID),
        APU_TUPLE(asni_pc_shrd, NI710AE_ASNI, SI_ASNI_PC_SHRD_ID),
        APU_TUPLE(asni_rss_mm, NI710AE_ASNI, SI_ASNI_RSS_MM_ID),
        APU_TUPLE(asni_rss_pp, NI710AE_ASNI, SI_ASNI_RSS_PP_ID),
        APU_TUPLE(amni_cluster0_acel, NI710AE_AMNI, SI_AMNI_CLUSTER0_ACEL_ID),
        APU_TUPLE(amni_cluster0_utility, NI710AE_AMNI, SI_AMNI_CLUSTER0_UTILITY_ID),
        APU_TUPLE(amni_cluster1_acel, NI710AE_AMNI, SI_AMNI_CLUSTER1_ACEL_ID),
        APU_TUPLE(amni_cluster1_utility, NI710AE_AMNI, SI_AMNI_CLUSTER1_UTILITY_ID),
        APU_TUPLE(amni_cluster2_acel, NI710AE_AMNI, SI_AMNI_CLUSTER2_ACEL_ID),
        APU_TUPLE(amni_cluster2_utility, NI710AE_AMNI, SI_AMNI_CLUSTER2_UTILITY_ID),
        APU_TUPLE(amni_ethernet_ctrl0, NI710AE_AMNI, SI_AMNI_ETHERNET_CTRL0_ID),
        APU_TUPLE(amni_ethernet_ctrl1, NI710AE_AMNI, SI_AMNI_ETHERNET_CTRL1_ID),
        APU_TUPLE(amni_gic_si, NI710AE_AMNI, SI_AMNI_GIC_SI_ID),
        APU_TUPLE(amni_pcma0, NI710AE_AMNI, SI_AMNI_PCMA0_ID),
        APU_TUPLE(amni_pcma1, NI710AE_AMNI, SI_AMNI_PCMA1_ID),
        APU_TUPLE(amni_pcpa0, NI710AE_AMNI, SI_AMNI_PCPA0_ID),
        APU_TUPLE(amni_pcpa1, NI710AE_AMNI, SI_AMNI_PCPA1_ID),
        APU_TUPLE(amni_qspi, NI710AE_AMNI, SI_AMNI_QSPI_ID),
        APU_TUPLE(amni_sram0, NI710AE_AMNI, SI_AMNI_SRAM0_ID),
        APU_TUPLE(amni_sram1, NI710AE_AMNI, SI_AMNI_SRAM1_ID),
        APU_TUPLE(amni_sram_shrd, NI710AE_AMNI, SI_AMNI_SRAM_SHRD_ID),
    };

    if (program_ni710ae_apu_table(apu_table, sizeof(apu_table) /
                                             sizeof(struct apu_cfgs)) != 0) {
        return 1;
    }

    BOOT_LOG_INF("Safety Island runtime APU programming done");

    return 0;
}

uint8_t prune_ni710ae_node_cb(uint16_t type, uint16_t id, uint16_t child_idx)
{
    return 0;
}

uint32_t program_si_ni710ae_apu(uint32_t nci_addr,
                                    uint64_t chip_addr_offset,
                                    bool is_boot_time)
{
    enum ni710ae_err_t err = 0;
    uint32_t res = 0;

    struct ni710ae_discovery_tree_t dis_tree = {
        .type = NI710AE_CFGNI,
        .id = 0,
        .address = 0x0,
        .sibling = NULL,
    };

    err = ni710ae_discovery(&dis_tree, nci_addr, &prune_ni710ae_node_cb);
    if (err != NI710AE_SUCCESS) {
        free_ni710ae_discovery_tree(dis_tree.child);
        return 1;
    }

    if (is_boot_time)
        res = program_si_apu_boot_time(&dis_tree, nci_addr, chip_addr_offset);
    else
        res = program_si_apu_runtime(&dis_tree, nci_addr, chip_addr_offset);

    free_ni710ae_discovery_tree(dis_tree.child);

    return res;
}
