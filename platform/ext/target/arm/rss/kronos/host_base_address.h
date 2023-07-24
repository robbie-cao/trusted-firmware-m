/*
 * Copyright (c) 2023 Arm Limited. All rights reserved.
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

/**
 * \file host_base_address.h
 * \brief This file defines the host memory map addresses accessed by RSS.
 */

#ifndef __HOST_BASE_ADDRESS_H__
#define __HOST_BASE_ADDRESS_H__

#include "platform_base_address.h"
#include "size_defs.h"

#ifndef ALIGN_UP
#define ALIGN_UP(num, align)    (((num) + ((align) - 1)) & ~((align) - 1))
#endif

#ifndef ALIGN_DOWN
#define ALIGN_DOWN(num, align)  ((num) & ~((align) - 1))
#endif

/* Host Addresses for ATU setup */
/* ATU regions for loading firmware in BL2. Reused per firmware. Not used outside BL2 */
#define RSS_ATU_IMG_HDR_LOAD_ID             0 /*ID to use for region loading the header of an image*/
#define RSS_ATU_IMG_CODE_LOAD_ID            1 /*ID to use for region loading the rest of an image*/
#define RSS_ATU_FW_INIT_ID                  2 /*ID to use for region initialising firmware*/
/* ATU regions left open after BL2 to allow continued access so cannot be reused. */
#define RSS_ATU_SCP_ATU_ID              3 /*ID to use for region mapping to host SCP ATU */
/* ATU region ID for accessing RSS-SCP shared mailbox */
#define RSS_ATU_SCP_RSS_MAILBOX_ATU_ID  4
#define RSS_ATU_AP_RSS_MAILBOX_ATU_ID   5 /*ID to use for region mapping to MHU
                                            outband msg buffer between AP and RSS */
#define RSS_ATU_SI_FLASH_ID             6  /* ID to use for region mapping to SI NVM flash */

/* The ATU has a minimum size and all regions are restricted to align with it */
#define RSS_ATU_PAGE_SIZE           0x2000U /* 8KB */

/*
 * In flash the image is layed out like so:
 *
 * ┌────────┬──────┬─────────┐
 * │ HEADER │ CODE │ TRAILER │
 * ├────────┴──────┴─────────┤
 * │ Flash Image             │
 * └─────────────────────────┘
 *
 * However, when the image is loaded the CODE region needs to be at the start
 * of the memory for the host firmware to load. To resolve this with one
 * contiguous copy the HEADER region needs to be at the start of the image in
 * RSS logical memory but in a different location in physical memory. The ATU
 * is setup with two regions: ATU HEADER and ATU CODE which are contiguous in
 * logical memory but separate in physical memory. The image is then setup so
 * that the boundary between HEADER and CODE regions is the boundary between
 * the two ATU regions in logical memory.
 *
 *                 ┌──────┬─────────┬────────┐
 * Physical Memory │ CODE │ TRAILER │ HEADER │
 *                 ├──────┴─────────┴────────┤
 *                 │ ATU CODE                │
 *                 └──────────▲─┬────────────┤
 *                            │ │ ATU HEADER │
 *                            │ └──────▲─────┘
 *                        ┌───┼────────┘
 *                        │   └────────────┐
 *                 ┌──────┴─────┬──────────┴──────────────┐
 * Logical Memory  │ ATU HEADER │ ATU CODE                │
 *                 └───┬────────┼──────┬─────────┬────────┘
 *                     │ HEADER │ CODE │ TRAILER │
 *                     └────────┴──────┴─────────┘
 *
 * Note: Regions not to scale
 * Note: Some overlap is expected between ATU CODE and ATU HEADER in physical
 *       memory as the HEADER is smaller than the ATU page size. This is fine
 *       as the whole image is shifted and would have fit inside ATU CODE.
 * Note: The physical memory for MCP is used for all images which allows the
 *       code on other firmware (particularly LCP) to be large as the memory
 *       no longer needs to contain the header.
 */


#define RSS_IMG_HDR_ATU_SIZE    ALIGN_UP(BL2_HEADER_SIZE, RSS_ATU_PAGE_SIZE)

