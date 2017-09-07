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
#include <autoconf.h>
#include <sel4/sel4.h>
#include "timer.h"
#include <utils/util.h>
#include <sel4testsupport/testreporter.h>

void
wait_for_timer_interrupt(driver_env_t env)
{
    if (config_set(CONFIG_HAVE_TIMER)) {
        seL4_Word sender_badge;
        seL4_Wait(env->timer_notification.cptr, &sender_badge);
        sel4platsupport_handle_timer_irq(&env->timer, sender_badge);
    } else {
        ZF_LOGF("There is no timer configured for this target");
    }
}

void timeout(driver_env_t env, uint64_t ns, timeout_type_t timeout_type) {
    if (config_set(CONFIG_HAVE_TIMER)) {
        int error = ltimer_set_timeout(&env->timer.ltimer, ns, timeout_type);
        test_eq(error, 0);
    } else {
        ZF_LOGF("There is no timer configured for this target");
    }
}

void timer_reset(driver_env_t env) {
    if (config_set(CONFIG_HAVE_TIMER)) {
        int error = ltimer_reset(&env->timer.ltimer);
        test_eq(error, 0);
    } else {
        ZF_LOGF("There is no timer configured for this target");
    }
}

uint64_t
timestamp(driver_env_t env)
{
    uint64_t time = 0;
    if(config_set(CONFIG_HAVE_TIMER)) {
        int error = ltimer_get_time(&env->timer.ltimer, &time);
        ZF_LOGF_IF(error, "failed to get time");

    } else {
        ZF_LOGF("There is no timer configured for this target");
    }
    return time;
}
