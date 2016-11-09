/*
 *  Copyright 2016, Data61
 *  Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 *  ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */
#include <autoconf.h>
#include "../../test.h"

#include "../../init.h"
#include <sel4platsupport/plat/timer.h>
#include <vka/capops.h>

void
plat_init_env(env_t env, test_init_data_t *data)
{
#ifdef CONFIG_HAVE_TIMER
    env->clock_timer = sel4platsupport_get_tsc_timer_freq(data->tsc_freq);
    if (env->clock_timer == NULL) {
        ZF_LOGF("Failed to intiialise tsc\n");
    }

    timer_start(env->clock_timer->timer);
#endif /* CONFIG_HAVE_TIMER */
}

seL4_Error
plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{

    test_init_data_t *init = (test_init_data_t *) data;

    return seL4_CNode_Copy(root, index, depth, init->root_cnode,
                           init->timer_irq, seL4_WordBits, seL4_AllRights);
}

void
plat_add_uts(UNUSED env_t env, UNUSED allocman_t *alloc, UNUSED test_init_data_t *data)
{
    /* nothing to do */
}