/*
 * RSS ATU Regions for image loading
 * Note: these need to consistent with values used when signing the images.
 * Note: MCU Boot requires that the logical addresses do not overlap.
 */

/* MCP ATU HEADER logical address start */
#define HOST_MCP_HDR_ATU_BASE_S     HOST_ACCESS_BASE_S
/* MCP Code region and MCP ATU CODE logical address start */
#define HOST_MCP_CODE_BASE_S        (HOST_MCP_HDR_ATU_BASE_S + RSS_IMG_HDR_ATU_SIZE)
/* MCP Image address start, offset so end of HEADER is at end of ATU HEADER */
#define HOST_MCP_IMG_BASE_S         (HOST_MCP_CODE_BASE_S - BL2_HEADER_SIZE)
/* MCP ITCM physical address start */
#define HOST_MCP_PHYS_BASE          0x2000000000000ULL
/* MCP ATU CODE size (aligned size of MCP image) */
#define HOST_MCP_ATU_SIZE           ALIGN_UP(SIZE_DEF_MCP_IMAGE, RSS_ATU_PAGE_SIZE)
/* ATU ID for MCP ATU HEADER region */
#define HOST_MCP_IMG_HDR_ATU_ID     RSS_ATU_IMG_HDR_LOAD_ID
/* ATU ID for MCP ATU CODE region */
#define HOST_MCP_IMG_CODE_ATU_ID    RSS_ATU_IMG_CODE_LOAD_ID

/* SCP ATU HEADER logical address start */
#define HOST_SCP_HDR_ATU_BASE_S     (HOST_MCP_CODE_BASE_S + HOST_MCP_ATU_SIZE)
/* SCP Code region and SCP ATU CODE logical address start */
#define HOST_SCP_CODE_BASE_S        (HOST_SCP_HDR_ATU_BASE_S + RSS_IMG_HDR_ATU_SIZE)
/* SCP Image address start, offset so end of HEADER is at end of ATU HEADER */
#define HOST_SCP_IMG_BASE_S         (HOST_SCP_CODE_BASE_S - BL2_HEADER_SIZE)
/* SCP ITCM physical address start */
#define HOST_SCP_PHYS_BASE          0x1000000000000ULL
/* SCP ATU CODE size (aligned size of SCP image) */
#define HOST_SCP_ATU_SIZE           ALIGN_UP(SIZE_DEF_SCP_IMAGE, RSS_ATU_PAGE_SIZE)
/* ATU ID for SCP ATU HEADER region */
#define HOST_SCP_IMG_HDR_ATU_ID     RSS_ATU_IMG_HDR_LOAD_ID
/* ATU ID for SCP ATU CODE region */
#define HOST_SCP_IMG_CODE_ATU_ID    RSS_ATU_IMG_CODE_LOAD_ID

/*
 * All LCP use the same ATU region but map it to a different physical address
 * The physical address is the cluster utility space in SCP which has an
 * address translation window for each LCP.
 * Note: The cluster utility space requires SCP's ATU to be setup correctly.
 */

/* LCP ATU HEADER logical address start */
#define HOST_LCP_HDR_ATU_BASE_S     (HOST_SCP_CODE_BASE_S + HOST_SCP_ATU_SIZE)
/* LCP Code region and LCP ATU CODE logical address start */
#define HOST_LCP_CODE_BASE_S        (HOST_LCP_HDR_ATU_BASE_S + RSS_IMG_HDR_ATU_SIZE)
/* LCP Image address start, offset so end of HEADER is at end of ATU HEADER */
#define HOST_LCP_IMG_BASE_S         (HOST_LCP_CODE_BASE_S - BL2_HEADER_SIZE)
/* LCP0 ITCM window in SCP address start */
#define HOST_LCP_0_PHYS_BASE        0x0000200050000ULL
/* Offset between LCP ITCM windows in SCP */
#define HOST_LCP_N_PHYS_OFFSET      0x200000U
/* LCP ITCM window in SCP address start for LCP N*/
#define HOST_LCP_N_PHYS_BASE(n)     (HOST_LCP_0_PHYS_BASE + HOST_LCP_N_PHYS_OFFSET * n)
/* LCP ATU CODE size (aligned size of LCP image) */
#define HOST_LCP_ATU_SIZE           ALIGN_UP(SIZE_DEF_LCP_IMAGE, RSS_ATU_PAGE_SIZE)
/* ATU ID for LCP ATU HEADER region */
#define HOST_LCP_IMG_HDR_ATU_ID     RSS_ATU_IMG_HDR_LOAD_ID
/* ATU ID for LCP ATU CODE region */
#define HOST_LCP_IMG_CODE_ATU_ID    RSS_ATU_IMG_CODE_LOAD_ID

