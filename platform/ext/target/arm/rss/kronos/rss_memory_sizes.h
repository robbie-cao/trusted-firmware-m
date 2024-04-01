/*
 * Copyright (c) 2023, Arm Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __RSS_MEMORY_SIZES_H__
#define __RSS_MEMORY_SIZES_H__

#define VM0_SIZE                         0x00080000 /* 512 KiB */
#define VM1_SIZE                         0x00080000 /* 512 KiB */

#define BOOT_FLASH_SIZE                  0x04000000 /* 64 MB */

/* The total size of the OTP for the RSS */
#define OTP_TOTAL_SIZE     (0x4000)
/* How much OTP is reserved for the portion of the DMA Initial Command Sequence
 * which is located in OTP. This is loaded by directly by the DMA hardware, so
 * this must match the size configured into the ROM part of the ICS.
 */
#define OTP_DMA_ICS_SIZE   (0x400)
/* How much space in ROM is used for the DMA Initial Command Sequence */
#define ROM_DMA_ICS_SIZE   (0x1000)

#endif /* __RSS_MEMORY_SIZES_H__ */
