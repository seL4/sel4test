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

#include <assert.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <vka/object.h>

#include "../helpers.h"

#define NUM_RUNS 10
#define ASYNC 1
#define SYNC 2

static seL4_CPtr
badge_endpoint(env_t env, seL4_Word badge, seL4_CPtr ep)
{

    seL4_CPtr slot = get_free_slot(env);
    int error = cnode_mint(env, ep, slot, seL4_AllRights, badge);
    test_assert(!error);
    return slot;
}

static int
sender(seL4_Word ep, seL4_Word id, seL4_Word runs, seL4_Word arg3)
{
    assert(runs > 0);
    for (seL4_Word i = 0; i < runs; i++) {
        seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);
        seL4_Send((seL4_CPtr) ep, info);
    }

    return 0;
}

static int
test_notification_binding(env_t env)
{
    helper_thread_t sync, notification;

    /* create endpoints */
    seL4_CPtr sync_ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr notification_ep = vka_alloc_notification_leaky(&env->vka);
    seL4_CPtr badged_notification_ep = badge_endpoint(env, ASYNC, notification_ep);
    seL4_CPtr badged_sync_ep = badge_endpoint(env, SYNC, sync_ep);

    assert(notification_ep);
    assert(sync_ep);

    /* badge endpoints so we can tell them apart */
    create_helper_thread(env, &sync);
    create_helper_thread(env, &notification);

    /* bind the endpoint */
    int error = seL4_TCB_BindNotification(env->tcb, notification_ep);
    test_assert(error == seL4_NoError);

    start_helper(env, &notification, sender, badged_notification_ep, ASYNC, NUM_RUNS, 0);
    start_helper(env, &sync, sender, badged_sync_ep, SYNC, NUM_RUNS, 0);

    int num_notification_messages = 0;
    int num_sync_messages = 0;
    for (int i = 0; i < NUM_RUNS * 2; i++) {
        seL4_Word badge = 0;
        seL4_Wait(sync_ep, &badge);

        switch (badge) {
        case ASYNC:
            num_notification_messages++;
            break;
        case SYNC:
            num_sync_messages++;
            break;
        }
    }

    test_check(num_notification_messages == NUM_RUNS);
    test_check(num_sync_messages == NUM_RUNS);

    error = seL4_TCB_UnbindNotification(env->tcb);
    test_assert(error == seL4_NoError);

    cleanup_helper(env, &sync);
    cleanup_helper(env, &notification);

    return sel4test_get_result();
}
DEFINE_TEST(BIND0001, "Test that a bound tcb waiting on a sync endpoint receives normal sync ipc and notification notifications.", test_notification_binding, true)

static int
test_notification_binding_2(env_t env)
{
    helper_thread_t notification;

    /* create endpoints */
    seL4_CPtr notification_ep = vka_alloc_notification_leaky(&env->vka);
    seL4_CPtr badged_notification_ep = badge_endpoint(env, ASYNC, notification_ep);

    test_assert(notification_ep);
    test_assert(badged_notification_ep);

    /* badge endpoints so we can tell them apart */
    create_helper_thread(env, &notification);

    /* bind the endpoint */
    int error = seL4_TCB_BindNotification(env->tcb, notification_ep);
    test_assert(error == seL4_NoError);

    start_helper(env, &notification, sender, badged_notification_ep, ASYNC, NUM_RUNS, 0);

    int num_notification_messages = 0;
    for (int i = 0; i < NUM_RUNS; i++) {
        seL4_Word badge = 0;
        seL4_Wait(notification_ep, &badge);

        switch (badge) {
        case ASYNC:
            num_notification_messages++;
            break;
        }
    }

    test_check(num_notification_messages == NUM_RUNS);

    error = seL4_TCB_UnbindNotification(env->tcb);
    test_assert(error == seL4_NoError);

    cleanup_helper(env, &notification);

    return sel4test_get_result();
}
DEFINE_TEST(BIND0002, "Test that a bound tcb waiting on its bound notification recieves notifications", test_notification_binding_2, true)

/* helper thread for testing the ordering of bound notification endpoint operations */
static int
waiter(seL4_Word bound_ep, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3)
{
    seL4_Word badge;
    seL4_Wait(bound_ep, &badge);
    return 0;
}

