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

#include "../helpers.h"

int
test_sched_control_configure(env_t env)
{
    int error;
    seL4_SchedContext sc = vka_alloc_sched_context_leaky(&env->vka);
    test_neq(sc, seL4_CapNull);

    /* test it works */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 5000llu, 5000llu, 0);
    test_eq(error, seL4_NoError);

    /* test calling it on something that isn't a sched context */
    seL4_CPtr tcb = vka_alloc_tcb_leaky(&env->vka);
    test_neq(tcb, seL4_CapNull);

    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), tcb, 5000llu, 5000llu, 0);
    test_eq(error, seL4_InvalidCapability);

    /* test a 0 budget doesn't work */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 0llu, 5000llu, 0);
    test_eq(error, seL4_InvalidArgument);

    /* test a period of 0 doesn't work */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 0llu, 0llu, 0);
    test_eq(error, seL4_InvalidArgument);

    /* test budget > period doesn't work */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 5000llu, 1000llu, 0);
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
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 5000llu, 5000llu, 0);
    test_eq(error, seL4_NoError);
    
    /* now start the thread */
    start_helper(env, &thread, (helper_fn_t) sched_context_0002_fn, 0, 0, 0, 0);

    /* let it run a little */
    sleep(env, 10 * NS_IN_MS);
    
    /* reconfigure a resumed thread */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 10000llu, 10000llu, 0);
    test_eq(error, seL4_NoError);
    
    /* let it run a little */
    sleep(env, 10 * NS_IN_MS);
   
    /* less */
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sc, 3000llu, 3000llu, 0);
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

int
sched_context_007_helper_fn(void)
{
    return 1;
}

int
test_passive_thread_start(env_t env)
{
    helper_thread_t helper;
    int error;

    ZF_LOGD("z");
    create_helper_thread(env, &helper);
 
    ZF_LOGD("z");
    error = seL4_SchedContext_UnbindTCB(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* resume then bind */
    start_helper(env, &helper, (helper_fn_t) sched_context_007_helper_fn, 0, 0, 0, 0);
    
    error = seL4_SchedContext_BindTCB(helper.thread.sched_context.cptr, helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);
   
    error = wait_for_helper(&helper);
    test_eq(error, 1);

    /* bind then resume */
    create_helper_thread(env, &helper);
    
    error = seL4_SchedContext_UnbindTCB(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);
    
    error = seL4_SchedContext_BindTCB(helper.thread.sched_context.cptr, helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);
    
    start_helper(env, &helper, (helper_fn_t) sched_context_007_helper_fn, 0, 0, 0, 0);
    
    error = seL4_TCB_Resume(helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);
    
    error = wait_for_helper(&helper);
    test_eq(error, 1);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT0007, "test resuming a passive thread and binding scheduling context", 
            test_passive_thread_start)


static void
sched_context0008_client_fn(seL4_CPtr send_ep, seL4_CPtr wait_ep)
{
    ZF_LOGD("Client send\n");
    seL4_NBSendRecv(send_ep, seL4_MessageInfo_new(0, 0, 0, 0), wait_ep, NULL);
}

static void
sched_context0008_proxy_fn(seL4_CPtr send_ep, seL4_CPtr wait_ep)
{
    /* signal to test runner that we are initialised and waiting for client */
    ZF_LOGD("Proxy init\n");
    seL4_NBSendRecv(send_ep, seL4_MessageInfo_new(0, 0, 0, 0), wait_ep, NULL);

    /* forward on the message we got */
    ZF_LOGD("Proxy fwd\n");
    seL4_NBSendRecv(send_ep, seL4_MessageInfo_new(0, 0, 0, 0), wait_ep, NULL);

    ZF_LOGF("Should not get here");
}

static void
sched_context0008_server_fn(seL4_CPtr init_ep, seL4_CPtr wait_ep)
{
    ZF_LOGD("Server init\n");
    /* tell test runner we are done by sending to init ep, then wait for proxy message */
    seL4_NBSendRecv(init_ep, seL4_MessageInfo_new(0, 0, 0, 0), wait_ep, NULL);
    ZF_LOGD("Server exit\n");
    /* hold on to scheduling context */
}

static int 
test_delete_sendwait_tcb(env_t env)
{
    helper_thread_t client, proxy, server;
    seL4_CPtr client_send, client_wait, proxy_send, server_ep;
    int error;
    
    client_send = vka_alloc_endpoint_leaky(&env->vka);
    client_wait = vka_alloc_endpoint_leaky(&env->vka);
    proxy_send = vka_alloc_endpoint_leaky(&env->vka);
    server_ep = vka_alloc_endpoint_leaky(&env->vka);

    /* set up and start server */
    ZF_LOGD("Create server\n");
    error = create_passive_thread(env, &server, (helper_fn_t) sched_context0008_server_fn, 
                                  server_ep, proxy_send, 0, 0);
    test_eq(error, 0);

    /* setup and start proxy */
    ZF_LOGD("Create proxy\n");
    error = create_passive_thread(env, &proxy, (helper_fn_t) sched_context0008_proxy_fn, proxy_send, 
                                  client_send, 0, 0);
    test_eq(error, 0);

    ZF_LOGD("Create client\n");
    /* create and start the client */
    create_helper_thread(env, &client);
    start_helper(env, &client, (helper_fn_t) sched_context0008_client_fn, client_send, 
                 client_wait, 0, 0);
    
    ZF_LOGD("Wait for server\n");
    /* wait for the server to finish, who was stolen the scheduling context */
    wait_for_helper(&server);

    ZF_LOGD("Kill server\n");
    /* kill the server */
    vka_free_object(&env->vka, &server.thread.tcb);

    ZF_LOGD("Signal client\n");
    seL4_Signal(client_wait);

    ZF_LOGD("Wait for client\n");
    /* now the client should finish */
    wait_for_helper(&client);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0008, "Test deleting a tcb sends donated scheduling context home", 
            test_delete_sendwait_tcb)

