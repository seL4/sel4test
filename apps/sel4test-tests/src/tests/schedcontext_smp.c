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

