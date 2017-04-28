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
plat_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{

    init->clock_timer_dev_ut_cap = copy_cap_to_process(test_process, env->clock_timer_dev_ut_obj.cptr);
    if (init->clock_timer_dev_ut_cap == 0) {
        ZF_LOGF("Failed to copy clock timer frame cap to process");
    }
    init->clock_timer_paddr = GPT2_DEVICE_PADDR;
    init->clock_timer_irq_cap = copy_cap_to_process(test_process, env->clock_irq_path.capPtr);
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
   /* No plat specific init */
}

