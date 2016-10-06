/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <sel4/sel4.h>
#include <vka/object.h>

#include "../helpers.h"

#include <utils/util.h>

#if CONFIG_HAVE_TIMER

static int
test_interrupt(env_t env)
{

    int error = timer_start(env->timer->timer);
    test_eq(error, 0);

    error = timer_periodic(env->timer->timer, 1 * NS_IN_S);
    test_eq(error, 0);

    sel4_timer_handle_single_irq(env->timer);

    for (int i = 0; i < 3; i++) {
        wait_for_timer_interrupt(env);
        ZF_LOGV("Tick\n");
    }

    timer_stop(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);

    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0001, "Test interrupts with timer", test_interrupt);
#endif