/* AP BL1 ATU HEADER logical address start */
#define HOST_AP_BL1_HDR_ATU_BASE_S     (HOST_LCP_CODE_BASE_S + HOST_LCP_ATU_SIZE)
/*  AP BL1 Code region and SCP ATU CODE logical address start */
#define HOST_AP_BL1_CODE_BASE_S        (HOST_AP_BL1_HDR_ATU_BASE_S + RSS_IMG_HDR_ATU_SIZE)
/*  AP BL1 Image address start, offset so end of HEADER is at end of ATU HEADER */
#define HOST_AP_BL1_IMG_BASE_S         (HOST_AP_BL1_CODE_BASE_S - BL2_HEADER_SIZE)
/*  AP BL1 ITCM physical address start */
#define HOST_AP_BL1_PHYS_BASE          (0x0000000000000ULL +  0x42000UL) /* AP initial boot SRAM base address
/*  AP BL1 ATU CODE size (aligned size of SCP image) */
#define HOST_AP_BL1_ATU_SIZE           ALIGN_UP(SIZE_DEF_AP_BL1_IMAGE, RSS_ATU_PAGE_SIZE)
/*  AP BL1 ID for SCP ATU HEADER region */
#define HOST_AP_BL1_IMG_HDR_ATU_ID     RSS_ATU_IMG_HDR_LOAD_ID
/*  AP BL1 ID for SCP ATU CODE region */
#define HOST_AP_BL1_IMG_CODE_ATU_ID    RSS_ATU_IMG_CODE_LOAD_ID

/* ATU HEADER physical address start (mapped so end of region is end of MCP ITCM) */
#define RSS_HDR_PHYS_BASE              (HOST_SCP_PHYS_BASE + HOST_SCP_ATU_SIZE - RSS_IMG_HDR_ATU_SIZE)

/* SI CL0 ATU HEADER logical address start */
#define HOST_SI_CL0_HDR_ATU_BASE_S     (HOST_AP_BL1_CODE_BASE_S + HOST_AP_BL1_ATU_SIZE)
/*  SI CL0 Code region and AP ATU CODE logical address start */
#define HOST_SI_CL0_CODE_BASE_S        (HOST_SI_CL0_HDR_ATU_BASE_S + RSS_IMG_HDR_ATU_SIZE)
/*  SI CL0 Image address start, offset so end of HEADER is at end of ATU HEADER */
#define HOST_SI_CL0_IMG_BASE_S         (HOST_SI_CL0_CODE_BASE_S - BL2_HEADER_SIZE)
/*  SI physical address start */
#define HOST_SI_PHYS_BASE              0x2000000000000ULL
/*  SI CL0 ATU CODE size (aligned size of SI image) */
#define HOST_SI_CL0_ATU_SIZE           ALIGN_UP(SIZE_DEF_SI_CL0_IMAGE, RSS_ATU_PAGE_SIZE)
/*  SI CL0 ID for SI CL0 ATU HEADER region */
#define HOST_SI_CL0_IMG_HDR_ATU_ID     RSS_ATU_IMG_HDR_LOAD_ID
/*  SI CL0 ID for SI CL0 ATU CODE region */
#define HOST_SI_CL0_IMG_CODE_ATU_ID    RSS_ATU_IMG_CODE_LOAD_ID

