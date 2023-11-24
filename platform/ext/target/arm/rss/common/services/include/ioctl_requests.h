/*
 * Copyright (c) 2021,2023 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef IOCTL_REQUESTS_H
#define IOCTL_REQUESTS_H

#include <stdint.h>

enum ioctl_id_t {
   IOCTL_FWU_FLASH_IMAGES = 0,
   IOCTL_FWU_HOST_ACK,
   IOCTL_FMP_GET_IMAGE_INFO,
};

#endif /* IOCTL_REQUESTS_H */
