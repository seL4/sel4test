/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#define ZF_LOG_LEVEL ZF_LOG_DEBUG
#include <assert.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <vka/object.h>
#include <sel4/messages.h>
#include "../helpers.h"

static int 
test_set_criticality(env_t env)
{
    int error;

    /* first set our criticality high so we get to run */
    error = seL4_TCB_SetCriticality(env->tcb, seL4_MaxCrit);
    test_eq(error, seL4_NoError);

    /* try to set it too high */
    error = seL4_SchedControl_SetCriticality(simple_get_sched_ctrl(&env->simple), seL4_MaxCrit + 1);
    test_eq(error, seL4_RangeError);

    /* set to lowest */
    error = seL4_SchedControl_SetCriticality(simple_get_sched_ctrl(&env->simple), seL4_MinCrit);
    test_eq(error, seL4_NoError);

    /* set to highest */
    error = seL4_SchedControl_SetCriticality(simple_get_sched_ctrl(&env->simple), seL4_MaxCrit);
    test_eq(error, seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(CRITICALITY0000, "Test set criticality", test_set_criticality)

static int
test_max_control_criticality(env_t env)
{
    int error;

    /* need at least 2 criticalities for this test to work */
    test_geq(seL4_MaxCrit, seL4_MinCrit + 1);

    /* set criticality to a known value (this test assumes we have mcc == seL4_MaxCrit) */
    error = seL4_TCB_SetCriticality(env->tcb, seL4_MinCrit);
    test_eq(error, seL4_NoError);
 
    /* now set our criticality up */
    error = seL4_TCB_SetCriticality(env->tcb, seL4_MaxCrit);
    test_eq(error, seL4_NoError);

    /* now set it back down */
    error = seL4_TCB_SetCriticality(env->tcb, seL4_MinCrit);
    test_eq(error, seL4_NoError);
    
    /* set our mcc down */
    error = seL4_TCB_SetMCCriticality(env->tcb, seL4_MinCrit);
    test_eq(error, seL4_NoError);

    /* set our criticality higher than the just set mcc */
    error = seL4_TCB_SetCriticality(env->tcb, seL4_MinCrit + 1);
    test_neq(error, seL4_NoError);

    /* set criticality higher than mcc */
    error = seL4_TCB_SetMCCriticality(env->tcb, seL4_MinCrit + 1);
    test_neq(error, seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(CRITICALITY0001, "Test maximum control criticality", test_max_control_criticality)

/* copy from a volatile src */
static void 
vmemcpy(uint64_t *dst, volatile uint64_t *src, uint32_t n)
{
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
        ZF_LOGD("%llu --> %llu\n", src[i], dst[i]);
    }
}

static void
crit0002_helper_fn(volatile uint64_t *state, pstimer_t *timer, seL4_CPtr sched_context)
{
    while (true) {
        uint64_t start = timer_get_time(timer);
        uint64_t now = start;
        /* spin for a bit */
        while ((now - start) < (100 * NS_IN_MS)) {
            now = timer_get_time(timer);
        }
        *state = *state + 1llu;
        assert(*state < UINT64_MAX);
        seL4_SchedContext_Yield(sched_context);
    }
}

static int 
test_criticality_mode_switch(env_t env)
{
    /* Create a set of HI threads and a set of LO threads. Create one HI thread with a 
     * very small budget such that it will overrun quickly. Make sure the mode 
     * switch happens.
     */
    seL4_CPtr tfep;
    int error;
    uint8_t prios[] = {1, 7, 5, 3, 2, 6, 4};
    uint8_t crits[] = {1, 1, 1, 1, 0, 0, 0};
    int num_threads = ARRAY_SIZE(prios);
    int special = 3;

    test_eq(num_threads, ARRAY_SIZE(crits));
    /* need at least 2 criticalities */
    test_geq(seL4_MaxCrit, seL4_MinCrit + 1);

    volatile uint64_t states[num_threads];
    helper_thread_t threads[num_threads];

    tfep = vka_alloc_endpoint_leaky(&env->vka);

    error = seL4_TCB_SetCriticality(env->tcb, seL4_MinCrit + 1);
    test_eq(error, 0);

    for (int i = 0; i < num_threads; i++) {
        states[i] = 0llu;
        create_helper_thread(env, &threads[i]);
        set_helper_priority(&threads[i], prios[i]);
        set_helper_criticality(&threads[i], crits[i]);
        seL4_CPtr minted_tfep = get_free_slot(env);
        error = cnode_mint(env, tfep, minted_tfep, seL4_AllRights, seL4_CapData_Badge_new(i));
        test_eq(error, seL4_NoError);

        if (i == special) {
            set_helper_sched_params(env, &threads[i], 1 * US_IN_MS, 1000 * US_IN_MS, i);
        } else {
            set_helper_sched_params(env, &threads[i], 500 * US_IN_MS, 1000 * US_IN_MS, i);
        }

        set_helper_tfep(env, &threads[i], minted_tfep);
        start_helper(env, &threads[i], (helper_fn_t) crit0002_helper_fn,
                     (seL4_Word) &states[i],
                     (seL4_Word) env->clock_timer->timer, 
                     threads[i].thread.sched_context.cptr, 0);
    }

    ZF_LOGD("Waiting for temporal fault\n");
    seL4_Word badge;
    seL4_Recv(tfep, &badge);
    test_eq(badge, special);

    ZF_LOGD("Got it, mode switching");
    error = set_helper_sched_params(env, &threads[special], 500 * US_IN_MS, 1000 * US_IN_MS, special);
    test_eq(error, seL4_NoError);

    error = seL4_SchedControl_SetCriticality(simple_get_sched_ctrl(&env->simple), seL4_MinCrit + 1);
    test_eq(error, seL4_NoError);
   
    /* save states */
    uint64_t prev_states[num_threads];
    vmemcpy(prev_states, states, num_threads);

    ZF_LOGD("Replying to faulting thread");
    /* reply to faulting thread */
    seL4_Reply(seL4_MessageInfo_new(0, 0, 0, 0));

    ZF_LOGV("Sleep");
    /* sleep for a while */
    sleep(env, 2 * NS_IN_S);

    ZF_LOGV("Checking states thread");
    /* check only HI criticality thread states have changed */
    for (int i = 0; i < num_threads; i++) {
        printf("%d: %llu --> %llu\n", i, states[i], prev_states[i]);
        if (i <= special) {
            test_geq(states[i], prev_states[i]);
        } else {
            test_eq(states[i], prev_states[i]);
        }
    }

    /* save states again */
    vmemcpy(prev_states, states, num_threads);

    /* Set the criticality back down */
    ZF_LOGV("Set criticality back down");
    error = seL4_SchedControl_SetCriticality(simple_get_sched_ctrl(&env->simple), seL4_MinCrit);
    test_eq(error, seL4_NoError);

    /* wait again */
    ZF_LOGV("Sleep");
    sleep(env, 2 * NS_IN_S);

    /* now check all the states have changed */
    for (int i = 0; i < num_threads; i++) {
        test_geq(states[i], prev_states[i]);
    }

    return sel4test_get_result();
}
DEFINE_TEST(CRITICALITY0002, "Test criticality mode switch", test_criticality_mode_switch)

static void 
crit0003_server_fn(seL4_CPtr endpoint, pstimer_t *timer)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 1);
    
    /* signal we are initialised and wait on endpoint */
    seL4_NBSendRecv(endpoint, info, endpoint, NULL);
    while (1) {
        ZF_LOGD("Server spinning");
        uint64_t start = timer_get_time(timer);
        uint64_t end = start;
        while (end - start < (2 * NS_IN_S)) {
            end = timer_get_time(timer);
        }
        ZF_LOGD("Server reply");
        seL4_ReplyRecv(endpoint, info, NULL);
    }
}

static void
crit0003_client_fn(seL4_CPtr ep) {
    ZF_LOGD("Client call\n");
    seL4_Call(ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

static int
test_criticality_mode_switch_in_server(env_t env)
{
    int num_clients = 3;
    helper_thread_t clients[num_clients];
    helper_thread_t server;
    seL4_CPtr tfep, ep;
    sel4utils_checkpoint_t cp;
    int error;

    tfep = vka_alloc_endpoint_leaky(&env->vka);
    ep = vka_alloc_endpoint_leaky(&env->vka);

    /* set our criticality up */
    error = seL4_TCB_SetCriticality(env->tcb, seL4_MaxCrit);
    test_eq(error, seL4_NoError);

    /* set up and start server */
    ZF_LOGD("Create server\n");
    error = create_passive_thread_with_tfep(env, &server, tfep, 0, (helper_fn_t) crit0003_server_fn, 
                                    ep, (seL4_Word) env->clock_timer->timer, 0, 0, &cp);
    test_eq(error, 0);

    set_helper_criticality(&server, seL4_MinCrit + 1);
    ZF_LOGD("Server at prio %d\n", env->priority - 1);
    set_helper_priority(&server, env->priority - 1);

    /* set up clients */
    ZF_LOGD("Create clients");
    for (int i = 0; i < num_clients; i++) {
        create_helper_thread(env, &clients[i]);
        ZF_LOGD("Client %d at prio %d\n", i, env->priority - 2 - i);
        set_helper_priority(&clients[i], env->priority - 2 - i);
        set_helper_sched_params(env, &clients[i], 5 * US_IN_S, 5 * US_IN_S, i);
        if (i > 0) {
            /* make some clients HI criticality */
            set_helper_criticality(&clients[i], seL4_MinCrit + 1);
        }
        start_helper(env, &clients[i], (helper_fn_t) crit0003_client_fn, ep, 0, 0, 0);
    }

    /* sleep for a while */
    sleep(env, NS_IN_S);
    
    /* set criticality down */
    error = seL4_SchedControl_SetCriticality(simple_get_sched_ctrl(&env->simple), seL4_MinCrit + 1);
    test_eq(error, seL4_NoError);

    /* wait for temporal fault  - we should recieve a temporal fault from client 0, as it is LO 
     * criticality and made a request to the server. */

    ZF_LOGD("Wait for temporal fault");
    seL4_MessageInfo_t info = seL4_Recv(tfep, NULL);
    test_check(seL4_isTemporalFault_Tag(info));
    test_eq(seL4_TF_DataWord(), 0);

    ZF_LOGD("Reply to client");
    /* reply to the client on behalf of the server */
    seL4_CPtr reply = get_free_slot(env);
    error = cnode_saveTCBcaller(env, reply, &server.thread.tcb);
    test_eq(error, seL4_NoError);
    seL4_Signal(reply);

    ZF_LOGD("Restore server");
    /* restore the server */
    error = sel4utils_checkpoint_restore(&cp, true, true);
    test_eq(error, 0);
    
    error = seL4_SchedContext_BindTCB(server.thread.sched_context.cptr, server.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    /* wait for server to get back on the ep */
    seL4_Recv(ep, NULL);

    /* convert back to passive */
    error = seL4_SchedContext_UnbindTCB(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    ZF_LOGD("let hi clients run");
    /* let the high threads runn */
    for (int i = 1; i < num_clients; i++) {
        wait_for_helper(&clients[i]);
    }

    /* reset the criticality */
    error = seL4_SchedControl_SetCriticality(simple_get_sched_ctrl(&env->simple), seL4_MinCrit);
    test_eq(error, seL4_NoError);

    ZF_LOGD("Wait for lo thread");
    /* now the lo crit client should finish */
    wait_for_helper(&clients[0]);

    return sel4test_get_result();
}
DEFINE_TEST(CRITICALITY0003, "Test criticality mode change in server", test_criticality_mode_switch_in_server)



