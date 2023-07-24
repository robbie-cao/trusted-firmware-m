/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __RSS_FIRMWARE_IMAGE_ID_H__
#define __RSS_FIRMWARE_IMAGE_ID_H__

/*
 * ID of firmware images loaded by BL2
 * Images are loaded by BL2 in reverse order of ID
 * Cannot use enum as this is included in compiler files.
 */
/* RSS firmware */
#define RSS_FIRMWARE_SECURE_ID       0
#define RSS_FIRMWARE_NON_SECURE_ID   1
/* Host Firmware */
#define RSS_FIRMWARE_AP_BL1_ID       2
#define RSS_FIRMWARE_SI_CL2_ID       3
#define RSS_FIRMWARE_SI_CL1_ID       4
#define RSS_FIRMWARE_SI_CL0_ID       5
#define RSS_FIRMWARE_LCP_ID          6
#define RSS_FIRMWARE_SCP_ID          7
/* Number of firmware loaded by BL2 */
#define RSS_FIRMWARE_COUNT           8

#endif /* __RSS_FIRMWARE_IMAGE_ID_H__ */