/* SI CL1 ATU HEADER logical address start */
#define HOST_SI_CL1_HDR_ATU_BASE_S     (HOST_SI_CL0_CODE_BASE_S + HOST_SI_CL0_ATU_SIZE)
/*  SI CL1 Code region and AP ATU CODE logical address start */
#define HOST_SI_CL1_CODE_BASE_S        (HOST_SI_CL1_HDR_ATU_BASE_S + RSS_IMG_HDR_ATU_SIZE)
/*  SI CL1 Image address start, offset so end of HEADER is at end of ATU HEADER */
#define HOST_SI_CL1_IMG_BASE_S         (HOST_SI_CL1_CODE_BASE_S - BL2_HEADER_SIZE)
/*  SI CL1 ATU CODE size (aligned size of SI image) */
#define HOST_SI_CL1_ATU_SIZE           ALIGN_UP(SIZE_DEF_SI_CL1_IMAGE, RSS_ATU_PAGE_SIZE)
/*  SI CL1 ID for SI CL1 ATU HEADER region */
#define HOST_SI_CL1_IMG_HDR_ATU_ID     RSS_ATU_IMG_HDR_LOAD_ID
/*  SI CL1 ID for SI CL1 ATU CODE region */
#define HOST_SI_CL1_IMG_CODE_ATU_ID    RSS_ATU_IMG_CODE_LOAD_ID

/* SI CL2 ATU HEADER logical address start */
#define HOST_SI_CL2_HDR_ATU_BASE_S     (HOST_SI_CL1_CODE_BASE_S + HOST_SI_CL1_ATU_SIZE)
/*  SI CL2 Code region and AP ATU CODE logical address start */
#define HOST_SI_CL2_CODE_BASE_S        (HOST_SI_CL2_HDR_ATU_BASE_S + RSS_IMG_HDR_ATU_SIZE)
/*  SI CL2 Image address start, offset so end of HEADER is at end of ATU HEADER */
#define HOST_SI_CL2_IMG_BASE_S         (HOST_SI_CL2_CODE_BASE_S - BL2_HEADER_SIZE)
/*  SI CL2 ATU CODE size (aligned size of SI image) */
#define HOST_SI_CL2_ATU_SIZE           ALIGN_UP(SIZE_DEF_SI_CL2_IMAGE, RSS_ATU_PAGE_SIZE)
/*  SI CL2 ID for SI CL2 ATU HEADER region */
#define HOST_SI_CL2_IMG_HDR_ATU_ID     RSS_ATU_IMG_HDR_LOAD_ID
/*  SI CL2 ID for SI CL2 CODE region */
#define HOST_SI_CL2_IMG_CODE_ATU_ID    RSS_ATU_IMG_CODE_LOAD_ID

/* Last rss logical address used for loading images */
#define RSS_IMAGE_LOADING_END   (HOST_SI_CL2_CODE_BASE_S + HOST_SI_CL2_ATU_SIZE)

/*
 * RSS ATU Regions for accessing the initiation registers.
 * These will not be used at the same time so logical addresses can overlap.
 */

/* MCP initialisation region logical address start */
#define HOST_MCP_INIT_BASE_S    RSS_IMAGE_LOADING_END
/* MCP initialisation region physical address start */
#define HOST_MCP_INIT_PHYS_BASE 0x2000050020000ULL
/* MCP initialisation region ATU size */
#define HOST_MCP_INIT_SIZE      ALIGN_UP(0x1000U, RSS_ATU_PAGE_SIZE)
/* MCP initialisation region ATU id */
#define HOST_MCP_INIT_ATU_ID    RSS_ATU_FW_INIT_ID

/* SCP initialisation region logical address start */
#define HOST_SCP_INIT_BASE_S    RSS_IMAGE_LOADING_END
/* SCP initialisation region physical address start */
#define HOST_SCP_INIT_PHYS_BASE 0x1000050050000ULL
/* SCP initialisation region ATU size */
#define HOST_SCP_INIT_SIZE      ALIGN_UP(0x1000U, RSS_ATU_PAGE_SIZE)
/* SCP initialisation region ATU id */
#define HOST_SCP_INIT_ATU_ID    RSS_ATU_FW_INIT_ID

