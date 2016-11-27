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
#include <sel4platsupport/plat/timer.h>

void
plat_init_timer_caps(env_t env)
{
    /* Platforms that use a different device/driver to satisfy their wall-clock
     * timer requirement may be initialized here.
     */
    int error = sel4platsupport_copy_irq_cap(&env->vka, &env->simple, GPT1_INTERRUPT, &env->clock_irq_path);
    ZF_LOGF_IF(error != 0, "Failed to get GPT1_INTERRUPT");

    error = vka_alloc_untyped_at(&env->vka, seL4_PageBits, GPT1_DEVICE_PADDR, &env->clock_timer_dev_ut_obj);
    ZF_LOGF_IF(error != 0, "Failed to allocate GPT1 device-untyped");
}

void
plat_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{

    ZF_LOGV("Copying clock timer frame cap %x\n", env->clock_timer_dev_ut_obj.cptr);
    init->clock_timer_dev_ut_cap = copy_cap_to_process(test_process, env->clock_timer_dev_ut_obj.cptr);
    if (init->clock_timer_dev_ut_cap == 0) {
        ZF_LOGF("Failed to copy clock timer device-ut cap to process");
    }
    init->clock_timer_paddr = GPT1_DEVICE_PADDR;

    ZF_LOGV("Copying clock timer irq\n");
    init->clock_timer_irq_cap = copy_cap_to_process(test_process, env->clock_irq_path.capPtr);
    if (init->clock_timer_irq_cap == 0) {
        ZF_LOGF("Failed to copy clock timer irq cap to process");
    }
}

int
plat_init_serial_caps(env_t env)
{
    return 0;
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


