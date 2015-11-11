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
test_sched_control_configure(env_t env, void *args) 
{
    int error;
    seL4_SchedContext sc = vka_alloc_sched_context_leaky(&env->vka);
    test_neq(sc, seL4_CapNull);

    /* test it works */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 5llu);
    test_eq(error, seL4_NoError);

    /* test calling it on something that isn't a sched context */   
    seL4_CPtr tcb = vka_alloc_tcb_leaky(&env->vka);
    test_neq(tcb, seL4_CapNull);

    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), tcb, 5llu);
    test_eq(error, seL4_InvalidCapability);

    /* test a 0 budget doesn't work */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 0llu);
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
test_sched_control_reconfigure(env_t env, void *arg)
{
    helper_thread_t thread;
    int error;

    create_helper_thread(env, &thread);
    seL4_CPtr sc = thread.thread.sched_context.cptr;

    /* reconfigure a paused thread */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 5llu);
    test_eq(error, seL4_NoError);
    
    /* now start the thread */
    start_helper(env, &thread, (helper_fn_t) sched_context_0002_fn, 0, 0, 0, 0);

    /* let it run a little */
    sleep(env, 10 * NS_IN_MS);
    
    /* reconfigure a resumed thread */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 10llu);
    test_eq(error, seL4_NoError);
    
    /* let it run a little */
    sleep(env, 10 * NS_IN_MS);
   
    /* less */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 3llu);
    test_eq(error, seL4_NoError);

    /* done! */
    return SUCCESS;
}
DEFINE_TEST(SCHED_CONTEXT_0002, "Test reconfiguring a thread", test_sched_control_reconfigure)

