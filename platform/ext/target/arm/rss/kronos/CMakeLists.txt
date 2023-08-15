#-------------------------------------------------------------------------------
# Copyright (c) 2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

cmake_policy(SET CMP0076 NEW)
set(CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../common common)

#========================= Platform Region Defs ===============================#

target_include_directories(platform_region_defs
    INTERFACE
        .
)

#========================= tfm_spm ============================================#
target_sources(tfm_spm
    PRIVATE
        device/host_device_definition.c
        ${CMAKE_CURRENT_LIST_DIR}/../common/mscp_atu_request_handler.c
        ${CMAKE_CURRENT_LIST_DIR}/../common/mscp_mailbox.c
)

target_include_directories(tfm_spm
    PUBLIC
        device
)

#========================= Platform Secure ====================================#

target_include_directories(platform_s
    PUBLIC
        cmsis_drivers
        host_drivers
        device
        ${PLATFORM_DIR}/ext/target/arm/drivers/usart/pl011
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/common
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata
)

target_sources(platform_s
    PRIVATE
        host_drivers/cfi_drv.c
        cmsis_drivers/Driver_Flash.c
        cmsis_drivers/Driver_USART.c
        device/host_device_definition.c
        device/rss_expansion_device_definition.c
        ${CMAKE_CURRENT_LIST_DIR}/../common/mscp_atu_request_handler.c
        ${CMAKE_CURRENT_LIST_DIR}/../common/mscp_mailbox.c
        ${PLATFORM_DIR}/ext/target/arm/drivers/usart/pl011/uart_pl011_drv.c
        rss_expansion_peripherals_def.c
        rss_comms/rss_comms_permissions_hal.c
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata/spi_strataflashj3_flash_lib.c
)

target_sources(tfm_s
    PRIVATE
        device/rss_expansion_device_definition.c
        ${CMAKE_CURRENT_LIST_DIR}/../common/mscp_atu_request_handler.c
        ${CMAKE_CURRENT_LIST_DIR}/../common/mscp_mailbox.c
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata/spi_strataflashj3_flash_lib.c
)

target_include_directories(tfm_s
    PUBLIC
        ./device
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/common
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata
)

#========================= Platform Non-Secure ================================#

target_include_directories(platform_ns
    PUBLIC
        cmsis_drivers
        device
        host_drivers
        ${PLATFORM_DIR}/ext/target/arm/drivers/usart/pl011
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/common
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata
)

target_sources(platform_ns
    PRIVATE
        host_drivers/cfi_drv.c
        cmsis_drivers/Driver_Flash.c
        cmsis_drivers/Driver_USART.c
        device/host_device_definition.c
        device/rss_expansion_device_definition.c
        ${PLATFORM_DIR}/ext/target/arm/drivers/usart/pl011/uart_pl011_drv.c
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata/spi_strataflashj3_flash_lib.c
)

#========================= Platform BL2 =======================================#

target_sources(platform_bl2
    PRIVATE
        bl2/flash_map_bl2.c
        cmsis_drivers/Driver_Flash.c
        cmsis_drivers/Driver_USART.c
        device/host_device_definition.c
        device/rss_expansion_device_definition.c
        host_drivers/cfi_drv.c
        ${CMAKE_CURRENT_LIST_DIR}/../common/mscp_atu_request_handler.c
        ${CMAKE_CURRENT_LIST_DIR}/../common/mscp_mailbox.c
        ${PLATFORM_DIR}/ext/target/arm/drivers/usart/pl011/uart_pl011_drv.c
        tfm_interrupts.c
        fainlight_gic_lib.c
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata/spi_strataflashj3_flash_lib.c
)

target_compile_definitions(platform_bl2
    PRIVATE
        $<$<BOOL:${ENABLE_SCP_ATU_CTRL}>:ENABLE_SCP_ATU_CTRL>
        MEASURED_BOOT_HASH_ALG=${MEASURED_BOOT_HASH_ALG}
)

target_sources(bl2
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../common/mscp_atu_request_handler.c
        ${CMAKE_CURRENT_LIST_DIR}/../common/mscp_mailbox.c
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/common
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata
)

target_include_directories(platform_bl2
    PUBLIC
        cmsis_drivers
        device
        host_drivers
        ${PLATFORM_DIR}/ext/target/arm/drivers/usart/pl011
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/common
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata
)

target_compile_definitions(platform_bl2
    PRIVATE
        RSS_LCP_COUNT=${RSS_LCP_COUNT}
)

#========================= Platform BL1_1 =====================================#

target_sources(platform_bl1_1
    PRIVATE
        host_drivers/cfi_drv.c
        cmsis_drivers/Driver_Flash.c
        cmsis_drivers/Driver_USART.c
        device/host_device_definition.c
        device/rss_expansion_device_definition.c
        ${PLATFORM_DIR}/ext/target/arm/drivers/usart/pl011/uart_pl011_drv.c
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata/spi_strataflashj3_flash_lib.c
)

target_include_directories(platform_bl1_1_interface
    INTERFACE
        cmsis_drivers
        device
        host_drivers
        ${PLATFORM_DIR}/ext/target/arm/drivers/usart/pl011
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/common
        ${PLATFORM_DIR}/ext/target/arm/drivers/flash/strata
)

#=========================== MHUv3 Dependent Code ===============================#

if(PLAT_MHU_VERSION EQUAL 3)
    target_sources(platform_bl2
        PRIVATE
            tfm_interrupts.c
            bl2/boot_hal_bl2.c
    )

    target_sources(tfm_s
        PRIVATE
            tfm_interrupts.c
    )
endif()
