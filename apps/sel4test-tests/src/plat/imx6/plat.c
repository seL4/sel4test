

#include "../../test.h"

#include <sel4platsupport/plat/timer.h>


void
plat_init_env(env_t env)
{
    env->timer = sel4platsupport_get_default_timer(&env->vka, &env->vspace,
                                                   &env->simple, env->timer_notification.cptr);
    if (env->timer == NULL) {
        ZF_LOGF("Failed to initialise default timer");
    }

    env->clock_timer = sel4platsupport_get_gpt(&env->vspace, &env->simple, &env->vka,
                                               env->timer_notification.cptr, 999u);
    if (env->clock_timer == NULL) {
        ZF_LOGF("Failed to intiialise gpt\n");
    }

    timer_start(env->clock_timer->timer);
}

seL4_Error
plat_get_frame_cap(void *data, void *paddr, int size_bits, cspacepath_t *path) 
{
    test_init_data_t *init = (test_init_data_t *) data;

    if ((uintptr_t) paddr == GPT1_DEVICE_PADDR && size_bits == seL4_PageBits) {
        return seL4_CNode_Copy(path->root, path->capPtr, path->capDepth, init->root_cnode,
                               init->clock_timer_frame, seL4_WordBits, seL4_AllRights);
    }

    return -1;
}

seL4_Error
plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    test_init_data_t *init = (test_init_data_t *) data;

    if (irq == GPT1_INTERRUPT) {
        return seL4_CNode_Copy(root, index, depth, init->root_cnode,
                               init->clock_timer_irq, seL4_WordBits, seL4_AllRights);
    }

    return -1;
}


