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
    init_irq_cap(env, MSI_MIN, &env->irq_path);
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

    seL4_timer_t *hpet = sel4platsupport_get_hpet(&env->vspace, &env->simple, NULL, &env->vka,
                                                  notification.cptr, MSI_MIN);
    if (hpet == NULL) {
        ZF_LOGF("Failed to init hpet");
    }

    env->tsc_freq = tsc_calculate_frequency(hpet->timer) / US_IN_S;
    if (env->tsc_freq == 0) {
        ZF_LOGF("Failed to calculate tsc frequency");
    } else {
        ZF_LOGV("Calculated TSC freq %llu\n", env->tsc_freq);
    }

    sel4platsupport_destroy_hpet(hpet, &env->vka, &env->vspace);
    vka_free_object(&env->vka, &notification);
}


