#-------------------------------------------------------------------------------
# Copyright (c) 2020-2023, Arm Limited. All rights reserved.
# Copyright (c) 2022 Cypress Semiconductor Corporation (an Infineon company)
# or an affiliate of Cypress Semiconductor Corporation. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

include(${CORSTONE300_COMMON_DIR}/config.cmake)

set(PLATFORM_DEFAULT_SYSTEM_RESET_HALT OFF   CACHE BOOL    "Use default system reset/halt implementation")
