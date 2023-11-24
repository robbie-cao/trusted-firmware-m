/*
 * Copyright (c) 2022-2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __FIP_PARSER_H__
#define __FIP_PARSER_H__

#include "firmware_image_package.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "uuid.h"

/* Return code of fip parser APIs */
#define FIP_PARSER_SUCCESS                     (0)
#define FIP_PARSER_ERROR                       (INT32_MIN + 1)

/* This is used as a signature to validate the blob header */
#define TOC_HEADER_NAME 0xAA640001

/* ToC Entry UUIDs */
#define UUID_TRUSTED_BOOT_FIRMWARE_BL2 \
    {{0x5f, 0xf9, 0xec, 0x0b}, {0x4d, 0x22}, \
    {0x3e, 0x4d}, 0xa5, 0x44, \
    {0xc3, 0x9d, 0x81, 0xc7, 0x3f, 0x0a} }

typedef struct _FIP_TOC_HEADER {
    uint32_t    name;
    uint32_t    serial_number;
    uint64_t    flags;
} FIP_TOC_HEADER;

/* FIP tool creates fip table using 64 bit integers because of its
 * basic support for 64 bit Cortex A.Hence need to add 32 bit padding
 * for 32-bit Cortex-M CPUs
 */

typedef struct _FIP_TOC_ENTRY {
    uuid_t      uuid;
    uint32_t    address;
    uint32_t    pad1;
    uint32_t    size;
    uint32_t    pad2;
    uint64_t    flags;
} FIP_TOC_ENTRY;

int parse_fip_and_extract_tfa_info(uint32_t address, uint32_t fip_size,
        uint32_t *tfa_offset, uint32_t *tfa_size);
/**
 * \brief                    Parse a FIP and retrieve the offset and size of one
 *                           of the firmware images (specified by UUID).
 *
 * \param[in]  fip_base      The RSS address mapped to the FIP base address in
 *                           host flash.
 * \param[in]  atu_slot_size The size of the ATU region that was mapped for
 *                           access to this FIP. This is used to prevent reads
 *                           outside the mapped region.
 * \param[in]  uuid          The UUID of the firmware image to get the offset
 *                           and size of.
 *
 * \param[out] offset        The offset in host flash of the firmware image.
 * \param[out] size          The size of the firmware image.
 *
 * \return                   0 if operation completed successfully, another
 *                           value on error.
 */
int fip_get_entry_by_uuid(uint32_t fip_base, uint32_t atu_slot_size, uuid_t uuid,
                          uint64_t *offset, size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* __FIP_PARSER_H__ */
