/*
 * Copyright 2026, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <autoconf.h>
#include <sel4test-driver/gen_config.h>
#include <assert.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <vka/object.h>

#include "../helpers.h"

void sched_context_smp_001_recipient_fn(seL4_CPtr ep, void *arg1, void *arg2, void *arg3)
{
    /* Basically, say yes we ran */
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    seL4_Send(ep, tag);
}

int test_smp_bind_tcb_other_core(env_t env)
{
    /**
     * This is for testing when the target TCB has no SchedContext.
     *
     * Cases:
     *  - bind local SC to not running remote TCB
     *  - bind remote SC to not running remote TCB
     *  - bind remote SC to local thread (which necessarily is not running)
     *
     * The local:local case is tested elsewhere.
     **/

    helper_thread_t bind_recipient;
    seL4_CPtr ep_recipient;
    seL4_CPtr sc;
    int error;

    seL4_CPtr sc_ctrl_local = simple_get_sched_ctrl(&env->simple, 0);
    seL4_CPtr sc_ctrl_remote = simple_get_sched_ctrl(&env->simple, 1);
    seL4_Time timeslice = CONFIG_BOOT_THREAD_TIME_SLICE * US_IN_S;

    ep_recipient = vka_alloc_endpoint_leaky(&env->vka);

    {
        printf("Local SC to not running remote TCB\n");

        /* Our SC is local */
        sc = vka_alloc_sched_context_leaky(&env->vka);
        error = seL4_SchedControl_Configure(sc_ctrl_local, sc, timeslice, timeslice, 0, 0);
        ZF_LOGF_IF(error, "should be able to configure SC");

        create_helper_thread(env, &bind_recipient);
        NAME_THREAD(get_helper_tcb(&bind_recipient), "SCHED_CONTEXT_SMP_001 Bind Recipient");

        /* move the recipient to the other core so it is a remote TCB. */
        set_helper_affinity(env, &bind_recipient, /* core */ 1);
        /* unbind recipient so we can bind to it, also so it doesn't run */
        error = api_sc_unbind(get_helper_sched_context(&bind_recipient));
        ZF_LOGF_IF(error, "unable to unbind");

        start_helper(env, &bind_recipient, (helper_fn_t) sched_context_smp_001_recipient_fn, ep_recipient, 0, 0, 0);

        int error = seL4_SchedContext_Bind(sc, get_helper_tcb(&bind_recipient));
        test_eq(error, seL4_NoError);

        /* We know that this thread got to run if it signalled this */
        seL4_Wait(ep_recipient, NULL);

        cleanup_helper(env, &bind_recipient);
    }

    {
        printf("Remote SC to not running remote TCB\n");

        /* Our SC is remote */
        sc = vka_alloc_sched_context_leaky(&env->vka);
        error = seL4_SchedControl_Configure(sc_ctrl_remote, sc, timeslice, timeslice, 0, 0);
        ZF_LOGF_IF(error, "should be able to configure SC");

        create_helper_thread(env, &bind_recipient);
        NAME_THREAD(get_helper_tcb(&bind_recipient), "SCHED_CONTEXT_SMP_001 Bind Recipient");

        /* move the recipient to the other core so it is a remote TCB. */
        set_helper_affinity(env, &bind_recipient, /* core */ 1);
        /* unbind recipient so we can bind to it, also so it doesn't run */
        error = api_sc_unbind(get_helper_sched_context(&bind_recipient));
        ZF_LOGF_IF(error, "unable to unbind");

        start_helper(env, &bind_recipient, (helper_fn_t) sched_context_smp_001_recipient_fn, ep_recipient, 0, 0, 0);

        int error = seL4_SchedContext_Bind(sc, get_helper_tcb(&bind_recipient));
        test_eq(error, seL4_NoError);

        /* We know that this thread got to run if it signalled this */
        seL4_Wait(ep_recipient, NULL);

        cleanup_helper(env, &bind_recipient);
    }

    {
        printf("Remote SC to local thread\n");

        /* Our SC is remote */
        sc = vka_alloc_sched_context_leaky(&env->vka);
        error = seL4_SchedControl_Configure(sc_ctrl_remote, sc, timeslice, timeslice, 0, 0);
        ZF_LOGF_IF(error, "should be able to configure SC");

        create_helper_thread(env, &bind_recipient);
        NAME_THREAD(get_helper_tcb(&bind_recipient), "SCHED_CONTEXT_SMP_001 Bind Recipient");

        /* unbind recipient so we can bind to it, also so it doesn't run */
        error = api_sc_unbind(get_helper_sched_context(&bind_recipient));
        ZF_LOGF_IF(error, "unable to unbind");

        start_helper(env, &bind_recipient, (helper_fn_t) sched_context_smp_001_recipient_fn, ep_recipient, 0, 0, 0);

        int error = seL4_SchedContext_Bind(sc, get_helper_tcb(&bind_recipient));
        test_eq(error, seL4_NoError);

        /* We know that this thread got to run if it signalled this */
        seL4_Wait(ep_recipient, NULL);

        cleanup_helper(env, &bind_recipient);
    }

    return sel4test_get_result();
}

