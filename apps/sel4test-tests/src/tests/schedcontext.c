/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <vka/object.h>

#include "../helpers.h"

int
test_sched_control_configure(env_t env)
{
    int error;
    seL4_SchedContext sc = vka_alloc_sched_context_leaky(&env->vka);
    test_neq(sc, seL4_CapNull);

    /* test it works */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 5000llu, 5000llu);
    test_eq(error, seL4_NoError);

    /* test calling it on something that isn't a sched context */
    seL4_CPtr tcb = vka_alloc_tcb_leaky(&env->vka);
    test_neq(tcb, seL4_CapNull);

    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), tcb, 5000llu, 5000llu);
    test_eq(error, seL4_InvalidCapability);

    /* test a 0 budget doesn't work */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 0llu, 5000llu);
    test_eq(error, seL4_InvalidArgument);

    /* test a period of 0 doesn't work */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 0llu, 0llu);
    test_eq(error, seL4_InvalidArgument);

    /* test budget > period doesn't work */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 5000llu, 1000llu);
    test_eq(error, seL4_InvalidArgument);

    return SUCCESS;

}
DEFINE_TEST(SCHED_CONTEXT_0001, "Test seL4_SchedControl_Configure", test_sched_control_configure)


static NORETURN int
sched_context_0002_fn(void)
{
    while(1);
}


int
test_sched_control_reconfigure(env_t env)
{
    helper_thread_t thread;
    int error;

    create_helper_thread(env, &thread);
    seL4_CPtr sc = thread.thread.sched_context.cptr;

    /* reconfigure a paused thread */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 5000llu, 5000llu);
    test_eq(error, seL4_NoError);
    
    /* now start the thread */
    start_helper(env, &thread, (helper_fn_t) sched_context_0002_fn, 0, 0, 0, 0);

    /* let it run a little */
    sleep(env, 10 * NS_IN_MS);
    
    /* reconfigure a resumed thread */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 10000llu, 10000llu);
    test_eq(error, seL4_NoError);
    
    /* let it run a little */
    sleep(env, 10 * NS_IN_MS);
   
    /* less */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 3000llu, 3000llu);
    test_eq(error, seL4_NoError);

    /* done! */
    return SUCCESS;
}
DEFINE_TEST(SCHED_CONTEXT_0002, "Test reconfiguring a thread", test_sched_control_reconfigure)


/* test bindTCB errors */
int
test_bindTCB_errors(env_t env)
{
    seL4_CPtr tcb = vka_alloc_tcb_leaky(&env->vka);
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr sched_context = vka_alloc_sched_context_leaky(&env->vka);

    /* not a tcb */
    int error = seL4_SchedContext_BindTCB(sched_context, endpoint);
    test_eq(error, seL4_InvalidCapability);

    error = seL4_SchedContext_BindTCB(sched_context, tcb);
    test_eq(error, seL4_NoError);

    /* tcb already bound */
    error = seL4_SchedContext_BindTCB(sched_context, tcb);
    test_eq(error, seL4_IllegalOperation);

    error = seL4_SchedContext_UnbindTCB(sched_context);
    test_eq(error, seL4_NoError);   

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0003, "Basic seL4_SchedContext_BindTCB testing", test_bindTCB_errors);

/* test bindNotification errors */
int
test_bindNotification_errors(env_t env) 
{
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr sched_context = vka_alloc_sched_context_leaky(&env->vka);
    seL4_CPtr notification = vka_alloc_notification_leaky(&env->vka);

    /* not a notification */
    int error = seL4_SchedContext_BindNotification(sched_context, endpoint);
    test_eq(error, seL4_InvalidCapability);

    error = seL4_SchedContext_BindNotification(sched_context, notification);
    test_eq(error, seL4_NoError);

    /* notification already bound */
    error = seL4_SchedContext_BindNotification(sched_context, notification);
    test_eq(error, seL4_IllegalOperation);

    error = seL4_SchedContext_UnbindNotification(sched_context);
    test_eq(error, seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0004, "Basic seL4_SchedContext_BindNotification testing", test_bindNotification_errors);

void
sched_context_0005_helper_fn(volatile int *state)
{
    while (1) {
        *state = *state + 1;
    }
}

/* test deleting scheduling context from bound tcb stops tcb */
int
test_delete_tcb_sched_context(env_t env)
{
    helper_thread_t helper;
    volatile int state = 0;

    create_helper_thread(env, &helper);
    start_helper(env, &helper, (helper_fn_t) sched_context_0005_helper_fn, (seL4_Word) &state, 0, 0, 0);
    
    /* let helper run */
    sleep(env, 10 * MS_IN_S);

    int prev_state = state;
    test_geq(state, 0);

    vka_free_object(&env->vka, &helper.thread.sched_context);

    /* let it run again */
    sleep(env, 10 * MS_IN_S);

    /* it should not have run */
    test_eq(prev_state, state);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0005, "Test deleting a scheduling context prevents the bound tcb from running", test_delete_tcb_sched_context);

void 
sched_context_0006_helper_fn(seL4_CPtr notification, int *state)
{
    *state = 1;
    seL4_Wait(notification, NULL);
    *state = 2;
    while(1);
    
}

/* test deleting tcb running on notification sched context returns it */
int
test_delete_tcb_on_notification_context(env_t env)
{
    helper_thread_t one, two;
    volatile int state_one;
    volatile int state_two;
    seL4_CPtr notification;
    int error;
    
    notification = vka_alloc_notification_leaky(&env->vka);

    create_helper_thread(env, &one);
    create_helper_thread(env, &two);

    /* set helpers to our prio so we can seL4_SchedContext_Yield to let them run */
    set_helper_priority(&one, env->priority);
    set_helper_priority(&two, env->priority);

    start_helper(env, &one, (helper_fn_t) sched_context_0006_helper_fn, notification,
                 (seL4_Word) &state_one, 0, 0);

    /* let the other thread run */
    error = seL4_SchedContext_Yield(env->sched_context);
    test_eq(error, seL4_NoError);
    test_eq(state_one, 1);

    /* take away its sc and assign to notification */
    error = seL4_SchedContext_UnbindTCB(one.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);
    error = seL4_SchedContext_BindNotification(one.thread.sched_context.cptr, notification);
    test_eq(error, seL4_NoError);

    /* let it run and receive the notifications scheduling context */
    seL4_Signal(notification);
    error = seL4_SchedContext_Yield(env->sched_context);
    test_eq(error, seL4_NoError);
    test_eq(state_one, 2);

    /* kill it */
    vka_free_object(&env->vka, &one.thread.tcb);
    error = seL4_SchedContext_Yield(env->sched_context);
    test_eq(error, seL4_NoError);

    /* now start the other thread */
    start_helper(env, &two, (helper_fn_t) sched_context_0006_helper_fn, notification,
                 (seL4_Word) &state_two, 0, 0);

    /* let it initialise */
    error = seL4_SchedContext_Yield(env->sched_context);
    test_eq(error, 0);
    test_eq(state_two, 1);

    /* take away its sc */
    vka_free_object(&env->vka, &two.thread.sched_context);

    /* signal the notification */
    seL4_Signal(notification);

    /* now let the other thread run - if the first thread gave the scheduling context back on deletion 
     * this should work */
    error = seL4_SchedContext_Yield(env->sched_context);
    test_eq(error, seL4_NoError);
    test_eq(state_two, 2);
    
    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0006, "Test deleting a tcb running on a notifications scheduling context returns it", test_delete_tcb_on_notification_context);



