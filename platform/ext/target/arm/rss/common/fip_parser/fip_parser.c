/*
 * Copyright (c) 2022-2023, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "fip_parser.h"

#include "flash_layout.h"
#include "Driver_Flash.h"

#include <string.h>
#ifdef TFM_FWU_AGENT
#include "bootutil/bootutil_log.h"
#endif /* TFM_FWU_AGENT */

extern ARM_DRIVER_FLASH FLASH_DEV_NAME;

int fip_get_entry_by_uuid(uint32_t fip_base, uint32_t atu_slot_size, uuid_t uuid,
                          uint64_t *offset, size_t *size)
{
    ARM_FLASH_CAPABILITIES DriverCapabilities = FLASH_DEV_NAME.GetCapabilities();
    /* Valid entries for data item width */
    uint32_t data_width_byte[] = {
        sizeof(uint8_t),
        sizeof(uint16_t),
        sizeof(uint32_t),
    };
    size_t data_width = data_width_byte[DriverCapabilities.data_width];
    int rc;
    uint32_t idx = 0;
    fip_toc_header_t toc_header;
    fip_toc_entry_t toc_entry;
    uuid_t null_uuid;

    /* The NULL UUID is all-zeroes */
    memset(&null_uuid, 0, sizeof(null_uuid));

    rc = FLASH_DEV_NAME.ReadData(fip_base - FLASH_BASE_ADDRESS, &toc_header,
                                 sizeof(toc_header) / data_width);
    if (rc != sizeof(toc_header) / data_width) {
        return rc;
    }

    if (toc_header.name != TOC_HEADER_NAME) {
        return 2;
    }

    idx += sizeof(toc_header);

    do {
        /* Prevent reading out of bounds */
        if (idx + sizeof(toc_entry) > atu_slot_size) {
            return 3;
        }

        rc = FLASH_DEV_NAME.ReadData(fip_base + idx - FLASH_BASE_ADDRESS,
                                     &toc_entry, sizeof(toc_entry) / data_width);
        if (rc != sizeof(toc_entry) / data_width) {
            return rc;
        }

        if (!memcmp(&uuid, &toc_entry.uuid, sizeof(uuid))) {
            *offset = toc_entry.offset_address;

            /* We can't deal with partitions that are greater than UINT32_MAX,
             * since they aren't wholly mappable into the RSS memory space. This
             * is in reality bounded by the ATU mappable size, but that'll be
             * checked once the ATU region is set up, this just allows us to
             * perform safe type-conversion.
             */
            if (toc_entry.size > UINT32_MAX) {
                return 1;
            }

            *size = (uint32_t)toc_entry.size;

            return 0;
        }

        idx += sizeof(toc_entry);
        /* The FIP's TOC ends in an entry with a NULL UUID. This entry's size is
         * the size of the whole FIP. */
    } while (memcmp(&null_uuid, &toc_entry.uuid, sizeof(uuid_t)));

    /* UUID not found, return error. */
    return 3;
}

#ifdef TFM_FWU_AGENT
int parse_fip_and_extract_tfa_info(uint32_t address, uint32_t fip_size,
        uint32_t *tfa_offset, uint32_t *tfa_size)
{
    FIP_TOC_HEADER *toc_header = NULL;
    FIP_TOC_ENTRY *toc_entry = NULL;
    uuid_t uuid_null = {0};
    uuid_t tfa_uuid = UUID_TRUSTED_BOOT_FIRMWARE_BL2;
    uint8_t *iter;

    toc_header = (FIP_TOC_HEADER *) address;

    volatile uint32_t *tmp = address;

    if (toc_header->name != TOC_HEADER_NAME) {
        return FIP_PARSER_ERROR;
    }

    toc_entry = (FIP_TOC_ENTRY *)(toc_header + 1);

    for (iter = (char *)toc_entry;
        iter <= (char *)toc_header + fip_size;
        iter = iter + sizeof(FIP_TOC_ENTRY), toc_entry++) {

        if (!memcmp(iter, &uuid_null, sizeof(uuid_t))) {
            return FIP_PARSER_ERROR;
        }

        if (!memcmp(iter, &tfa_uuid, sizeof(uuid_t))) {
            BOOT_LOG_INF("TF-A FIP at : address = 0x%x : size = 0x%x \n\r",
                                toc_entry->address,
                                toc_entry->size);
            *tfa_offset = toc_entry->address;
            *tfa_size = toc_entry->size;
            return FIP_PARSER_SUCCESS;
        }
    }

    return FIP_PARSER_ERROR;
}
#endif /* TFM_FWU_AGENT */
