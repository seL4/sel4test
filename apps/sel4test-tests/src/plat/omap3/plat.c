/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "../../test.h"

#include <sel4platsupport/plat/timer.h>

void
plat_init_env(env_t env, test_init_data_t *data)
{
    env->timer = sel4platsupport_get_rel_gpt(&env->vspace, &env->simple, &env->vka,
                                             env->timer_notification.cptr, GPT1, 0);
    if (env->timer == NULL) {
        ZF_LOGF("Failed to initialise default timer");
    }

    env->clock_timer = sel4platsupport_get_abs_gpt(&env->vspace, &env->simple, &env->vka,
                                                   env->timer_notification.cptr, GPT2, 0);
    
    if (env->clock_timer == NULL) {
        ZF_LOGF("Failed to intiialise gpt\n");
    }

    timer_start(env->clock_timer->timer);
}

seL4_Error
plat_get_frame_cap(void *data, void *paddr, int size_bits, cspacepath_t *path) 
{
    test_init_data_t *init = (test_init_data_t *) data;

    if (size_bits != seL4_PageBits) {
        ZF_LOGE("Unknown size %d\n", size_bits);
        return -1;
    }

    seL4_CPtr frame;
    if ((uintptr_t) paddr == GPT1_DEVICE_PADDR) {
        frame = init->timer_frame;
    } else if ((uintptr_t) paddr == GPT2_DEVICE_PADDR) {
        frame = init->clock_timer_frame;
    } else {
        ZF_LOGE("Unknown paddr %p\n", paddr);
        return -1;
    }

    return seL4_CNode_Copy(path->root, path->capPtr, path->capDepth, init->root_cnode,
                               frame, seL4_WordBits, seL4_AllRights);
}

seL4_Error
plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    test_init_data_t *init = (test_init_data_t *) data;
    seL4_CPtr irq_cptr;

    if (irq == GPT1_INTERRUPT) {
        irq_cptr = init->timer_irq;
    } else if (irq == GPT2_INTERRUPT) {
        irq_cptr = init->clock_timer_irq;
    } else {
        ZF_LOGE("Unknown irq %d\n", irq);
        return -1;
    }

    return seL4_CNode_Copy(root, index, depth, init->root_cnode,
                           irq_cptr, seL4_WordBits, seL4_AllRights);
}

