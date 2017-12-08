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

/* A pending timeout requests from tests */
static bool timeServer_timeoutPending = false;
static timeout_type_t timeServer_timeoutType;

static int timeout_cb(uintptr_t token) {
    seL4_Signal((seL4_CPtr) token);

    if (timeServer_timeoutType != TIMEOUT_PERIODIC) {
        timeServer_timeoutPending = false;
    }
    return 0;
}

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
        ZF_LOGD_IF(timeServer_timeoutPending, "Overwriting a previous timeout request\n");
        timeServer_timeoutType = timeout_type;
        int error = tm_register_cb(&env->tm, timeout_type, ns, 0,
                                  TIMER_ID, timeout_cb, env->timer_notify_test.cptr);
        if (error == ETIME) {
            error = timeout_cb(env->timer_notify_test.cptr);
        } else {
            timeServer_timeoutPending = true;
        }
        ZF_LOGF_IF(error != 0, "register_cb failed");
    } else {
        ZF_LOGF("There is no timer configured for this target");
    }
}

void timer_reset(driver_env_t env) {
    if (config_set(CONFIG_HAVE_TIMER)) {
        int error = tm_deregister_cb(&env->tm, TIMER_ID);
        ZF_LOGF_IF(error, "ltimer_rest failed");
        timeServer_timeoutPending = false;
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

void timer_cleanup(driver_env_t env) {
    ZF_LOGF_IF(!config_set(CONFIG_HAVE_TIMER), "There is no timer configured for this target");
    tm_free_id(&env->tm, TIMER_ID);
    timeServer_timeoutPending = false;
}
