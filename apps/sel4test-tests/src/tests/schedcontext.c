/*
 *  Copyright 2016, Data61
 *  Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 *  ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
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
    vka_object_t sched_context;

    error = vka_alloc_sched_context(&env->vka, &sched_context);
    test_eq(error, 0);

    seL4_CPtr sc = sched_context.cptr;
    test_neq(sc, seL4_CapNull);

    /* test it works */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, 0), sc, 5000llu, 5000llu);
    test_eq(error, seL4_NoError);

    /* test calling it on something that isn't a sched context */
    seL4_CPtr tcb = vka_alloc_tcb_leaky(&env->vka);
    test_neq(tcb, seL4_CapNull);

    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, 0), tcb, 5000llu, 5000llu);
    test_eq(error, seL4_InvalidCapability);

    /* test a 0 budget doesn't work */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, 0), sc, 0llu, 5000llu);
    test_eq(error, seL4_RangeError);

    /* test a period of 0 doesn't work */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, 0), sc, 0llu, 0llu);
    test_eq(error, seL4_RangeError);

    /* test budget > period doesn't work */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, 0), sc, 5000llu, 1000llu);
    test_eq(error, seL4_RangeError);

    return sel4test_get_result();

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
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, 0), sc, 5000llu, 5000llu);
    test_eq(error, seL4_NoError);

    /* now start the thread */
    start_helper(env, &thread, (helper_fn_t) sched_context_0002_fn, 0, 0, 0, 0);

    /* let it run a little */
    sleep(env, 10 * NS_IN_MS);

    /* reconfigure a resumed thread */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, 0), sc, 10000llu, 10000llu);
    test_eq(error, seL4_NoError);

    /* let it run a little */
    sleep(env, 10 * NS_IN_MS);

    /* less */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, 0), sc, 3000llu, 3000llu);
    test_eq(error, seL4_NoError);

    /* done! */
    return SUCCESS;
}
DEFINE_TEST(SCHED_CONTEXT_0002, "Test reconfiguring a thread", test_sched_control_reconfigure)


/* test bindTCB errors */
int
test_bind_errors(env_t env)
{
    seL4_CPtr tcb = vka_alloc_tcb_leaky(&env->vka);
    seL4_CPtr sched_context = vka_alloc_sched_context_leaky(&env->vka);
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);

    /* binding an object that is not a tcb or ntfn should fail */
    int error = seL4_SchedContext_Bind(sched_context, endpoint);
    test_eq(error, seL4_InvalidCapability);

    error = seL4_SchedContext_Bind(sched_context, tcb);
    test_eq(error, seL4_NoError);

    /* so should trying to bind a schedcontext that is already bound to tcb */
    error = seL4_SchedContext_Bind(sched_context, tcb);
    test_eq(error, seL4_IllegalOperation);

    /* check unbinding an object that is not a tcb fails */
    error = seL4_SchedContext_UnbindObject(sched_context, endpoint);
    test_eq(error, seL4_InvalidCapability);

    /* check trying to unbinding the tcb */
    error = seL4_SchedContext_UnbindObject(sched_context, tcb);
    test_eq(error, seL4_NoError);

    /* check trying to unbind a valid object that is not bounnd fails */
    error = seL4_SchedContext_UnbindObject(sched_context, tcb);
    test_eq(error, seL4_InvalidCapability);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0003, "Basic seL4_SchedContext_Bind/UnbindObject testing", test_bind_errors);

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
    sleep(env, 1 * NS_IN_S);

    printf("Sleep....\n");
    int prev_state = state;
    test_geq(state, 0);
    printf("Awake\n");

    vka_free_object(&env->vka, &helper.thread.sched_context);

    /* let it run again */
    printf("Sleep....\n");
    sleep(env, 1 * NS_IN_S);
    printf("Awake\n");

    /* it should not have run */
    test_eq(prev_state, state);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0005, "Test deleting a scheduling context prevents the bound tcb from running", test_delete_tcb_sched_context);