DEFINE_TEST(SCHED_CONTEXT_SMP_001, "Test SC bind to a TCB on another core",
            test_smp_bind_tcb_other_core, config_set(CONFIG_KERNEL_MCS) && (CONFIG_MAX_NUM_NODES > 1));

int sched_context_smp_002_helper_fn(void)
{
    return 1;
}

void sched_context_smp_002_lazy_fn(seL4_CPtr notification_in, seL4_CPtr notification_out)
{
    seL4_Signal(notification_out);

    seL4_Wait(notification_in, NULL);

    seL4_Signal(notification_out);

    seL4_Wait(notification_in, NULL);
}

int test_passive_thread_start_smp(env_t env)
{
    helper_thread_t helper;
    seL4_CPtr notification = vka_alloc_notification_leaky(&env->vka);
    int error;

    seL4_Time timeslice = CONFIG_BOOT_THREAD_TIME_SLICE * US_IN_S;

    seL4_CPtr sc_remote = vka_alloc_sched_context_leaky(&env->vka);
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, 1), sc_remote, timeslice, timeslice, 0, 0);
    ZF_LOGF_IF(error, "should be able to configure SC");

    create_helper_thread(env, &helper);

    /* unbind the default SC; + make it so that there is no SC. */
    error = api_sc_unbind(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* ========== resume then bind ================ */
    start_helper(env, &helper, (helper_fn_t) sched_context_smp_002_helper_fn, 0, 0, 0, 0);

    error = api_sc_bind(sc_remote, helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    error = wait_for_helper(&helper);
    test_eq(error, 1);

    /* cleanup */
    cleanup_helper(env, &helper);

    /* ============ bind then resume =============== */
    create_helper_thread(env, &helper);

    /* unbind the default SC; + make it so that there is no SC. */
    error = api_sc_unbind(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = api_sc_bind(sc_remote, helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    start_helper(env, &helper, (helper_fn_t) sched_context_smp_002_helper_fn, 0, 0, 0, 0);

    error = seL4_TCB_Resume(helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    error = wait_for_helper(&helper);
    test_eq(error, 1);

    /* cleanup */
    cleanup_helper(env, &helper);

    /* ============ lazy unbind =================== */
    create_helper_thread(env, &helper);

    /* unbind the default SC */
    error = api_sc_unbind(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);
    /* add back our desired SC */
    error = api_sc_bind(sc_remote, helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    error = api_sc_bind(sc_remote, notification);
    test_eq(error, seL4_NoError);
    /* set helper to higher prio to make behaviour deterministic */
    set_helper_priority(env, &helper, env->priority + 1);

    /* double check that the tcb is still bound */
    error = api_sc_bind(sc_remote, helper.thread.tcb.cptr);
    test_eq(error, seL4_IllegalOperation);

    seL4_CPtr notification_in = vka_alloc_notification_leaky(&env->vka);
    start_helper(env, &helper, (helper_fn_t)sched_context_smp_002_lazy_fn, notification, notification_in, 0, 0);

    /* Unfortunately, unlike SCHED_CONTEXT_0007 we can't test that we can rebind the
     * TCB SC to test that this is the case, because there's no way to atomically
     * or deterministically wait for the helper thread on the other core to run
     * the seL4_Wait().
     * However we can check that it has run and that it can run again after
     * being signalled. */
    seL4_Wait(notification_in, NULL);
    seL4_Signal(notification);
    seL4_Wait(notification_in, NULL);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_SMP_002, "test resuming a passive thread and binding scheduling context on another core",
            test_passive_thread_start_smp, config_set(CONFIG_KERNEL_MCS) && (CONFIG_MAX_NUM_NODES > 1));