/* Last rss logical address used for initialising firmware */
#define RSS_HOST_INIT_END       (HOST_SCP_INIT_BASE_S + HOST_SCP_INIT_SIZE)

/*
 * RSS ATU Regions for communicating with and providing services for SCP
 * These regions are left open for use in SecureFW so cannot overlap other
 * regions.
 */

/* ATU region mapping to access SCP's ATU from RSS */
#define HOST_SCP_ATU_BASE_S         RSS_HOST_INIT_END
#define HOST_SCP_ATU_PHYS_BASE      0x1000050010000ULL
#define HOST_SCP_ATU_ATU_SIZE       ALIGN_UP(0x10000, RSS_ATU_PAGE_SIZE)
#define HOST_SCP_ATU_ATU_ID         RSS_ATU_SCP_ATU_ID

/* ATU region to access Shared mailbox between SCP and RSS */
#define HOST_SCP_RSS_MAILBOX_BASE_S \
    (HOST_SCP_ATU_BASE_S + HOST_SCP_ATU_ATU_SIZE)
/* Physical address of mailbox located in the RSM Shared RAM region */
#define HOST_SCP_RSS_MAILBOX_PHYS_BASE 0x2F000000
#define HOST_SCP_RSS_MAILBOX_ATU_SIZE ALIGN_UP(0x2000, RSS_ATU_PAGE_SIZE)
#define HOST_SCP_RSS_MAILBOX_ATU_ID RSS_ATU_SCP_RSS_MAILBOX_ATU_ID

/*
 * ATU region for AP<->RSS MHU outband message buffer
 * HOST_AP_RSS_MAILBOX_BASE_S: Logical address at which the RSS should access
 *                             the AP<->RSS outband msg buffer.
 *
 * HOST_AP_RSS_MAILBOX_PHYS_BASE: Physical address of outband msg buffer
 *                                located in the AP Shared RAM region.
 *
 * HOST_AP_RSS_MAILBOX_ATU_SIZE: Size of outband msg buffer as defined in TF-A.
 */
#define HOST_AP_RSS_MAILBOX_BASE_S     HOST_ACCESS_BASE_NS
#define HOST_AP_RSS_MAILBOX_PHYS_BASE  UINT64_C(0x86000)
#define HOST_AP_RSS_MAILBOX_ATU_SIZE   ALIGN_UP(0x1000, RSS_ATU_PAGE_SIZE)
#define HOST_AP_RSS_MAILBOX_ATU_ID     RSS_ATU_AP_RSS_MAILBOX_ATU_ID


/* Safety Island NVM flash */
#define SI_FLASH_BASE           0x60000000   /* Safety Island NVM flash base address */
#define SI_FLASH_SIZE           0x8000000    /* 128 MB */

/* Safety Island NVM flash logical base address using Non-secure ATU region */
#define SI_FLASH_BASE_NS_LOG    (HOST_ACCESS_BASE_NS + 0x8000000)
/* Safety Island NVM flash physical base address */
#define SI_FLASH_BASE_NS_PHY    (HOST_SI_PHYS_BASE + SI_FLASH_BASE)

/* Address to access safety island images */
#define SI_CL0_SRAM_BASE        0x120000000ULL
#define SI_CL1_SRAM_BASE        0x140000000ULL
#define SI_CL2_SRAM_BASE        0x160000000ULL

#define HOST_SI_CL0_SRAM_PHYS_BASE      (HOST_SI_PHYS_BASE + SI_CL0_SRAM_BASE)
#define HOST_SI_CL1_SRAM_PHYS_BASE      (HOST_SI_PHYS_BASE + SI_CL1_SRAM_BASE)
#define HOST_SI_CL2_SRAM_PHYS_BASE      (HOST_SI_PHYS_BASE + SI_CL2_SRAM_BASE)

#endif  /* __HOST_BASE_ADDRESS_H__ */
