/*
 * Copyright 2018, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#include "../../test.h"

#include <platsupport/io.h>
#include <platsupport/plat/clock.h>
#include <sel4platsupport/io.h>
#include <sel4platsupport/device.h>

void
plat_init_timer_caps(driver_env_t env)
{
}

void
plat_copy_timer_caps(test_init_data_t *init, driver_env_t env, sel4utils_process_t *test_process)
{
}

int
plat_init_serial_caps(env_t env)
{
    return 0;
}

void
plat_copy_serial_caps(test_init_data_t *init, driver_env_t env,
                       sel4utils_process_t *test_process)
{
}

void
plat_init(driver_env_t env)
{
    ZF_LOGD("Spike: plat_init: Done.");
}
