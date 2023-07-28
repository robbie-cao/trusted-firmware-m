/*
 * Copyright (c) 2023 Arm Limited. All rights reserved.
 *
 * Licensed under the Apache License Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __HOST_DEVICE_CFG_H__
#define __HOST_DEVICE_CFG_H__

/* ARM SCP ATU */
#define ATU_SCP

/* MHU */
#define MHU_V3_RSS_TO_AP
#define MHU_V3_AP_TO_RSS

#define MHU_V3_RSS_TO_SCP
#define MHU_V3_SCP_TO_RSS

/*External Intel Strata Flash Device*/
#define SPI_STRATAFLASHJ3_S

/* CFI Controller */
#define CFI_S

#endif /* __HOST_DEVICE_CFG_H__ */
