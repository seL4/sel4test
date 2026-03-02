/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/* Include Kconfig variables. */
#include <autoconf.h>
#include <sel4test-driver/gen_config.h>

#include <assert.h>
#include <stdio.h>

/* Our headers are not C++ friendly */
extern "C" {

#include <sel4/sel4.h>

#include "../helpers.h"

}

#ifdef CONFIG_MCS
#ifndef CONFIG_TIMER_FREQUENCY
/* Probably x86, just use 1 GHz, doesn't really matter anyway */
#define CONFIG_TIMER_FREQUENCY (1000 * 1000 * 1000)
#endif
/* Duration is in timer ticks: */
#define DURATION (CONFIG_TIMER_FREQUENCY / 1000)

#else
/* Duration is in scheduler ticks: */
#define DURATION 1
#endif

/* This is a domain schedule that is suitable for the domains tests in sel4test. All
 * sel4test actually needs is for every domain to be executable for some period of time
 * in order for the tests to make progress.
 *
 * We pick 2 ticks as the shortest period so that tests can make some progress if they exist,
 * and we pick some variety in the first four domains so that not everything is equal.
 */
#define D0 (DURATION * 10)
#define D1 (DURATION * 4)
#define D2 (DURATION * 3)
#define D3 (DURATION * 2)

/* Create the domain schedule for this test, match old domain_schedule.c */
static void init_domain_schedules(struct env *env)
{
    int error;
    seL4_Time duration[] = {D0, D1, D2};

    assert(CONFIG_NUM_DOMAINS <= CONFIG_NUM_DOMAIN_SCHEDULES);
    for (seL4_Word i = 0; i < CONFIG_NUM_DOMAINS; ++i) {
        seL4_Time d = i < 3 ? duration[i] : D3;
        error = seL4_DomainSet_ScheduleConfigure(env->domain, i, i, d);
        assert(error == seL4_NoError);
    }
    /* Force a reload of the updated config: */
    seL4_DomainSet_ScheduleSetStart(env->domain, 0);
}

static void cleanup_domain_schedules(struct env *env)
{
    int error;

    /* Set maximum duration: */
    error = seL4_DomainSet_ScheduleConfigure(env->domain, 0, 0, 0x00ffffffffffffUL);
    assert(error == seL4_NoError);
    /* Set end marker: */
    error = seL4_DomainSet_ScheduleConfigure(env->domain, 0, 1, 0);
    assert(error == seL4_NoError);
}

#define POLL_DELAY_NS 4000000

typedef int (*test_func_t)(seL4_Word /* id */, env_t env /* env */);

static int
own_domain_success(struct env *env)
{
    int error;

    error = seL4_DomainSet_Set(env->domain, CONFIG_NUM_DOMAINS - 1,
                               env->tcb);
    return (error == seL4_NoError) ? SUCCESS : FAILURE;
}

static int
own_domain_baddom(struct env *env)
{
    int error;

    error = seL4_DomainSet_Set(env->domain, CONFIG_NUM_DOMAINS + 10,
                               env->tcb);
    return (error == seL4_InvalidArgument) ? SUCCESS : FAILURE;
}

static int
own_domain_badcap(struct env *env)
{
    int error;

    error = seL4_DomainSet_Set(env->tcb, 0,
                               env->tcb);
    return (error == seL4_IllegalOperation) ? SUCCESS : FAILURE;
}

int fdom1(seL4_Word id, env_t env)
{
    int countdown = 50;

    while (countdown > 0) {
        sleep_busy(env, POLL_DELAY_NS);
        --countdown;
        ZF_LOGD("%2d, ", (int)id);
    }

    return sel4test_get_result();
}

/* This is a very simple (and rather stupid) C++ usage. Proves that a template
 * can be defined but is not good C++ */
template<bool shift, typename F>
static int
test_domains(struct env *env, F func)
{
    UNUSED int error;
    helper_thread_t thread[CONFIG_NUM_DOMAINS];

    init_domain_schedules(env);

    for (int i = 0; i < CONFIG_NUM_DOMAINS; ++i) {
        create_helper_thread(env, &thread[i]);
        set_helper_priority(env, &thread[i], 64);
        error = seL4_DomainSet_Set(env->domain, (seL4_Word)i, get_helper_tcb(&thread[i]));
        assert(error == seL4_NoError);
    }

    for (int i = 0; i < CONFIG_NUM_DOMAINS; ++i) {
        start_helper(env, &thread[i], (helper_fn_t) func, i, (seL4_Word) env, 0, 0);
    }

    if (CONFIG_NUM_DOMAINS > 1 && shift) {
        sel4test_sleep(env, POLL_DELAY_NS * 2);
        error = seL4_DomainSet_Set(env->domain, CONFIG_NUM_DOMAINS - 1, get_helper_tcb(&thread[0]));
        assert(error == seL4_NoError);
    }

    for (int i = 0; i < CONFIG_NUM_DOMAINS; ++i) {
        wait_for_helper(&thread[i]);
        cleanup_helper(env, &thread[i]);
    }
    cleanup_domain_schedules(env);
    return sel4test_get_result();
}

/* The output from this test should show alternating "domain blocks", with,
 * within each, a single thread printing. For example:
 *
 * 00, 00, ..., 00, 01, 01, ..., 01, 00, 00, ..., 00, 01, 01, ..., 01, etc.
 * +-------------+  +-------------+  +-------------+  +-------------+
 *  block 0           block 1          block 0          block 1
 */
static int
test_run_domains(struct env* env)
{
    return test_domains<false>(env, fdom1);
}
DEFINE_TEST(DOMAINS0004, "Run threads in domains()", test_run_domains, config_set(CONFIG_HAVE_TIMER))

/* The output of this test differs from that of DOMAINS0004 in that the thread
 * in domain 0 is moved into domain 1 after a short delay. This should be
 * visible in the output, where the "domain block" for domain 1 should contain
 * the alternating output of threads 0 and 1. For example:
 *
 * 00, 00, ..., 00, 01, 00, 01, 00, ..., 01, 01, 01, 01
 * +-------------+  +------------------+ +------------+
 *  initial block    alterations           final block
 *  (due to delay)   (after shift)         (01 catches up)
 */
static int
test_run_domains_shift(struct env* env)
{
    return test_domains<true>(env, fdom1);
}
DEFINE_TEST(DOMAINS0005, "Move thread between domains()", test_run_domains_shift, config_set(CONFIG_HAVE_TIMER) && CONFIG_NUM_DOMAINS > 1)

static int
test_own_domain1(struct env* env)
{
    return own_domain_success(env);
}
DEFINE_TEST(DOMAINS0001, "Change domain successfully()", test_own_domain1, true)

static int
test_own_domain2(struct env* env)
{
    return own_domain_baddom(env);
}
DEFINE_TEST(DOMAINS0002, "Try non-existant domain()", test_own_domain2, true)

static int
test_own_domain3(struct env* env)
{
    return own_domain_badcap(env);
}
DEFINE_TEST(DOMAINS0003, "Invoke non-domain cap()", test_own_domain3, true)
