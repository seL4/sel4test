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

#include <sel4/sel4.h>
#include <vka/object.h>

#include "../helpers.h"

#include <utils/util.h>

static void
interrupt_helper(env_t env, volatile int *state, int runs, seL4_CPtr endpoint)
{
    while (*state < runs) {
       *state = *state + 1;
       ZF_LOGD("Tick");
       sel4test_ntfn_timer_wait(env);
   }
   ZF_LOGD("Boom");
   sel4test_ntfn_timer_wait(env);

}

/* test an interrupt handling thread that inherits the scheduling context of the notification
 * object */
static int
test_interrupt_notification_sc(env_t env)
{
    helper_thread_t helper;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    volatile seL4_Word state = 0;
    seL4_Word runs = 10;
    int error;

    /* set up helper */
    create_helper_thread(env, &helper);
    start_helper(env, &helper, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state,
                 runs, endpoint);
    set_helper_priority(env, &helper, 10);
    error = seL4_TCB_SetPriority(env->tcb, env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* helper should not have finished */
    test_leq(state, runs);

    /* take away scheduling context and give it to notification object */
    error = api_sc_unbind(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = api_sc_bind(helper.thread.sched_context.cptr,
                                               env->timer_notification.cptr);
    test_eq(error, seL4_NoError);

    sel4test_periodic_start(env, 10 * NS_IN_MS);

    /* wait for the helper */
    wait_for_helper(&helper);
    test_eq(state, runs);

    sel4test_timer_reset(env);

    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0002, "Test interrupts with scheduling context donation from notification object", test_interrupt_notification_sc, config_set(CONFIG_HAVE_TIMER) && config_set(CONFIG_KERNEL_RT));

/* test an interrupt handling thread with a scheduling context doesn't inherit the notification objects scheduling context */
static int
test_interrupt_notification_and_tcb_sc(env_t env)
{
    helper_thread_t helper_with_sc, helper_without_sc;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    volatile seL4_Word state_with_sc = 0;
    volatile seL4_Word state_without_sc = 0;
    seL4_Word runs = 10;
    int error;

    /* set up helpers */
    create_helper_thread(env, &helper_without_sc);
    start_helper(env, &helper_without_sc, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state_without_sc,
                 runs, endpoint);
    set_helper_priority(env, &helper_without_sc, 10);


    create_helper_thread(env, &helper_with_sc);
    start_helper(env, &helper_with_sc, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state_with_sc,
                 runs, endpoint);
    set_helper_priority(env, &helper_with_sc, 10);

    /* helper_with_sc will run first */
    error = seL4_TCB_SetPriority(env->tcb, env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* both helpers should run and wait for irq */
    test_leq(state_with_sc, (seL4_Word) runs);
    test_leq(state_without_sc, (seL4_Word) runs);

    /* take away scheduling context from helper_without_sc and give it to notification object */
    error = api_sc_unbind(helper_without_sc.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    sel4test_periodic_start(env, 10 * NS_IN_MS);

    error = api_sc_bind(helper_without_sc.thread.sched_context.cptr,
                                   env->timer_notification.cptr);
    test_eq(error, seL4_NoError);

    /* wait for the helper */
    wait_for_helper(&helper_with_sc);
    test_eq(state_with_sc, runs);

    wait_for_helper(&helper_without_sc);
    test_eq(state_without_sc, runs);

    sel4test_timer_reset(env);

    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0003, "Test interrupts with scheduling context donation from notification object and without (two clients)", test_interrupt_notification_and_tcb_sc, config_set(CONFIG_HAVE_TIMER) && config_set(CONFIG_KERNEL_RT));

/* test that if niether the thread or notification object have a scheduling context, nothing happens */
static int
test_interrupt_no_sc(env_t env)
{
    helper_thread_t helper;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    volatile seL4_Word state = 0;
    seL4_Word runs = 10;
    int error;

    /* set up helper */
    create_helper_thread(env, &helper);
    start_helper(env, &helper, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state,
                 runs, endpoint);
    set_helper_priority(env, &helper, 10);
    error = seL4_TCB_SetPriority(env->tcb, env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* helper should run and wait for irq */
    test_leq(state, (seL4_Word) runs);
    seL4_Word prev_state = state;

    /* take away scheduling context */
    error = api_sc_unbind(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    sel4test_periodic_start(env, 10 * NS_IN_MS);

    test_eq(state, (seL4_Word) prev_state);

    sel4test_timer_reset(env);

    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0004, "Test interrupts with no scheduling context at all", test_interrupt_no_sc, config_set(CONFIG_HAVE_TIMER) && config_set(CONFIG_KERNEL_RT));

/* test that a second interrupt handling thread on the same endpoint works */
int
test_interrupt_notification_sc_two_clients(env_t env)
{
    helper_thread_t helper_first, helper_second;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    volatile seL4_Word state_first = 0;
    volatile seL4_Word state_second = 0;
    seL4_Word runs = 10;
    int error;

    /* set up helpers */
    create_helper_thread(env, &helper_second);
    start_helper(env, &helper_second, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state_first,
                 runs, endpoint);
    set_helper_priority(env, &helper_second, 10);

    create_helper_thread(env, &helper_first);
    start_helper(env, &helper_first, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state_second,
                 runs, endpoint);
    set_helper_priority(env, &helper_first, 10);

    /* helper_with_sc will run first */
    error = seL4_TCB_SetPriority(env->tcb, env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* both helpers should run and wait for irq */
    test_leq(state_first, (seL4_Word) runs);
    test_leq(state_second, (seL4_Word) runs);

    /* take away scheduling context from both, give one to a notification object */
    error = api_sc_unbind(helper_first.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = api_sc_unbind(helper_second.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = api_sc_bind(helper_first.thread.sched_context.cptr,
                                   env->timer_notification.cptr);
    test_eq(error, seL4_NoError);

    sel4test_periodic_start(env, 10 * NS_IN_MS);

    /* wait for the helper */
    wait_for_helper(&helper_first);
    /* second will not exit as first stole the scheduling context when it exited */

    test_eq(state_first, runs);
    test_eq(state_second, runs);

    sel4test_timer_reset(env);

    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0005, "Test the same scheduling context cannot be loaned to different threads",
            test_interrupt_notification_sc_two_clients, config_set(CONFIG_HAVE_TIMER) && config_set(CONFIG_KERNEL_RT));

/* test deleting the scheduling context stops the notification from donating it */
static int
test_interrupt_delete_sc(env_t env)
{
    helper_thread_t helper;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    volatile seL4_Word state = 0;
    seL4_Word runs = 10;
    int error;

    /* set up helper */
    create_helper_thread(env, &helper);
    start_helper(env, &helper, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state,
                 runs, endpoint);
    set_helper_priority(env, &helper, 10);
    error = seL4_TCB_SetPriority(env->tcb, env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* helper should run and wait for irq */
    test_leq(state, (seL4_Word) runs);
    seL4_Word prev_state = state;

    /* take away scheduling context and give it to notification object */
    error = api_sc_unbind(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = api_sc_bind(helper.thread.sched_context.cptr,
                                               env->timer_notification.cptr);
    test_eq(error, seL4_NoError);

    /* now delete it */
    vka_free_object(&env->vka, &helper.thread.sched_context);

    sel4test_periodic_start(env, 10 * NS_IN_MS);

    test_eq(state, prev_state);

    sel4test_timer_reset(env);
    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0006, "Test interrupts after deleting scheduling context bound to notification", test_interrupt_delete_sc, config_set(CONFIG_HAVE_TIMER) && config_set(CONFIG_KERNEL_RT));
