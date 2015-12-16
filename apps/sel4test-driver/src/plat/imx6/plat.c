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
    init_irq_cap(env, GPT1_INTERRUPT, &env->clock_irq_path);
    init_frame_cap(env, (void *) DEFAULT_TIMER_PADDR, &env->frame_path);
    init_frame_cap(env, (void *) GPT1_DEVICE_PADDR, &env->clock_frame_path);
}

void
plat_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{

    ZF_LOGV("Copying clock timer frame cap\n");
    init->clock_timer_frame = copy_cap_to_process(test_process, env->clock_frame_path.capPtr);
    if (init->clock_timer_frame == 0) {
        ZF_LOGF("Failed to copy clock timer frame cap to process");
    }

    ZF_LOGV("Copying clock timer irq\n");
    init->clock_timer_irq = copy_cap_to_process(test_process, env->clock_irq_path.capPtr);
}

void
plat_init(env_t env)
{
    int error;
    ps_io_ops_t io_ops;
    clock_sys_t clock;
    clk_t *clk;

    error = sel4platsupport_new_io_mapper(env->simple, env->vspace, env->vka, &io_ops.io_mapper);
    if (error != 0) {
        ZF_LOGF("Failed to initialise IO mapper");
    }

    error = clock_sys_init(&io_ops, &clock);
    if (error != 0) {
        ZF_LOGF("Failed to initalise clock");
    }

    clk = clk_get_clock(&clock, CLK_ARM);
    if (clk == NULL) {
        ZF_LOGF("Failed to get clock");
    }

    /* set the clock rate to 1GHz */
    clk_set_freq(clk, 1000 * MHZ);
}


