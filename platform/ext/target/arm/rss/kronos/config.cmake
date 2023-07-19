#-------------------------------------------------------------------------------
# Copyright (c) 2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

set(PLAT_MHU_VERSION                        3        CACHE STRING  "Supported MHU version by platform")
set(TFM_BL1_MEMORY_MAPPED_FLASH             OFF      CACHE BOOL    "Whether BL1 can directly access flash content")
set(PLATFORM_HAS_BOOT_DMA                   OFF      CACHE BOOL    "Enable dma support for memory transactions for bootloader")
set(RSS_USE_HOST_UART                       OFF      CACHE BOOL    "Whether RSS should use the UART from the host system (opposed to dedicated UART)")
set(RSS_USE_HOST_FLASH                      OFF      CACHE BOOL    "Enable RSS using the host flash.")
set(MCUBOOT_IMAGE_NUMBER                    6        CACHE STRING  "Number of images supported by MCUBoot")
set(ENABLE_SCP_ATU_CTRL                     OFF      CACHE BOOL    "Enable SCP to configure ATU")
set(RSS_HAS_EXPANSION_PERIPHERALS           ON       CACHE BOOL    "Whether RSS has sub-platform specific peripherals in the expansion layer")
set(ENABLE_MCP_ATU_CTRL                     OFF      CACHE BOOL    "Enable MCP to configure ATU")
set(PLATFORM_DEFAULT_MCUBOOT_IMAGE_NUMBER   OFF      CACHE BOOL    "Whether to use default value of MCUBOOT_IMAGE_NUMBER")
if (TFM_PLATFORM_VARIANT STREQUAL "1")
    set(RSS_LCP_COUNT                          8        CACHE STRING   "Number of LCPs to load")
elseif(TFM_PLATFORM_VARIANT STREQUAL "2")
    set(RSS_LCP_COUNT                          4        CACHE STRING   "Number of LCPs to load")
else()
    set(RSS_LCP_COUNT                          16       CACHE STRING   "Number of LCPs to load")
endif()

# Maximum number of MCUBoot images supported by TF-M NV counters and ROTPKs
if(MCUBOOT_IMAGE_NUMBER GREATER 9)
    message(FATAL_ERROR "INVALID CONFIG: MCUBOOT_IMAGE_NUMBER value not supported!")
endif()

# Once all options are set, set common options as fallback
include(${CMAKE_CURRENT_LIST_DIR}/../common/config.cmake)
