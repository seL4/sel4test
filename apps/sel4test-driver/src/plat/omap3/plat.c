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
    init_irq_cap(env, GPT2_INTERRUPT, &env->clock_irq_path);
    init_frame_cap(env, (void *) DEFAULT_TIMER_PADDR, &env->frame_path);
    init_frame_cap(env, (void *) GPT2_DEVICE_PADDR, &env->clock_frame_path);
}

void
plat_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{

    init->clock_timer_frame = copy_cap_to_process(test_process, env->clock_frame_path.capPtr);
    if (init->clock_timer_frame == 0) {
        ZF_LOGF("Failed to copy clock timer frame cap to process");
    }

    init->clock_timer_irq = copy_cap_to_process(test_process, env->clock_irq_path.capPtr);
}

void
plat_init(env_t env)
{
   /* No plat specific init */
}

