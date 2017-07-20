/*
 * Copyright 2017, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */

#include <sel4/sel4.h>
#include <vka/object.h>

#include "../helpers.h"

#include <utils/util.h>

#if CONFIG_HAVE_TIMER

static int
test_interrupt(env_t env)
{
    int error = ltimer_set_timeout(&env->timer.ltimer, 1 * NS_IN_S, TIMEOUT_PERIODIC);
    test_eq(error, 0);

    for (int i = 0; i < 3; i++) {
        wait_for_timer_interrupt(env);
        ZF_LOGV("Tick\n");
    }

    ltimer_reset(&env->timer.ltimer);

    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0001, "Test interrupts with timer", test_interrupt);
#endif
