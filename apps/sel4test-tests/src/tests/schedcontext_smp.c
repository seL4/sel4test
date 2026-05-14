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

/* FIXME: this is a temporary hack and should be exported via libsel4.
 *        https://github.com/seL4/seL4/issues/1659
 */
#ifndef MAX_PERIOD_US
/* Conservative, but long enough (~ half an hour) */
#define MAX_PERIOD_US (1ULL << 31)
#endif

#ifndef MIN_BUDGET_US
#ifdef CONFIG_PLAT_TK1
#define MIN_BUDGET_US (2 * 100)
#else
#define MIN_BUDGET_US (2 * 10)
#endif
#endif

static inline seL4_CPtr badge_endpoint(env_t env, seL4_Word badge, seL4_CPtr ep)
{
    seL4_CPtr slot = get_free_slot(env);
    int error = cnode_mint(env, ep, slot, seL4_AllRights, badge);
    test_error_eq(error, seL4_NoError);
    return slot;
}

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

void sched_context_smp_002_lazy_fn(seL4_CPtr endpoint, seL4_CPtr notification)
{
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    (void)seL4_NBSendWait(endpoint, tag, notification, NULL);
}

void sched_context_smp_002_high_priority_helper(seL4_CPtr endpoint_cross, seL4_CPtr ep_hi_to_low)
{
    (void)seL4_Wait(endpoint_cross, NULL);

    seL4_Send(ep_hi_to_low, seL4_MessageInfo_new(0, 0, 0, 0));
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

    /* To be able to reproduce the test in SCHED_CONTEXT_007 where we rebind the
     * TCB SC to test that it has been returned away, it is difficult to do
     * deterministically. The best solution I can think of is to start a higher
     * priority thread on this core that waits on an endpoint; this test thread
     * should then only continue once that is blocked. Then the other core thread
     * can perform an NBSendWait which should result in returning the TCB SC.
     * This will then allow us to in this thread wait for the higher priority
     * local thread to signal us, and so we can guarantee at this point the
     * remote thread will be blocked on Wait.
     */

    helper_thread_t hi_helper;
    create_helper_thread(env, &hi_helper);
    set_helper_priority(env, &hi_helper, env->priority + 1);
    seL4_CPtr endpoint_cross = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr endpoint_hi_low = vka_alloc_endpoint_leaky(&env->vka);
    /* this will run and receive first */
    start_helper(env, &hi_helper, (helper_fn_t)sched_context_smp_002_high_priority_helper, endpoint_cross, endpoint_hi_low, 0, 0);

    start_helper(env, &helper, (helper_fn_t)sched_context_smp_002_lazy_fn, endpoint_cross, notification, 0, 0);

    seL4_Wait(endpoint_hi_low, NULL);

    /* the tcb should have been unbound lazily when the helper called seL4_Wait */
    error = api_sc_bind(get_helper_sched_context(&helper), get_helper_tcb(&helper));
    test_eq(error, seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_SMP_002, "test resuming a passive thread and binding scheduling context on another core",
            test_passive_thread_start_smp, config_set(CONFIG_KERNEL_MCS) && (CONFIG_MAX_NUM_NODES > 1));

/*
 * Once https://github.com/seL4/seL4/issues/1617 is fixed, an equivalent of
 * BIND0007 should be added here as SCHED_CONTEXT_SMP_003.
 */

void sched_context_smp_004_helper_fn(env_t env, seL4_CPtr ep)
{
    /* Make sure this has run for at least MIN_BUDGET_US by poll-waiting that long */
    uint64_t start_ns = sel4test_timestamp(env);
    uint64_t end_ns = start_ns + MIN_BUDGET_US * NS_IN_US;
    while (sel4test_timestamp(env) < end_ns) {
        for (int i = 0; i < 1000; i++) {
            asm volatile("nop" ::: "memory");
        }
    }

    seL4_Send(ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

int test_update_remote_sc_with_budget(env_t env)
{

    /**
     * Idea behind this: a remote thread might have run out of a budget.
     * If we make a remote thread that has a tiny budget (the minimum)
     * but a very long period (the maximum) then once it will runout it will
     * basically never refill: so, if we reconfigure the SC to have full
     * bandwidth (budget = period) then it should start running again.
     **/

    int error;
    helper_thread_t helper;
    seL4_CPtr ep_unbadged = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr ep = badge_endpoint(env, 0x100, ep_unbadged);

    create_helper_thread(env, &helper);

    /* make the helper run on the remote core with no budget */
    error = seL4_SchedControl_Configure(
        /* schedcontrol */ simple_get_sched_ctrl(&env->simple, 1),
        /* schedcontext */ get_helper_sched_context(&helper),
        /* budget */ MIN_BUDGET_US, /* period */ MAX_PERIOD_US, /* refills */ 0, /* badge */ 0);
    ZF_LOGF_IF(error, "should be able to configure SC");

    start_helper(env, &helper, (helper_fn_t)sched_context_smp_004_helper_fn, (seL4_Word)env, ep, 0, 0);

    /* Mostly a hack: wait at least twice MIN_BUDGET_US */
    sel4test_sleep(env, 2 * MIN_BUDGET_US * NS_IN_US);

    seL4_Word sender_badge;
    seL4_NBWait(ep, &sender_badge);
    /* if the badge is zero then NBWait failed (see doNBRecvFailedTransfer)
     * and so it must haven't run up to seL4_Send. but we've waited at least
     * 2x as long as we needed for this remote helper to to have run if it
     * did have the budget. basically this is testing that the remote thread got
     * blocked due to not having enough budget. */
    test_eq(sender_badge, 0);

    /* give the helper on the remote core infinite budget, so it should run again */
    error = seL4_SchedControl_Configure(
        /* schedcontrol */ simple_get_sched_ctrl(&env->simple, 1),
        /* schedcontext */ get_helper_sched_context(&helper),
        /* budget */ MAX_PERIOD_US, /* period */ MAX_PERIOD_US, /* refills */ 0, /* badge */ 0);

    seL4_Wait(ep, NULL);

    cleanup_helper(env, &helper);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_SMP_004, "update a remote task's SC to have budget to run immediately.",
            test_update_remote_sc_with_budget, config_set(CONFIG_HAVE_TIMER) && config_set(CONFIG_KERNEL_MCS) && (CONFIG_MAX_NUM_NODES > 1));

void sched_context_smp_005_helper_fn(volatile int *state, seL4_CPtr ntfn, seL4_CPtr ep)
{
    *state = 1;
    /* wait on ntfn */
    seL4_Wait(ntfn, NULL);
    *state = 2;

    /* wait on ep */
    seL4_Wait(ep, NULL);
    *state = 3;

    /* perform a send so we generate a reply that server can use to wake us up */
    seL4_Call(ep, seL4_MessageInfo_new(0, 0, 0, 0));
    *state = 4;
}

void wait_eq_timeout(volatile int *state, int value, int timeout)
{
    int count = 0;
    while (true) {
        if (*state == value) {
            return;
        } else if (count >= timeout) {
            /* register the failure */
            test_lt(count, timeout);
            return;
        }

        count++;
    }
}

int test_blocking_remote_task_activation(env_t env)
{
    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr ntfn = vka_alloc_notification_leaky(&env->vka);
    seL4_CPtr reply = vka_alloc_reply_leaky(&env->vka);

    /* fine to use this cross core because of single-copy atomicity on an aligned (32-bit) variable */
    volatile int state = 0;

    /* make remote helper */
    helper_thread_t helper;
    create_helper_thread(env, &helper);
    set_helper_affinity(env, &helper, 1);
    start_helper(env, &helper, (helper_fn_t)sched_context_smp_005_helper_fn,
                 (seL4_Word)&state, ntfn, ep, 0);

    /* wait like a whole second (at 1GHz) to start */
    wait_eq_timeout(&state, 1, 1 * NS_IN_S);

    /* test notification waking blocked remote */
    seL4_Signal(ntfn);
    /* wait a bit for the next state */
    wait_eq_timeout(&state, 2, 1 * US_IN_S);

    /* test endpoint waking blocked remote */
    seL4_Send(ep, seL4_MessageInfo_new(0, 0, 0, 0));
    /* wait a bit for the next state */
    wait_eq_timeout(&state, 3, 1 * US_IN_S);

    (void)seL4_Recv(ep, NULL, reply);

    /* test reply waiting blocked remote */
    seL4_Send(reply, seL4_MessageInfo_new(0, 0, 0, 0));
    wait_eq_timeout(&state, 4, 1 * US_IN_S);

    cleanup_helper(env, &helper);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_SMP_005, "test activation of blocking remote tasks (call/reply/signal)",
            test_blocking_remote_task_activation, config_set(CONFIG_KERNEL_MCS) && (CONFIG_MAX_NUM_NODES > 1));

#define SC_SMP_006_EP   1
#define SC_SMP_006_NTFN 2

int sched_context_smp_006_helper_fn(seL4_CPtr ep, seL4_CPtr reply, seL4_CPtr ntfn, seL4_CPtr tcb)
{
    seL4_Word sender;

    /* This ReplyRecv will cause passive server-ification */
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    tag = seL4_ReplyRecv(ep, tag, &sender, reply);
    test_eq(sender, SC_SMP_006_EP);

    test_eq(0, seL4_DebugGetThreadAffinity(tcb));

    /* Now running on the SC of our `seL4_Call`er we signal our bound ntfn before
     * ReplyRecv'ing */
    seL4_Signal(ntfn);

    /* After this we will be running on the notification SC on a different core */
    tag = seL4_ReplyRecv(ep, tag, &sender, reply);
    test_eq(sender, SC_SMP_006_NTFN);

    test_eq(1, seL4_DebugGetThreadAffinity(tcb));

    return sel4test_get_result();
}

int test_passive_remote_task_signal_migration_bound(env_t env)
{
    int error;
    seL4_CPtr unbadged_ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr unbadged_ntfn = vka_alloc_notification_leaky(&env->vka);
    seL4_CPtr reply = vka_alloc_reply_leaky(&env->vka);
    /* badge_endpoint does work for a ntfn too */
    seL4_CPtr ntfn = badge_endpoint(env, SC_SMP_006_NTFN, unbadged_ntfn);
    seL4_CPtr ep = badge_endpoint(env, SC_SMP_006_EP, unbadged_ep);

    /* make helper */
    helper_thread_t helper;
    create_helper_thread(env, &helper);

    /* make the helper (and the bound ntfn sc) run on the second core */
    set_helper_affinity(env, &helper, 1);

    /* setup a bound notification */
    error = seL4_TCB_BindNotification(get_helper_tcb(&helper), ntfn);
    test_eq(error, seL4_NoError);

    /* do lazy rebind for passive */
    error = api_sc_bind(get_helper_sched_context(&helper), ntfn);
    test_eq(error, seL4_NoError);

    /* set helper to higher prio so it runs until blocked above us. */
    set_helper_priority(env, &helper, env->priority + 1);

    start_helper(env, &helper, (helper_fn_t)sched_context_smp_006_helper_fn,
                 ep, reply, ntfn, get_helper_tcb(&helper));

    /* Make it run on our core and then continue with migration via signal on self */
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    tag = seL4_Call(ep, tag);

    error = wait_for_helper(&helper);
    test_eq(error, seL4_NoError);

    cleanup_helper(env, &helper);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_SMP_006, "signal on bound notification causing core migration passive task (current task)",
            test_passive_remote_task_signal_migration_bound, config_set(CONFIG_KERNEL_MCS) && (CONFIG_MAX_NUM_NODES > 1));

int sched_context_smp_007_helper_fn(seL4_CPtr ep, seL4_CPtr reply, seL4_CPtr ntfn, seL4_CPtr tcb)
{
    seL4_Word sender;

    /* This ReplyRecv will cause passive server-ification */
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    tag = seL4_ReplyRecv(ep, tag, &sender, reply);
    test_eq(sender, SC_SMP_006_EP);

    test_eq(0, seL4_DebugGetThreadAffinity(tcb));

    seL4_Signal(ntfn);
    /* wait on the notification as an alternative to bound notification ep wait */
    seL4_Wait(ntfn, NULL);

    test_eq(1, seL4_DebugGetThreadAffinity(tcb));

    return sel4test_get_result();
}

int test_passive_remote_task_signal_migration_wait(env_t env)
{
    int error;
    seL4_CPtr unbadged_ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr unbadged_ntfn = vka_alloc_notification_leaky(&env->vka);
    seL4_CPtr reply = vka_alloc_reply_leaky(&env->vka);
    /* badge_endpoint does work for a ntfn too */
    seL4_CPtr ntfn = badge_endpoint(env, SC_SMP_006_NTFN, unbadged_ntfn);
    seL4_CPtr ep = badge_endpoint(env, SC_SMP_006_EP, unbadged_ep);

    /* make helper */
    helper_thread_t helper;
    create_helper_thread(env, &helper);

    /* make the helper (and the bound ntfn sc) run on the second core */
    set_helper_affinity(env, &helper, 1);

    /* setup a bound notification */
    error = seL4_TCB_BindNotification(get_helper_tcb(&helper), ntfn);
    test_eq(error, seL4_NoError);

    /* do lazy rebind for passive */
    error = api_sc_bind(get_helper_sched_context(&helper), ntfn);
    test_eq(error, seL4_NoError);

    /* set helper to higher prio so it runs until blocked above us. */
    set_helper_priority(env, &helper, env->priority + 1);

    start_helper(env, &helper, (helper_fn_t)sched_context_smp_007_helper_fn,
                 ep, reply, ntfn, get_helper_tcb(&helper));

    /* Make it run on our core and then continue with migration via signal on self */
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    tag = seL4_Call(ep, tag);

    error = wait_for_helper(&helper);
    test_eq(error, seL4_NoError);

    cleanup_helper(env, &helper);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_SMP_007, "signal causing (seL4_Wait) core migration current passive task",
            test_passive_remote_task_signal_migration_wait, config_set(CONFIG_KERNEL_MCS) && (CONFIG_MAX_NUM_NODES > 1));

int sched_context_smp_008_helper_fn(seL4_CPtr ep, seL4_CPtr reply, seL4_CPtr ntfn_to_helper, seL4_CPtr tcb)
{
    seL4_Word sender;

    /* Tell helper 2 we're ready and also do passive server-ification */
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    tag = seL4_NBSendRecv(ntfn_to_helper, tag, ep, NULL, reply);

    /* now running on core 2 (helper 2's core) */
    test_eq(2, seL4_DebugGetThreadAffinity(tcb));

    /* respond to helper 2 and wait for the notification from core 0 */
    tag = seL4_ReplyRecv(ep, tag, &sender, reply);
    test_eq(sender, SC_SMP_006_NTFN);

    test_eq(1, seL4_DebugGetThreadAffinity(tcb));

    return sel4test_get_result();
}

int sched_context_smp_008_core2_helper_fn(seL4_CPtr ep, seL4_CPtr ntfn_from_helper)
{
    /* wait for the other helper to be ready - so that when it does the
     * passive server Recv() it goes into a blocked state, instead of immediately
     * progressing as this helper already made the ep into a send state */
    seL4_Wait(ntfn_from_helper, NULL);

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);
    /* Call helper 1 and make it migrate to core 2 */
    (void)seL4_Call(ep, tag);
    /* die */
    return 0;
}

int test_passive_remote_task_migration(env_t env)
{
    int error;
    seL4_CPtr unbadged_ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr unbadged_ntfn = vka_alloc_notification_leaky(&env->vka);
    seL4_CPtr reply = vka_alloc_reply_leaky(&env->vka);
    /* badge_endpoint does work for a ntfn too */
    seL4_CPtr ntfn = badge_endpoint(env, SC_SMP_006_NTFN, unbadged_ntfn);
    seL4_CPtr ep = badge_endpoint(env, SC_SMP_006_EP, unbadged_ep);
    seL4_CPtr ntfn_between_helpers = vka_alloc_notification_leaky(&env->vka);

    /* make helper */
    helper_thread_t helper_1, helper_2;
    create_helper_thread(env, &helper_1);
    create_helper_thread(env, &helper_2);

    /* make the bound ntfn sc (and initial server setup) run on the second core */
    set_helper_affinity(env, &helper_1, 1);
    set_helper_affinity(env, &helper_2, 2);

    /* setup a bound notification */
    error = seL4_TCB_BindNotification(get_helper_tcb(&helper_1), ntfn);
    test_eq(error, seL4_NoError);

    /* do lazy rebind for passive */
    error = api_sc_bind(get_helper_sched_context(&helper_1), ntfn);
    test_eq(error, seL4_NoError);

    /**
     * Situation at start:
     *
     *  Core 0:              Core 1:                             Core 2
     *   test program      helper 1 (blocked on signal)       helper 2 (that calls 1)
     *
     * Helper 1/2 run; helper 2 blocks waiting for helper 1 to NBSendRecv.
     * This means helper 2 passive-server-ifies before helper 2 runs.
     * Helper 2 performs seL4_Call(helper 1). This causes helper 1 to run
     * on core 2. Then helper 1 rplies to helper 2, which tells core 0 to signal
     * helper 1 and cause a migration to core 1.
     *
     **/

    start_helper(env, &helper_1, (helper_fn_t)sched_context_smp_008_helper_fn,
                 ep, reply, ntfn_between_helpers, get_helper_tcb(&helper_1));
    start_helper(env, &helper_2, (helper_fn_t)sched_context_smp_008_core2_helper_fn,
                 ep, ntfn_between_helpers, 0, 0);

    /* wait for helper 2 to start and die */
    error = wait_for_helper(&helper_2);
    test_eq(error, seL4_NoError);

    seL4_Signal(ntfn);

    /* wait for helper to die */
    error = wait_for_helper(&helper_1);
    test_eq(error, seL4_NoError);

    cleanup_helper(env, &helper_1);
    cleanup_helper(env, &helper_2);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_SMP_008, "signal on bound notification remote task causing core migration (two remote cores)",
            test_passive_remote_task_migration, config_set(CONFIG_KERNEL_MCS) && (CONFIG_MAX_NUM_NODES > 2));

void sc_smp_009_helper_fn(seL4_CPtr ntfn)
{
    /* tell remote we are started now */
    seL4_Signal(ntfn);

    /* Infinitely loop taking up budget */
    while (true) {
        asm volatile("nop" ::: "memory");
    };
}

int test_update_remote_sc_with_no_budget(env_t env)
{

    /**
     * Idea behind this: a remote thread might have run out of a budget.
     * If we make a remote thread that has a tiny budget (the minimum)
     * but a very long period (the maximum) then once it will runout it will
     * basically never refill: so, if we reconfigure the SC to have full
     * bandwidth (budget = period) then it should start running again.
     **/

    int error;
    helper_thread_t helper;
    seL4_CPtr ntfn;
    seL4_CPtr timeout_ep;
    seL4_CPtr timeout_reply;
    seL4_MessageInfo_t tag;

    ntfn = vka_alloc_notification_leaky(&env->vka);
    timeout_ep = vka_alloc_endpoint_leaky(&env->vka);
    timeout_reply = vka_alloc_reply_leaky(&env->vka);

    create_helper_thread(env, &helper);

    /* give the helper infinite budget */
    error = seL4_SchedControl_Configure(
        /* schedcontrol */ simple_get_sched_ctrl(&env->simple, 1),
        /* schedcontext */ get_helper_sched_context(&helper),
        /* budget */ MAX_PERIOD_US, /* period */ MAX_PERIOD_US, /* refills */ 0, /* badge */ 0);
    ZF_LOGF_IF(error, "should be able to configure SC");

    /* configure a timeout EP so we know when it's budget expire s*/
    set_helper_tfep(env, &helper, timeout_ep);

    start_helper(env, &helper, (helper_fn_t)sc_smp_009_helper_fn, ntfn, 0, 0, 0);

    /* wait for it to tell us it is ready */
    seL4_Wait(ntfn, NULL);

    /* remove the remote helper's budget. it should stop counting immediately.
       specifically: since it has infinite budget if we don't make it stop rather
       quickly then it means it kept going. the idea is that if we don't stall
       the remote TCB it will continue to run and never need to drop back into
       the kernel as no timer interrupts are scheduled for over an hour. */
    error = seL4_SchedControl_Configure(
        /* schedcontrol */ simple_get_sched_ctrl(&env->simple, 1),
        /* schedcontext */ get_helper_sched_context(&helper),
        /* budget */ MIN_BUDGET_US, /* period */ MAX_PERIOD_US, /* refills */ 0, /* badge */ 0);
    test_error_eq(error, seL4_NoError);

    tag = seL4_Recv(timeout_ep, NULL, timeout_reply);
    seL4_Fault_t fault = seL4_getFault(tag);
    test_eq(seL4_Fault_get_seL4_FaultType(fault), seL4_Fault_Timeout);

    cleanup_helper(env, &helper);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_SMP_009, "update a remote task's SC to remove budget to run immediately.",
            test_update_remote_sc_with_no_budget, config_set(CONFIG_KERNEL_MCS) && (CONFIG_MAX_NUM_NODES > 1));
