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

#include <platsupport/io.h>
#include <sel4platsupport/plat/timer.h>

void
plat_init_caps(env_t env)
{
    /* get the msi irq cap */
    seL4_CPtr cap;
    int error = vka_cspace_alloc(&env->vka, &cap);
    if (error != 0) {
        ZF_LOGF("Failed to allocate cslot, error %d", error);
    }

    vka_cspace_make_path(&env->vka, cap, &env->irq_path);
    error = arch_simple_get_msi(&env->simple.arch_simple, env->irq_path, 0, 0, 0, 0, MSI_MIN);

    /* this relies on a hack in the kernel that exposes the HPET */
    init_frame_cap(env, (void *) DEFAULT_HPET_ADDR, &env->frame_path);

}

void
plat_init(env_t env)
{
    /* calculate the tsc frequency */
    vka_object_t notification = {0};
    int error = vka_alloc_notification(&env->vka, &notification);
    if (error) {
        ZF_LOGF("Failed to allocate notification object");
    }

    env->tsc_freq = seL4_GetBootInfo()->archInfo;
}


