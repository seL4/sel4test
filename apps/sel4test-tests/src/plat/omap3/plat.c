/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#include "../../test.h"
#include "../../helpers.h"

#include <sel4platsupport/plat/timer.h>

void
plat_add_uts(env_t env, allocman_t *alloc, test_init_data_t *data)
{
    seL4_Word size_bits = seL4_PageBits;
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, data->clock_timer_dev_ut_cap, &path);
    int error = allocman_utspace_add_uts(alloc, 1, &path, &size_bits,
                                     &data->clock_timer_paddr,
                                     ALLOCMAN_UT_DEV);
    ZF_LOGF_IF(error, "Failed to add GPT untyped to allocator");
}


void
plat_init_env(env_t env, test_init_data_t *data)
{
    env->clock_timer = sel4platsupport_get_abs_gpt(&env->vspace, &env->simple, &env->vka,
                                                   env->timer_notification.cptr, GPT2, 0);

    if (env->clock_timer == NULL) {
        ZF_LOGF("Failed to intiialise gpt\n");
    }

    timer_start(env->clock_timer->timer);
}

seL4_Error
plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    test_init_data_t *init = (test_init_data_t *) data;

    if (irq == GPT2_INTERRUPT) {
    	return seL4_CNode_Copy(root, index, depth, init->root_cnode,
                               init->clock_timer_irq_cap, seL4_WordBits, seL4_AllRights);
	}
    return -1;
}
