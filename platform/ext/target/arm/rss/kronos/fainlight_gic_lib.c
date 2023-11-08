/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include "array.h"
#include "bootutil/bootutil_log.h"
#include "fainlight_gic_drv.h"
#include "fainlight_gic_lib.h"

#include <stddef.h>

/* Kronos has view-0/1/2/3 for Safety Island */
#define GIC_MV_MAX_VIEW_NUM     4
/*
 * Kronos GIC View-0 has a contigous region for all
 * Safety Island GIC re-distributors:
 * Cluster-0: 1 re-distributors for 1 PE
 * Cluster-1: 2 re-distributors for 2 PE
 * Cluster-2: 4 re-distributors for 4 PE
 */
#define GIC_MV_MAX_REDIST_NUM  7

static struct gic_mv_dev_t gic_view0;

/* GIC Multiple View, Safety Island Cluster PE to view mapping table */
static struct gic_mv_p2v_map view_pe_map[] = {
    /* Cluster 0 PE for View 1 */
    { 0x0, 1 },
    /* Cluster 1 PE for View 2 */
    { 0x10000, 2 },
    { 0x10100, 2 },
    /* Cluster 2 PE for View 3 */
    { 0x20000, 3 },
    { 0x20100, 3 },
    { 0x20200, 3 },
    { 0x20300, 3 }
};

static struct gic_mv_i2v_map view_spi_map[] = {
    /* Safety Island CL0 SPI for view 1 */
    { 34, 1 }, /* System Timer for Cluster 0 */
    { 37, 1 }, /* 1st interrupt of System Watchdog for Cluster 0 */
    { 40, 1 }, /* UART for Cluster 0 */
    { 88, 1 }, /* MHU to PC Cluster 0 NS */
    { 89, 1 },
    { 90, 1 },
    { 91, 1 },
    { 92, 1 }, /* MHU from PC Cluster 0 NS */
    { 93, 1 },
    { 94, 1 },
    { 95, 1 },
    { 96, 1 }, /* MHU to PC Cluster 0 S */
    { 97, 1 },
    { 98, 1 },
    { 99, 1 },
    { 100, 1 }, /* MHU from PC Cluster 0 S */
    { 101, 1 },
    { 102, 1 },
    { 103, 1 },
    { 104, 1 }, /* MHU to RSS Cluster 0 S */
    { 105, 1 },
    { 106, 1 },
    { 107, 1 },
    { 108, 1 },
    { 109, 1 },
    { 110, 1 },
    { 111, 1 }, /* MHU from RSS Cluster 0 S */
    { 112, 1 }, /* MHU CL0 To CL1 Sender */
    { 114, 1 }, /* MHU CL0 To CL2 Sender */
    { 117, 1 }, /* MHU CL1 To CL0 Receiver */
    { 121, 1 }, /* MHU CL2 To CL0 Receiver */
    { 130, 1 }, /* PC STC 1 */
    { 131, 1 }, /* PC STC 0 */
    { 132, 1 }, /* PC PIK  */
    { 133, 1 }, /* QSPI */
    { 136, 1 }, /* Ethernet 1 */
    { 137, 1 }, /* Ethernet 0 */
    { 138, 1 }, /* DMA 0 */
    { 141, 1 }, /* ATU Interrupt CL0 */
    { 143, 1 }, /* NCI main */
    { 151, 1 }, /* MHU SCP to CL0 receiver */
    { 152, 1 }, /* MHU CL0 to SCP sender */
    { 160, 1 }, /* PPU Interrupt for SI CL0 Core 0 */
    { 164, 1 }, /* PPU Interrupt for SI CL0 */
    { 165, 1 }, /* PMU Interrupt for SI CL0 */

