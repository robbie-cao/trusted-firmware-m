#-------------------------------------------------------------------------------
# Copyright (c) 2021-2023, Arm Limited. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

# preload.cmake is used to set things that related to the platform that are both
# immutable and global, which is to say they should apply to any kind of project
# that uses this platform. In practise this is normally compiler definitions and
# variables related to hardware.

add_definitions(
    -DCORSTONE300_AN552
)

set(CORSTONE300_COMMON_DIR "${CMAKE_CURRENT_LIST_DIR}/../common")

include(${CORSTONE300_COMMON_DIR}/preload.cmake)

set(CONFIG_TFM_FP_ARCH "fpv5-d16")
set(CONFIG_TFM_FP_ARCH_ASM "FPv5_D16")
