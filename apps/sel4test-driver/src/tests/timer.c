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

#include "../timer.h"

#include <utils/util.h>

int
test_timer(driver_env_t env)
{
    int error = ltimer_set_timeout(&env->timer.ltimer, 1 * NS_IN_S, TIMEOUT_PERIODIC);
    test_assert_fatal(!error);

    for (int i = 0; i < 3; i++) {
        wait_for_timer_interrupt(env);
        ZF_LOGV("Tick\n");
    }

    error = ltimer_reset(&env->timer.ltimer);
    test_assert_fatal(!error);

    return sel4test_get_result();
}
DEFINE_TEST_BOOTSTRAP(TIMER0001, "Basic timer testing", test_timer, config_set(CONFIG_HAVE_TIMER))

int
test_gettime_timeout(driver_env_t env)
{
    int error = 0;
    uint64_t start, end;

    start = timestamp(env);
    error = ltimer_set_timeout(&env->timer.ltimer, 1 * NS_IN_MS, TIMEOUT_PERIODIC);
    test_assert_fatal(!error);

    for (int i = 0; i < 3; i++) {
        wait_for_timer_interrupt(env);
        ZF_LOGV("Tick\n");
    }

    end = timestamp(env);

    test_ge(end, start);

    error = ltimer_reset(&env->timer.ltimer);
    test_assert_fatal(!error);

    return sel4test_get_result();
}
DEFINE_TEST_BOOTSTRAP(TIMER0002, "Test that the timer moves between gettime and timeout calls", test_gettime_timeout, config_set(CONFIG_HAVE_TIMER))
