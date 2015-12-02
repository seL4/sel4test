

#include "../../test.h"

#include <sel4platsupport/plat/timer.h>
#include <vka/capops.h>

void
plat_init_env(env_t env, test_init_data_t *data)
{
    env->timer = sel4platsupport_get_hpet(&env->vspace, &env->simple, NULL, &env->vka,
                                          env->timer_notification.cptr, MSI_MIN);
    if (env->timer == NULL) {
        ZF_LOGF("Failed to initialise HPET");
    }

    env->clock_timer = sel4platsupport_get_tsc_timer_freq(data->tsc_freq);
    if (env->clock_timer == NULL) {
        ZF_LOGF("Failed to intiialise tsc\n");
    }

    timer_start(env->clock_timer->timer);
}

seL4_Error
plat_get_frame_cap(void *data, void *paddr, int size_bits, cspacepath_t *path) 
{
    test_init_data_t *init = (test_init_data_t *) data;

    if ((uintptr_t) paddr == DEFAULT_HPET_ADDR) {
        return seL4_CNode_Copy(path->root, path->capPtr, path->capDepth,
                init->root_cnode, init->timer_frame, seL4_WordBits, seL4_AllRights);
    }
    
    return 0;
}

seL4_Error
plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    
    test_init_data_t *init = (test_init_data_t *) data;

    if (irq == MSI_MIN) {
        return seL4_CNode_Copy(root, index, depth, init->root_cnode,
                               init->timer_irq, seL4_WordBits, seL4_AllRights);
    }

    return -1;
}


