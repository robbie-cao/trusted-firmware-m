/*
 * Copyright (c) 2021-2023, Arm Limited. All rights reserved.
 * Copyright (c) 2022 Cypress Semiconductor Corporation (an Infineon
 * company) or an affiliate of Cypress Semiconductor Corporation. All rights
 * reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "cmsis.h"
#include "device_definition.h"
#include "spm.h"
#include "tfm_hal_interrupt.h"
#include "tfm_peripherals_def.h"
#include "interrupt.h"
#include "load/interrupt_defs.h"
#include "platform_irq.h"
#ifdef TFM_MULTI_CORE_TOPOLOGY
#include "rss_comms_hal.h"
#endif

extern struct mhu_v3_x_dev_t MHU_AP_TO_RSS_DEV;
extern struct mhu_v3_x_dev_t MHU_RSS_TO_AP_DEV;

#ifdef MHU_V3_RSS_SI_CL0
extern struct mhu_v3_x_dev_t MHU_SI_CL0_TO_RSS_DEV;
extern struct mhu_v3_x_dev_t MHU_RSS_TO_SI_CL0_DEV;
#endif /* MHU_V3_RSS_SI_CL0 */

#ifdef MHU_V3_RSS_SI_CL1
extern struct mhu_v3_x_dev_t MHU_SI_CL1_TO_RSS_DEV;
extern struct mhu_v3_x_dev_t MHU_RSS_TO_SI_CL1_DEV;
#endif /* MHU_V3_RSS_SI_CL1 */

#ifdef MHU_V3_RSS_SI_CL2
extern struct mhu_v3_x_dev_t MHU_SI_CL2_TO_RSS_DEV;
extern struct mhu_v3_x_dev_t MHU_RSS_TO_SI_CL2_DEV;
#endif /* MHU_V3_RSS_SI_CL2 */

static struct irq_t timer0_irq = {0};

void TFM_TIMER0_IRQ_Handler(void)
{
    spm_handle_interrupt(timer0_irq.p_pt, timer0_irq.p_ildi);
}

enum tfm_hal_status_t tfm_timer0_irq_init(void *p_pt,
                                          const struct irq_load_info_t *p_ildi)
{
    timer0_irq.p_ildi = p_ildi;
    timer0_irq.p_pt = p_pt;

    NVIC_SetPriority(TFM_TIMER0_IRQ, DEFAULT_IRQ_PRIORITY);
    NVIC_ClearTargetState(TFM_TIMER0_IRQ);
    NVIC_DisableIRQ(TFM_TIMER0_IRQ);

    return TFM_HAL_SUCCESS;
}

#ifdef TFM_MULTI_CORE_TOPOLOGY
static struct irq_t mbox_irq_info = {0};

/* Platform specific inter-processor communication interrupt handler. */
void CMU_MHU0_Receiver_Handler(void)
{
    (void)tfm_multi_core_hal_receive(&MHU_AP_TO_RSS_DEV, &MHU_RSS_TO_AP_DEV);

    /*
     * SPM will send a MAILBOX_SIGNAL to the corresponding partition
     * indicating that a message has arrived and can be processed.
     */
    spm_handle_interrupt(mbox_irq_info.p_pt, mbox_irq_info.p_ildi);
}

enum tfm_hal_status_t mailbox_irq_init(void *p_pt,
                                       const struct irq_load_info_t *p_ildi)
{
    mbox_irq_info.p_pt = p_pt;
    mbox_irq_info.p_ildi = p_ildi;

    /* Set MHU interrupt priority to the same as PendSV (the lowest)
     * TODO: Consider advantages/disadvantages of setting it one higher
     */
    NVIC_SetPriority(CMU_MHU0_Receiver_IRQn, NVIC_GetPriority(PendSV_IRQn));

    /* CMU_MHU0 is a secure peripheral, so its IRQs have to target S state */
    NVIC_ClearTargetState(CMU_MHU0_Receiver_IRQn);
    NVIC_DisableIRQ(CMU_MHU0_Receiver_IRQn);

    return TFM_HAL_SUCCESS;
}

/* Platform specific inter-processor communication interrupt handler. */
void CMU_MHU2_Receiver_Handler(void)
{
    (void)tfm_multi_core_hal_receive(&MHU_AP_TO_RSS_DEV, &MHU_RSS_TO_AP_DEV);

    /*
     * SPM will send a MAILBOX_SIGNAL to the corresponding partition
     * indicating that a message has arrived and can be processed.
     */
    spm_handle_interrupt(mbox_irq_info.p_pt, mbox_irq_info.p_ildi);
}

#ifdef MHU_V3_RSS_SI_CL0
/* Platform specific inter-processor communication interrupt handler. */
void CMU_MHU6_Receiver_Handler(void)
{
    (void)tfm_multi_core_hal_receive(&MHU_SI_CL0_TO_RSS_DEV,
                                     &MHU_RSS_TO_SI_CL0_DEV);

    /*
     * SPM will send a MAILBOX_SIGNAL to the corresponding partition
     * indicating that a message has arrived and can be processed.
     */
    spm_handle_interrupt(mbox_irq_info.p_pt, mbox_irq_info.p_ildi);
}
#endif /* MHU_V3_RSS_SI_CL0 */

#ifdef MHU_V3_RSS_SI_CL1
/* Platform specific inter-processor communication interrupt handler. */
void CMU_MHU7_Receiver_Handler(void)
{
    (void)tfm_multi_core_hal_receive(&MHU_SI_CL1_TO_RSS_DEV,
                                     &MHU_RSS_TO_SI_CL1_DEV);

    /*
     * SPM will send a MAILBOX_SIGNAL to the corresponding partition
     * indicating that a message has arrived and can be processed.
     */
    spm_handle_interrupt(mbox_irq_info.p_pt, mbox_irq_info.p_ildi);
}
#endif /* MHU_V3_RSS_SI_CL1 */

#ifdef MHU_V3_RSS_SI_CL2
/* Platform specific inter-processor communication interrupt handler. */
void CMU_MHU8_Receiver_Handler(void)
{
    (void)tfm_multi_core_hal_receive(&MHU_SI_CL2_TO_RSS_DEV,
                                     &MHU_RSS_TO_SI_CL2_DEV);

    /*
     * SPM will send a MAILBOX_SIGNAL to the corresponding partition
     * indicating that a message has arrived and can be processed.
     */
    spm_handle_interrupt(mbox_irq_info.p_pt, mbox_irq_info.p_ildi);
}
#endif /* MHU_V3_RSS_SI_CL2 */
#endif /* TFM_MULTI_CORE_TOPOLOGY */

static struct irq_t dma0_ch0_irq = {0};

void DMA_Combined_S_Handler(void)
{
    spm_handle_interrupt(dma0_ch0_irq.p_pt, dma0_ch0_irq.p_ildi);
}

enum tfm_hal_status_t tfm_dma0_combined_s_irq_init(void *p_pt,
                                          struct irq_load_info_t *p_ildi)
{
    dma0_ch0_irq.p_ildi = p_ildi;
    dma0_ch0_irq.p_pt = p_pt;

    NVIC_SetPriority(TFM_DMA0_COMBINED_S_IRQ, DEFAULT_IRQ_PRIORITY);
    NVIC_ClearTargetState(TFM_DMA0_COMBINED_S_IRQ);
    NVIC_DisableIRQ(TFM_DMA0_COMBINED_S_IRQ);

    return TFM_HAL_SUCCESS;
}
