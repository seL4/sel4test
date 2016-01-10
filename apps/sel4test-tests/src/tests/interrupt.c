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

    int error = timer_periodic(env->timer->timer, 10 * NS_IN_MS);
    timer_start(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);
    test_check(error == 0);

    for (int i = 0; i < 3; i++) {
        wait_for_timer_interrupt(env);
    }

    timer_stop(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);

    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0001, "Test interrupts with timer", test_interrupt);


static void
interrupt_helper(env_t env, volatile int *state, int runs, seL4_CPtr endpoint) 
{
    while (*state < runs) {
       *state = *state + 1;
       ZF_LOGD("Tick");
       wait_for_timer_interrupt(env);
   }
   ZF_LOGD("Boom");
   wait_for_timer_interrupt(env);

}

/* test an interrupt handling thread that inherits the scheduling context of the notification 
 * object */
static int 
test_interrupt_notification_sc(env_t env)
{
    helper_thread_t helper;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    volatile int state = 0;
    seL4_Word runs = 10;
    int error;

    /* set up helper */
    create_helper_thread(env, &helper);
    start_helper(env, &helper, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state, 
                 runs, endpoint);
    set_helper_priority(&helper, 10);
    error = seL4_TCB_SetPriority(env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* helper should run and wait for irq */
    test_eq(state, 1);

    /* take away scheduling context and give it to notification object */
    error = seL4_SchedContext_UnbindTCB(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = seL4_SchedContext_BindNotification(helper.thread.sched_context.cptr, 
                                               env->timer_notification.cptr);
    test_eq(error, seL4_NoError);

    error = timer_periodic(env->timer->timer, 10 * NS_IN_MS);
    timer_start(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);
    test_check(error == 0);

    /* wait for the helper */
    wait_for_helper(&helper);
    test_eq(state, runs);

    /* turn off the timer */
    timer_stop(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);

    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0002, "Test interrupts with scheduling context donation from notification object", test_interrupt_notification_sc);

/* test an interrupt handling thread with a scheduling context doesn't inherit the notification objects scheduling context */
static int 
test_interrupt_notification_and_tcb_sc(env_t env)
{
    helper_thread_t helper_with_sc, helper_without_sc;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    volatile int state_with_sc = 0;
    volatile int state_without_sc = 0;
    seL4_Word runs = 10;
    int error;

    /* set up helpers */
    create_helper_thread(env, &helper_without_sc);
    start_helper(env, &helper_without_sc, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state_without_sc, 
                 runs, endpoint);
    set_helper_priority(&helper_without_sc, 10);
       

    create_helper_thread(env, &helper_with_sc);
    start_helper(env, &helper_with_sc, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state_with_sc, 
                 runs, endpoint);
    set_helper_priority(&helper_with_sc, 10);

    /* helper_with_sc will run first */
    error = seL4_TCB_SetPriority(env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* both helpers should run and wait for irq */
    test_eq(state_with_sc, 1);
    test_eq(state_without_sc, 1);

    /* take away scheduling context from helper_without_sc and give it to notification object */
    error = seL4_SchedContext_UnbindTCB(helper_without_sc.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = seL4_SchedContext_BindNotification(helper_without_sc.thread.sched_context.cptr, 
                                               env->timer_notification.cptr);
    test_eq(error, seL4_NoError);

    error = timer_periodic(env->timer->timer, 10 * NS_IN_MS);
    timer_start(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);
    test_check(error == 0);

    /* wait for the helper */
    wait_for_helper(&helper_with_sc);
    test_eq(state_with_sc, runs);

    wait_for_helper(&helper_without_sc);
    test_eq(state_without_sc, runs);

    /* turn off the timer */
    timer_stop(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);
    
    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0003, "Test interrupts with scheduling context donation from notification object and without (two clients)", test_interrupt_notification_and_tcb_sc);

/* test that if niether the thread or notification object have a scheduling context, nothing happens */
static int 
test_interrupt_no_sc(env_t env)
{
    helper_thread_t helper;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    volatile int state = 0;
    seL4_Word runs = 10;
    int error;

    /* set up helper */
    create_helper_thread(env, &helper);
    start_helper(env, &helper, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state, 
                 runs, endpoint);
    set_helper_priority(&helper, 10);
    error = seL4_TCB_SetPriority(env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* helper should run and wait for irq */
    test_eq(state, 1);

    /* take away scheduling context and give it to notification object */
    error = seL4_SchedContext_UnbindTCB(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = timer_periodic(env->timer->timer, 10 * NS_IN_MS);
    timer_start(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);
    test_check(error == 0);

    test_eq(state, 1);

    /* turn off the timer */
    timer_stop(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);

    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0004, "Test interrupts with no scheduling context at all", test_interrupt_no_sc);

/* test that a second interrupt handling thread on the same endpoint works */
int
test_interrupt_notification_sc_two_clients(env_t env)
{
    helper_thread_t helper_first, helper_second;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    volatile int state_first = 0;
    volatile int state_second = 0;
    seL4_Word runs = 10;
    int error;

    /* set up helpers */
    create_helper_thread(env, &helper_second);
    start_helper(env, &helper_second, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state_first, 
                 runs, endpoint);
    set_helper_priority(&helper_second, 10);
       
    create_helper_thread(env, &helper_first);
    start_helper(env, &helper_first, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state_second, 
                 runs, endpoint);
    set_helper_priority(&helper_first, 10);

    /* helper_with_sc will run first */
    error = seL4_TCB_SetPriority(env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* both helpers should run and wait for irq */
    test_eq(state_first, 1);
    test_eq(state_second, 1);

    /* take away scheduling context from both, give one to a notification object */
    error = seL4_SchedContext_UnbindTCB(helper_first.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = seL4_SchedContext_UnbindTCB(helper_second.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);
    
    error = seL4_SchedContext_BindNotification(helper_first.thread.sched_context.cptr, 
                                               env->timer_notification.cptr);
    test_eq(error, seL4_NoError);

    error = timer_periodic(env->timer->timer, 10 * NS_IN_MS);
    timer_start(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);
    test_check(error == 0);

    /* wait for the helper */
    wait_for_helper(&helper_first);
    /* second will not exit as first stole the scheduling context when it exited */

    test_eq(state_first, runs);
    test_eq(state_second, runs);

    /* turn off the timer */
    timer_stop(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);
    
    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0005, "Test the same scheduling context cannot be loaned to different threads",
            test_interrupt_notification_sc_two_clients);

/* test deleting the scheduling context stops the notification from donating it */
static int 
test_interrupt_delete_sc(env_t env)
{
    helper_thread_t helper;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    volatile int state = 0;
    seL4_Word runs = 10;
    int error;

    /* set up helper */
    create_helper_thread(env, &helper);
    start_helper(env, &helper, (helper_fn_t) interrupt_helper, (seL4_Word) env, (seL4_Word) &state, 
                 runs, endpoint);
    set_helper_priority(&helper, 10);
    error = seL4_TCB_SetPriority(env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* helper should run and wait for irq */
    test_eq(state, 1);

    /* take away scheduling context and give it to notification object */
    error = seL4_SchedContext_UnbindTCB(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = seL4_SchedContext_BindNotification(helper.thread.sched_context.cptr, 
                                               env->timer_notification.cptr);
    test_eq(error, seL4_NoError);

    /* now delete it */
    vka_free_object(&env->vka, &helper.thread.sched_context);

    error = timer_periodic(env->timer->timer, 10 * NS_IN_MS);
    timer_start(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);
    test_check(error == 0);

    test_eq(state, 1);

    /* turn off the timer */
    timer_stop(env->timer->timer);
    sel4_timer_handle_single_irq(env->timer);

    return sel4test_get_result();
}
DEFINE_TEST(INTERRUPT0006, "Test interrupts after deleting scheduling context bound to notification", test_interrupt_delete_sc);

#endif /* CONFIG_HAVE_TIMER */