    /* Safety Island CL1 SPI for view 2 */
    { 33, 2 }, /* System Timer for Cluster 1 */
    { 36, 2 }, /* 1st interrupt of System Watchdog for Cluster 1 */
    { 39, 2 }, /* UART for Cluster 1 */
    { 72, 2 }, /* MHU to PC Cluster 1 NS */
    { 73, 2 },
    { 74, 2 },
    { 75, 2 },
    { 76, 2 }, /* MHU from PC Cluster 1 NS */
    { 77, 2 },
    { 78, 2 },
    { 79, 2 },
    { 80, 2 }, /* MHU to PC Cluster 1 S */
    { 81, 2 },
    { 82, 2 },
    { 83, 2 },
    { 84, 2 }, /* MHU from PC Cluster 1 S */
    { 85, 2 },
    { 86, 2 },
    { 87, 2 },
    { 113, 2 }, /* MHU CL0 To CL1 Receiver */
    { 116, 2 }, /* MHU CL1 To CL0 Sender */
    { 118, 2 }, /* MHU CL1 To CL2 Sender */
    { 123, 2 }, /* MHU CL2 To CL1 Receiver */
    { 124, 2 }, /* MHU to RSS Cluster 1 S */
    { 125, 2 }, /* MHU from RSS Cluster 1 S */
    { 128, 2 }, /* FMU */
    { 129, 2 }, /* SSU */
    { 134, 2 }, /* FMU Non-Critical */
    { 139, 2 }, /* DMA 1 */
    { 145, 2 }, /* ATU Interrupt CL1 */
    { 153, 2 }, /* MHU SCP to CL1 receiver */
    { 154, 2 }, /* MHU CL1 to SCP sender */
    { 168, 2 }, /* PPU Interrupt for SI CL1 Core 0 */
    { 169, 2 }, /* PPU Interrupt for SI CL1 Core 1 */
    { 172, 2 }, /* PPU Interrupt for SI CL1 */
    { 173, 2 }, /* PMU Interrupt for SI CL1 */

    /* Safety Island CL2 SPI for view 3 */
    { 32, 3 }, /* System Timer for Cluster 2 */
    { 35, 3 }, /* 1st interrupt of System Watchdog for *nCluster 2 */
    { 38, 3 }, /* UART for Cluster 2 */
    { 48, 3 }, /* MHU to PC Cluster 2 NS */
    { 49, 3 },
    { 50, 3 },
    { 51, 3 },
    { 52, 3 }, /* MHU from PC Cluster 2 NS */
    { 53, 3 },
    { 54, 3 },
    { 55, 3 },
    { 56, 3 }, /* MHU to PC Cluster 2 S */
    { 57, 3 },
    { 58, 3 },
    { 59, 3 },
    { 60, 3 }, /* MHU from PC Cluster 2 S */
    { 61, 3 },
    { 62, 3 },
    { 63, 3 },
    { 64, 3 }, /* MHU to RSS Cluster 2 S */
    { 65, 3 },
    { 66, 3 },
    { 67, 3 },
    { 68, 3 }, /* MHU from RSS Cluster 2 S */
    { 69, 3 },
    { 70, 3 },
    { 71, 3 },
    { 115, 3 }, /* MHU CL0 To CL2 Receiver */
    { 119, 3 }, /* MHU CL1 To CL2 Receiver */
    { 120, 3 }, /* MHU CL2 To CL0 Sender */
    { 122, 3 }, /* MHU CL2 To CL1 Sender */
    { 140, 3 }, /* DMA 2 */
    { 146, 3 }, /* ATU Interrupt CL2 */
    { 155, 3 }, /* MHU SCP to CL2 receiver */
    { 156, 3 }, /* MHU CL2 to SCP sender */
    { 176, 3 }, /* PPU Interrupt for SI CL2 Core 0 */
    { 177, 3 }, /* PPU Interrupt for SI CL2 Core 1 */
    { 178, 3 }, /* PPU Interrupt for SI CL2 Core 2 */
    { 179, 3 }, /* PPU Interrupt for SI CL2 Core 3 */
    { 180, 3 }, /* PPU Interrupt for SI CL2 */
    { 181, 3 }, /* PMU Interrupt for SI CL2 */
};

int gic_multiple_view_programming(void)
{
    int ret;

    BOOT_LOG_INF("GIC: Multiple Views configure PE ...");
    ret = gic_multiple_view_config_pe(&gic_view0,
                                      view_pe_map,
                                      ARRAY_SIZE(view_pe_map));
    if (ret) {
        BOOT_LOG_ERR("GIC: configure PE views failed!");
        return ret;
    }

    BOOT_LOG_INF("GIC: Multiple Views configure SPI ...");
    ret = gic_multiple_view_config_spi(&gic_view0,
                                       view_spi_map,
                                       ARRAY_SIZE(view_spi_map));
    if (ret) {
        BOOT_LOG_ERR("GIC: configure SPI views failed!");
        return ret;
    }

    BOOT_LOG_INF("GIC: Multiple Views configuration done!");

    return 0;
}

int gic_multiple_view_probe(uint32_t view0_base)
{
    int ret;

    ret = gic_multiple_view_device_probe(&gic_view0, view0_base,
                                         GIC_MV_MAX_VIEW_NUM,
                                         GIC_MV_MAX_REDIST_NUM);
    if (ret) {
        BOOT_LOG_ERR("Probe Multiple View GIC device failed!");
        return ret;
    }

    return 0;
}
