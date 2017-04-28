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
#include "../../test.h"

#include <platsupport/io.h>
#include <platsupport/plat/clock.h>
#include <sel4platsupport/io.h>
#include <sel4platsupport/device.h>
#include <sel4platsupport/timer.h>
#include <utils/zf_log.h>
#include <sel4utils/process.h>

void
plat_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    plat_timer_objects_t *plat_timer_objects = &env->timer_objects.arch_timer_objects.plat_timer_objects;

    ZF_LOGV("Copying clock timer frame cap %x\n", plat_timer_objects->clock_timer_dev_ut_obj.cptr);
    init->clock_timer_dev_ut_cap = sel4utils_copy_cap_to_process(test_process, &env->vka, plat_timer_objects->clock_timer_dev_ut_obj.cptr);
    if (init->clock_timer_dev_ut_cap == 0) {
        ZF_LOGF("Failed to copy clock timer device-ut cap to process");
    }
    init->clock_timer_paddr = GPT1_DEVICE_PADDR;

    ZF_LOGV("Copying clock timer irq\n");
    init->clock_timer_irq_cap = sel4utils_copy_cap_to_process(test_process, &env->vka, plat_timer_objects->clock_irq_path.capPtr);
    if (init->clock_timer_irq_cap == 0) {
        ZF_LOGF("Failed to copy clock timer irq cap to process");
    }
}


void
plat_copy_serial_caps(test_init_data_t *init, env_t env,
                      sel4utils_process_t *test_process)
{
}

void
plat_init(env_t env)
{
    int error;
    ps_io_ops_t io_ops;
    clock_sys_t clock;
    clk_t *clk;

    error = sel4platsupport_new_io_mapper(env->vspace, env->vka, &io_ops.io_mapper);
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
