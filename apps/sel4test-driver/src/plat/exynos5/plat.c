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
#include <platsupport/plat/clock.h>
#include <sel4platsupport/io.h>
#include <sel4platsupport/plat/timer.h>

void
plat_init_caps(env_t env)
{
    init_irq_cap(env, DEFAULT_TIMER_INTERRUPT, &env->irq_path);
    init_frame_cap(env, (void *) DEFAULT_TIMER_PADDR, &env->frame_path);
}

void
plat_init(env_t env)
{
    /* nothing to do */
}


