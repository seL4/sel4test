/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <sel4test-driver/gen_config.h>
#include <sel4/sel4.h>
#include <vka/object.h>

#include "../timer.h"

#include <utils/util.h>

static bool test_finished;

typedef struct timer_test_data {
    int curr_count;
    int goal_count;
} timer_test_data_t;

static int test_callback(uintptr_t token)
{
    assert(token);
    timer_test_data_t *test_data = (timer_test_data_t *) token;
    test_data->curr_count++;
    if (test_data->curr_count == test_data->goal_count) {
        test_finished = true;
    }
    return 0;
}

int test_timer(driver_env_t env)
{
    test_finished = false;
    timer_test_data_t test_data = { .goal_count = 3 };

    int error = tm_alloc_id_at(&env->tm, TIMER_ID);
    test_assert_fatal(!error);

    error = tm_register_cb(&env->tm, TIMEOUT_PERIODIC, 100 * NS_IN_MS, 0, TIMER_ID,
                           test_callback, (uintptr_t) &test_data);
    test_assert_fatal(!error);

    while (!test_finished) {
        wait_for_timer_interrupt(env);
        ZF_LOGV("Tick");
        error = tm_update(&env->tm);
        test_assert_fatal(!error);
    }

    error = tm_free_id(&env->tm, TIMER_ID);
    test_assert_fatal(!error);

    error = ltimer_reset(&env->ltimer);
    test_assert_fatal(!error);

    return sel4test_get_result();
}
DEFINE_TEST_BOOTSTRAP(TIMER0001, "Basic timer testing", test_timer, config_set(CONFIG_HAVE_TIMER))

int
test_gettime_timeout(driver_env_t env)
{
    int error = 0;
    uint64_t start, end;
    test_finished = false;
    timer_test_data_t test_data = { .goal_count = 3 };

    error = tm_alloc_id_at(&env->tm, TIMER_ID);
    test_assert_fatal(!error);

    start = timestamp(env);
    error = tm_register_cb(&env->tm, TIMEOUT_PERIODIC, 1 * NS_IN_MS, 0, TIMER_ID,
                           test_callback, (uintptr_t) &test_data);
    test_assert_fatal(!error);

    while (!test_finished) {
        wait_for_timer_interrupt(env);
        ZF_LOGV("Tick");
        error = tm_update(&env->tm);
        test_assert_fatal(!error);
    }

    end = timestamp(env);

    test_gt(end, start);

    error = tm_free_id(&env->tm, TIMER_ID);
    test_assert_fatal(!error);

    error = ltimer_reset(&env->ltimer);
    test_assert_fatal(!error);

    return sel4test_get_result();
}
DEFINE_TEST_BOOTSTRAP(TIMER0002, "Test that the timer moves between gettime and timeout calls", test_gettime_timeout,
                      config_set(CONFIG_HAVE_TIMER))
