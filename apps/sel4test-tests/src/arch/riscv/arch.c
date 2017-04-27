/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#include "../../init.h"

//#include <platsupport/plat/serial.h>
//#include <sel4platsupport/device.h>


seL4_timer_t *
arch_init_timer(env_t env, test_init_data_t *data)
{
    return NULL;
}

void
arch_init_allocator(env_t env, test_init_data_t *data)
{
    /* nothing to do */
}
