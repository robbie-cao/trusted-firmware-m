#-------------------------------------------------------------------------------
# Copyright (c) 2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

# Load the default extra configs about generated files and secure partitions
include(${CMAKE_SOURCE_DIR}/config/extra_build_config.cmake)

# Load defaults, setting options not already set
include(config/config_base.cmake)

# Load regression configs overrided by platform
include(${TARGET_PLATFORM_PATH}/reg_config_override.cmake OPTIONAL)

# Build system log config
include(${CMAKE_SOURCE_DIR}/config/tfm_build_log_config.cmake)

# The library to collect compile definitions of config options.
add_library(tfm_config INTERFACE)

target_compile_definitions(tfm_config
    INTERFACE
        $<$<STREQUAL:${TEST_PSA_API},CRYPTO>:PSA_API_TEST_CRYPTO>
        $<$<STREQUAL:${TEST_PSA_API},IPC>:PSA_API_TEST_IPC>
)

# Set user defined TF-M config header file
if(PROJECT_CONFIG_HEADER_FILE)
    if(NOT EXISTS ${PROJECT_CONFIG_HEADER_FILE})
        message(FATAL_ERROR "${PROJECT_CONFIG_HEADER_FILE} does not exist! Please use absolute path.")
    endif()
    target_compile_definitions(tfm_config
        INTERFACE
            PROJECT_CONFIG_HEADER_FILE="${PROJECT_CONFIG_HEADER_FILE}"
    )
endif()

# Set platform defined TF-M config header file
set(TARGET_CONFIG_HEADER_FILE ${TARGET_PLATFORM_PATH}/config_tfm_target.h)
if(EXISTS ${TARGET_CONFIG_HEADER_FILE})
    target_compile_definitions(tfm_config
        INTERFACE
            TARGET_CONFIG_HEADER_FILE="${TARGET_CONFIG_HEADER_FILE}"
    )
endif()

target_include_directories(tfm_config
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
)
