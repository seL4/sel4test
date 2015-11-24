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
#include <sel4platsupport/plat/timer.h>

void
arch_init_timer_caps(env_t env)
{
    env->io_port_cap = simple_get_IOPort_cap(&env->simple, PIT_IO_PORT_MIN, PIT_IO_PORT_MAX);
    if (env->io_port_cap == 0) {
        ZF_LOGF("Failed to get IO port cap for range %x to %x\n", PIT_IO_PORT_MIN, PIT_IO_PORT_MAX);
    }
}

/* copy the caps required to set up the sel4platsupport default timer */
void
arch_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    /* io port cap (since the default timer on ia32 is the PIT) */
    init->io_port = copy_cap_to_process(test_process, env->io_port_cap);
}
 