static int
test_notification_binding_prio(env_t env, uint8_t waiter_prio, uint8_t sender_prio)
{
    helper_thread_t waiter_thread;
    helper_thread_t sender_thread;

    seL4_CPtr notification_ep = vka_alloc_notification_leaky(&env->vka);
    seL4_CPtr sync_ep = vka_alloc_endpoint_leaky(&env->vka);

    test_assert(notification_ep);
    test_assert(sync_ep);

    create_helper_thread(env, &waiter_thread);
    set_helper_priority(env, &waiter_thread, waiter_prio);

    create_helper_thread(env, &sender_thread);
    set_helper_priority(env, &sender_thread, sender_prio);

    int error = seL4_TCB_BindNotification(get_helper_tcb(&waiter_thread), notification_ep);
    test_assert(error == seL4_NoError);

    start_helper(env, &waiter_thread, waiter, notification_ep, 0, 0, 0);
    start_helper(env, &sender_thread, sender, notification_ep, 0, 1, 0);

    wait_for_helper(&waiter_thread);
    wait_for_helper(&sender_thread);

    cleanup_helper(env, &waiter_thread);
    cleanup_helper(env, &sender_thread);

    return sel4test_get_result();
}

static int
test_notification_binding_3(env_t env)
{
    test_notification_binding_prio(env, 10, 9);
    return sel4test_get_result();
}
DEFINE_TEST(BIND0003, "Test IPC ordering 1) bound tcb waits on bound notification 2, true) another tcb sends a message",
            test_notification_binding_3, true)

static int
test_notification_binding_4(env_t env)
{
    test_notification_binding_prio(env, 9, 10);
    return sel4test_get_result();
}
DEFINE_TEST(BIND0004, "Test IPC ordering 2) bound tcb waits on bound notification 1, true) another tcb sends a message",
            test_notification_binding_4, true)

static void
bind0005_helper(seL4_CPtr endpoint, volatile int *state)
{
    *state = 1;
    seL4_Wait(endpoint, NULL);
    *state = 2;
}

static int
test_notification_binding_no_sc(env_t env)
{
    seL4_CPtr endpoint, notification;
    int error;
    helper_thread_t helper;
    volatile int state = 0;

    endpoint = vka_alloc_endpoint_leaky(&env->vka);
    notification = vka_alloc_notification_leaky(&env->vka);

    create_helper_thread(env, &helper);

    /* set our prio lower so the helper thread runs when we start it */
    set_helper_priority(env, &helper, 10);
    error = seL4_TCB_SetPriority(env->tcb, env->tcb, 9);
    test_eq(error, seL4_NoError);

    error = seL4_TCB_BindNotification(helper.thread.tcb.cptr, notification);
    test_eq(error, seL4_NoError);

    /* start the helper so it is waiting on the endpoint */
    start_helper(env, &helper, (helper_fn_t) bind0005_helper, endpoint,
                 (seL4_Word) &state, 0, 0);
    test_eq(state, 1);

    /* clear its sc */
    error = api_sc_unbind(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* signal it */
    seL4_Signal(notification);

    /* it should not have run */
    test_eq(state, 1);

    /* now give back the scheduling context */
    error = api_sc_bind(helper.thread.sched_context.cptr,
                              helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    /* now it should have got the signal */
    test_eq(state, 2);

    return sel4test_get_result();
}
DEFINE_TEST(BIND005, "Test passing thread notification binding with no scheduling context", test_notification_binding_no_sc, config_set(CONFIG_KERNEL_RT))

static int
test_notification_binding_with_sc(env_t env)
{
    seL4_CPtr endpoint, notification;
    int error;
    helper_thread_t helper;
    volatile int state = 0;

    endpoint = vka_alloc_endpoint_leaky(&env->vka);
    notification = vka_alloc_notification_leaky(&env->vka);

    create_helper_thread(env, &helper);

    /* set our prio lower so the helper thread runs when we start it */
    set_helper_priority(env, &helper, 10);
    error = seL4_TCB_SetPriority(env->tcb, env->tcb, 9);
    test_eq(error, seL4_NoError);

    error = seL4_TCB_BindNotification(helper.thread.tcb.cptr, notification);
    test_eq(error, seL4_NoError);

    /* start the helper so it is waiting on the endpoint */
    start_helper(env, &helper, (helper_fn_t) bind0005_helper, endpoint,
                 (seL4_Word) &state, 0, 0);
    test_eq(state, 1);

    /* clear its sc */
    error = api_sc_unbind(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = api_sc_bind(helper.thread.sched_context.cptr, notification);
    test_eq(error, seL4_NoError);

    /* signal it */
    seL4_Signal(notification);

    /* now it should have got the signal */
    test_eq(state, 2);

    return sel4test_get_result();
}
DEFINE_TEST(BIND006, "Test passing thread notification binding with a scheduling context", test_notification_binding_with_sc, config_set(CONFIG_KERNEL_RT))
