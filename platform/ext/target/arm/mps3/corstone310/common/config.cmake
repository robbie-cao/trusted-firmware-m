#-------------------------------------------------------------------------------
# Copyright (c) 2021-2023, Arm Limited. All rights reserved.
# Copyright (c) 2022 Cypress Semiconductor Corporation (an Infineon company)
# or an affiliate of Cypress Semiconductor Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

set(DEFAULT_NS_SCATTER                ON    CACHE BOOL    "Use default NS scatter files for target")
set(PSA_API_TEST_TARGET          "cs3x0"    CACHE STRING  "PSA_API_TARGET name")
# Make FLIH IRQ test as the default IRQ test on Corstone-310
set(TEST_NS_SLIH_IRQ                  OFF   CACHE BOOL    "Whether to build NS regression Second-Level Interrupt Handling tests")

set(FLASH_S_PARTITION_SIZE   "0x80000"    CACHE STRING    "Secure code size")
set(FLASH_NS_PARTITION_SIZE  "0x300000"   CACHE STRING    "Non-secure code size")

if(BL2)
    set(BL2_TRAILER_SIZE 0x800 CACHE STRING "Trailer size")
else()
    #No header if no bootloader, but keep IMAGE_CODE_SIZE the same
    set(BL2_TRAILER_SIZE 0xC00 CACHE STRING "Trailer size")
endif()

# Platform-specific configurations
set(CONFIG_TFM_USE_TRUSTZONE          ON)
set(TFM_MULTI_CORE_TOPOLOGY           OFF)

set(PLATFORM_SLIH_IRQ_TEST_SUPPORT    ON)
set(PLATFORM_FLIH_IRQ_TEST_SUPPORT    ON)

# Ethos-U NPU configurations
set(ETHOSU_ARCH                       "U55")
set(ETHOS_DRIVER_PATH                 "DOWNLOAD"  CACHE PATH      "Path to Ethos-U Core Driver (or DOWNLOAD to fetch automatically")
set(ETHOSU_LOG_SEVERITY               "-1"        CACHE STRING    "Ethos-U Core Driver log severity")
