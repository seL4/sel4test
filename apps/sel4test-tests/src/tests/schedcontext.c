/*
 *  Copyright 2017, Data61
 *  Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 *  ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#include <autoconf.h>
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
    test_neq(sc, (seL4_Word)seL4_CapNull);

    /* test it works */
    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0), sc, 5000llu, 5000llu, 0, 0);
    test_eq(error, seL4_NoError);

    /* test calling it on something that isn't a sched context */
    seL4_CPtr tcb = vka_alloc_tcb_leaky(&env->vka);
    test_neq(tcb, (seL4_Word)seL4_CapNull);

    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0), tcb, 5000llu, 5000llu, 0, 0);
    test_eq(error, seL4_InvalidCapability);

    /* test a 0 budget doesn't work */
    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0), sc, 0llu, 5000llu, 0, 0);
    test_eq(error, seL4_RangeError);

    /* test a period of 0 doesn't work */
    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0), sc, 0llu, 0llu, 0, 0);
    test_eq(error, seL4_RangeError);

    /* test budget > period doesn't work */
    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0), sc, 5000llu, 1000llu, 0, 0);
    test_eq(error, seL4_RangeError);

    //TODO test refills cases

    return sel4test_get_result();

}
DEFINE_TEST(SCHED_CONTEXT_0001, "Test api_sched_ctrl_configure", test_sched_control_configure, config_set(CONFIG_KERNEL_RT))


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
    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0), sc, 5000llu, 5000llu, 0, 0);
    test_eq(error, seL4_NoError);

    /* now start the thread */
    start_helper(env, &thread, (helper_fn_t) sched_context_0002_fn, 0, 0, 0, 0);

    /* let it run a little */
    sel4test_sleep(env, 10 * NS_IN_MS);

    /* reconfigure a resumed thread */
    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0), sc, 10000llu, 10000llu, 0, 0);
    test_eq(error, seL4_NoError);

    /* let it run a little */
    sel4test_sleep(env, 10 * NS_IN_MS);

    /* less */
    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0), sc, 3000llu, 3000llu, 0, 0);
    test_eq(error, seL4_NoError);

    /* done! */
    return SUCCESS;
}
DEFINE_TEST(SCHED_CONTEXT_0002, "Test reconfiguring a thread", test_sched_control_reconfigure, config_set(CONFIG_KERNEL_RT) && config_set(CONFIG_HAVE_TIMER))

int
test_bind_errors(env_t env)
{
    seL4_CPtr tcb = vka_alloc_tcb_leaky(&env->vka);
    seL4_CPtr sched_context = vka_alloc_sched_context_leaky(&env->vka);
    seL4_CPtr notification = vka_alloc_notification_leaky(&env->vka);
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);

    /* binding an object that is not a tcb or ntfn should fail */
    int error = api_sc_bind(sched_context, endpoint);
    test_eq(error, seL4_InvalidCapability);

    error = api_sc_bind(sched_context, tcb);
    test_eq(error, seL4_NoError);

    /* so should trying to bind a schedcontext that is already bound to tcb */
    error = api_sc_bind(sched_context, tcb);
    test_eq(error, seL4_IllegalOperation);

    /* similarly we cannot bind a notification if a tcb is bound */
    error = api_sc_bind(sched_context, notification);
    test_eq(error, seL4_IllegalOperation);

    error = api_sc_unbind_object(sched_context, tcb);
    test_eq(error, seL4_NoError);

    error = api_sc_bind(sched_context, notification);
    test_eq(error, seL4_NoError);

    /* and you can't bind a notification if a notification is already bound*/
    error = api_sc_bind(sched_context, notification);
    test_eq(error, seL4_IllegalOperation);

    /* and you can't bind a tcb if a notification is already bound */
    error = api_sc_bind(sched_context, tcb);
    test_eq(error, seL4_IllegalOperation);

    error = api_sc_unbind_object(sched_context, notification);
    test_eq(error, seL4_NoError);

    /* check unbinding an object that is not a tcb or notification fails */
    error = api_sc_unbind_object(sched_context, endpoint);
    test_eq(error, seL4_InvalidCapability);

    /* check trying to unbind a valid object that is not bounnd fails */
    error = api_sc_unbind_object(sched_context, notification);
    test_eq(error, seL4_IllegalOperation);

    error = api_sc_unbind_object(sched_context, tcb);
    test_eq(error, seL4_IllegalOperation);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0003, "Basic api_sc_bind/UnbindObject testing", test_bind_errors, config_set(CONFIG_KERNEL_RT));

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
    sel4test_sleep(env, 1 * NS_IN_S);

    printf("Sleep....\n");
    int prev_state = state;
    test_geq(state, 0);
    printf("Awake\n");

    vka_free_object(&env->vka, &helper.thread.sched_context);

    /* let it run again */
    printf("Sleep....\n");
    sel4test_sleep(env, 1 * NS_IN_S);
    printf("Awake\n");

    /* it should not have run */
    test_eq(prev_state, state);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0005, "Test deleting a scheduling context prevents the bound tcb from running", test_delete_tcb_sched_context, config_set(CONFIG_KERNEL_RT) && config_set(CONFIG_HAVE_TIMER));

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

    seL4_CPtr notification = vka_alloc_notification_leaky(&env->vka);

    create_helper_thread(env, &one);
    create_helper_thread(env, &two);

    /* set helpers to our prio so we can seL4_Yield to let them run */
    set_helper_priority(env, &one, env->priority);
    set_helper_priority(env, &two, env->priority);

    start_helper(env, &one, (helper_fn_t) sched_context_0006_helper_fn, notification,
                 (seL4_Word) &state_one, 0, 0);

    /* let the other thread run */
    seL4_Yield();
    test_eq(state_one, 1);

    /* take away its sc and assign to notification */
    int error = api_sc_unbind(one.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);
    error = api_sc_bind(one.thread.sched_context.cptr, notification);
    test_eq(error, seL4_NoError);

    /* let it run and receive the notifications scheduling context */
    seL4_Signal(notification);
    seL4_Yield();
    test_eq(error, seL4_NoError);
    test_eq(state_one, 2);
   /* kill it */
    vka_free_object(&env->vka, &one.thread.tcb);
    seL4_Yield();
    test_eq(error, seL4_NoError);

    /* now start the other thread */
    start_helper(env, &two, (helper_fn_t) sched_context_0006_helper_fn, notification,
                 (seL4_Word) &state_two, 0, 0);

    /* let it initialise */
    seL4_Yield();
    test_eq(error, 0);
    test_eq(state_two, 1);

    /* take away its sc */
    vka_free_object(&env->vka, &two.thread.sched_context);

    /* signal the notification */
    seL4_Signal(notification);

   /* signal the notification */
    seL4_Signal(notification);

    /* now let the other thread run - if the first thread gave the scheduling context back on deletion
     * this should work */
    seL4_Yield();
    test_eq(error, seL4_NoError);
    test_eq(state_two, 2);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0006, "Test deleting a tcb running on a notifications scheduling context returns it", test_delete_tcb_on_notification_context, config_set(CONFIG_KERNEL_RT));

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
    error = api_sc_unbind(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* resume then bind */
    start_helper(env, &helper, (helper_fn_t) sched_context_007_helper_fn, 0, 0, 0, 0);

    error = api_sc_bind(helper.thread.sched_context.cptr, helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    error = wait_for_helper(&helper);
    test_eq(error, 1);

    /* bind then resume */
    create_helper_thread(env, &helper);

    error = api_sc_unbind(helper.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = api_sc_bind(helper.thread.sched_context.cptr, helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    start_helper(env, &helper, (helper_fn_t) sched_context_007_helper_fn, 0, 0, 0, 0);

    error = seL4_TCB_Resume(helper.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    error = wait_for_helper(&helper);
    test_eq(error, 1);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT0007, "test resuming a passive thread and binding scheduling context",
            test_passive_thread_start, config_set(CONFIG_KERNEL_RT))

static void
sched_context0008_client_fn(seL4_CPtr send_ep, seL4_CPtr wait_ep)
{
    ZF_LOGD("Client send\n");
    api_nbsend_wait(send_ep, seL4_MessageInfo_new(0, 0, 0, 0), wait_ep, NULL);
}

static void
sched_context0008_proxy_fn(seL4_CPtr send_ep, seL4_CPtr wait_ep)
{
    /* signal to test runner that we are initialised and waiting for client */
    ZF_LOGD("Proxy init\n");
    api_nbsend_wait(send_ep, seL4_MessageInfo_new(0, 0, 0, 0), wait_ep, NULL);

    /* forward on the message we got */
    ZF_LOGD("Proxy fwd\n");
    api_nbsend_wait(send_ep, seL4_MessageInfo_new(0, 0, 0, 0), wait_ep, NULL);

    ZF_LOGF("Should not get here");
}

static void
sched_context0008_server_fn(seL4_CPtr init_ep, seL4_CPtr wait_ep)
{
    ZF_LOGD("Server init\n");
    /* tell test runner we are done by sending to init ep, then wait for proxy message */
    api_nbsend_wait(init_ep, seL4_MessageInfo_new(0, 0, 0, 0), wait_ep, NULL);
    ZF_LOGD("Server exit\n");
    /* hold on to scheduling context */
}
static int
test_delete_sendwait_tcb(env_t env)
{
    helper_thread_t client, proxy, server;
    seL4_CPtr client_send = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr client_wait = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr proxy_send = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr server_ep = vka_alloc_endpoint_leaky(&env->vka);

    /* set up and start server */
    ZF_LOGD("Create server\n");
    int error = create_passive_thread(env, &server, (helper_fn_t) sched_context0008_server_fn,
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
    /* wait for the server to finish, who has stolen the scheduling context */
    wait_for_helper(&server);

    ZF_LOGD("Kill server\n");
    /* kill the server */
    vka_free_object(&env->vka, &server.thread.tcb);

    /* give client back its sc */
    error = api_sc_bind(client.thread.sched_context.cptr, client.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    ZF_LOGD("Signal client\n");
    seL4_Signal(client_wait);

    ZF_LOGD("Wait for client\n");
    /* now the client should finish */
    wait_for_helper(&client);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0008, "Test deleting a tcb running on donated sc",
            test_delete_sendwait_tcb, config_set(CONFIG_KERNEL_RT))

void
sched_context_0009_server_fn(seL4_CPtr ep, volatile int *state, seL4_CPtr reply)
{
    ZF_LOGD("Server init\n");
    api_nbsend_recv(ep, seL4_MessageInfo_new(0, 0, 0, 0), ep, NULL, reply);
    while (1) {
        *state = *state + 1;
    }
}

void
sched_context_0009_client_fn(seL4_CPtr ep)
{
    ZF_LOGD("Client call\n");
    seL4_Call(ep, seL4_MessageInfo_new(0, 0, 0, 0));
}

int
test_sched_context_goes_to_to_caller_on_reply_cap_delete(env_t env)
{
    helper_thread_t client, server;
    seL4_CPtr ep, reply;
    volatile int state = 0;
    int prev_state = state;
    int error;

    ep = vka_alloc_endpoint_leaky(&env->vka);
    test_neq(ep, (seL4_CPtr) seL4_CapNull);

    reply = vka_alloc_reply_leaky(&env->vka);
    test_neq(reply, (seL4_CPtr) seL4_CapNull);

    /* create server */
    create_passive_thread(env, &server, (helper_fn_t) sched_context_0009_server_fn, ep,
                          (seL4_Word) &state, reply, 0);

    /* create client */
    create_helper_thread(env, &client);

    /* client calls blocking server */
    start_helper(env, &client, (helper_fn_t) sched_context_0009_client_fn, ep, 0, 0, 0);

    /* wait a bit, client should have called server */
    sel4test_sleep(env, 0.2 * NS_IN_S);
    test_ge(state, prev_state);
    prev_state = state;

    /* delete reply cap */
    error = cnode_delete(env, reply);
    test_eq(error, seL4_NoError);

    /* wait a bit, check server not running anymore */
    sel4test_sleep(env, 0.2 * NS_IN_S);
    test_eq(state, prev_state);

    /* save and resume client */
    restart_after_syscall(env, &client);

    printf("Waiting for client\n");
    wait_for_helper(&client);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0009, "Test scheduling context goes to caller if reply cap deleted", test_sched_context_goes_to_to_caller_on_reply_cap_delete, config_set(CONFIG_KERNEL_RT) && config_set(CONFIG_HAVE_TIMER))

void
sched_context_0010_client_fn(seL4_CPtr ep)
{
    api_nbsend_wait(ep, seL4_MessageInfo_new(0, 0, 0, 0), ep, NULL);
}

int
test_sched_context_unbind_server(env_t env)
{
    helper_thread_t client, server;
    volatile int state = 0;
    int prev_state = state;

    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);
    test_neq(ep, (seL4_CPtr) seL4_CapNull);

    seL4_CPtr reply = vka_alloc_reply_leaky(&env->vka);
    test_neq(reply, (seL4_CPtr) seL4_CapNull);

    /* create server */
    create_passive_thread(env, &server, (helper_fn_t) sched_context_0009_server_fn, ep,
                          (seL4_Word) &state, reply, 0);

    /* create client */
    create_helper_thread(env, &client);

    /* client calls blocking server */
    start_helper(env, &client, (helper_fn_t) sched_context_0010_client_fn, ep, 0, 0, 0);

    /* wait a bit, client should have called server */
    sel4test_sleep(env, 0.2 * NS_IN_S);
    test_ge(state, prev_state);
    prev_state = state;

    /* unbind scheduling context */
    int error = api_sc_unbind_object(client.thread.sched_context.cptr, server.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    /* wait a bit, check server not running anymore */
    sel4test_sleep(env, 0.2 * NS_IN_S);
    test_eq(state, prev_state);

    /* rebind the scheduling context to the client */
    error = api_sc_bind(client.thread.sched_context.cptr, client.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    /* save and resume client */
    restart_after_syscall(env, &client);

    printf("Waiting for client\n");
    wait_for_helper(&client);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0010, "Test unbinding scheduling context from server", test_sched_context_unbind_server, config_set(CONFIG_KERNEL_RT) && config_set(CONFIG_KERNEL_RT))

void
sched_context_0011_proxy_fn(seL4_CPtr in, seL4_CPtr out, seL4_CPtr reply)
{
    ZF_LOGD("Proxy init\n");
    api_nbsend_recv(in, seL4_MessageInfo_new(0, 0, 0, 0), in, NULL, reply);

    ZF_LOGD("Proxy call\n");
    seL4_Call(out, seL4_MessageInfo_new(0, 0, 0, 0));

    ZF_LOGD("Proxy here\n");
}

int
test_revoke_reply_on_call_chain_returns_sc(env_t env)
{
    helper_thread_t client, proxy, server;
    volatile int state = 0;
    int error;

    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr ep2 = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr proxy_reply = vka_alloc_reply_leaky(&env->vka);
    seL4_CPtr server_reply = vka_alloc_reply_leaky(&env->vka);

    create_passive_thread(env, &server, (helper_fn_t) sched_context_0009_server_fn, ep2,
                          (seL4_Word) &state, server_reply, 0);

    create_passive_thread(env, &proxy, (helper_fn_t) sched_context_0011_proxy_fn, ep, ep2, proxy_reply, 0);

    create_helper_thread(env, &client);
    start_helper(env, &client, (helper_fn_t) sched_context_0009_client_fn, ep, 0, 0, 0);

    /* let a call b which calls the server, let the server run a bit */
    sel4test_sleep(env, 0.2 * NS_IN_S);
    test_ge(state, 0);

    /* kill the servers reply cap */
    error = cnode_delete(env, server_reply);
    test_eq(error, seL4_NoError);

    /* check server stopped running */
    int prev_state = state;
    sel4test_sleep(env, 0.2 * NS_IN_S);
    test_eq(prev_state, state);

    /* kill the proxies reply cap */
    error = cnode_delete(env, proxy_reply);
    test_eq(error, seL4_NoError);

    /* save and resume client */
    restart_after_syscall(env, &client);

    ZF_LOGD("Waiting for client\n");
    wait_for_helper(&client);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0011, "Test revoking a reply on a call chain returns scheduling context along chain", test_revoke_reply_on_call_chain_returns_sc, config_set(CONFIG_KERNEL_RT) && config_set(CONFIG_KERNEL_RT))

/* sched 0011 but unordered */
int
test_revoke_reply_on_call_chain_unordered(env_t env)
{
    helper_thread_t client, proxy, server;
    volatile int state = 0;
    int error;

    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr ep2 = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr proxy_reply = vka_alloc_reply_leaky(&env->vka);
    seL4_CPtr server_reply = vka_alloc_reply_leaky(&env->vka);

    create_passive_thread(env, &server, (helper_fn_t) sched_context_0009_server_fn, ep2,
                          (seL4_Word) &state, server_reply, 0);

    create_passive_thread(env, &proxy, (helper_fn_t) sched_context_0011_proxy_fn, ep, ep2, proxy_reply, 0);

    create_helper_thread(env, &client);
    start_helper(env, &client, (helper_fn_t) sched_context_0009_client_fn, ep, 0, 0, 0);

    /* let a call b which calls the server, let the server run a bit */
    sel4test_sleep(env, 0.2 * NS_IN_S);
    test_ge(state, 0);

    /* kill the proxies reply cap */
    ZF_LOGD("Nuke proxy reply cap");
    error = cnode_delete(env, proxy_reply);
    test_eq(error, seL4_NoError);

    /* kill the servers reply cap */
    ZF_LOGD("Nuke server reply cap\n");
    error = cnode_delete(env, server_reply);
    test_eq(error, seL4_NoError);

    /* check server is not still running */
    int prev_state = state;
    sel4test_sleep(env, 0.2 * NS_IN_S);
    test_eq(state, prev_state);

    /* save and resume client */
    restart_after_syscall(env, &client);

    /* check the client got its scheduling context back and is now running */
    ZF_LOGD("Waiting for client\n");
    wait_for_helper(&client);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0012, "Test revoking a reply on a call chain unorderd", test_revoke_reply_on_call_chain_unordered, config_set(CONFIG_KERNEL_RT) && config_set(CONFIG_KENEL_RT))

int
test_revoke_sched_context_on_call_chain(env_t env)
{
    helper_thread_t client, proxy, server;
    volatile int state = 0;
    int error;

    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr ep2 = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr proxy_reply = vka_alloc_reply_leaky(&env->vka);
    seL4_CPtr server_reply = vka_alloc_reply_leaky(&env->vka);

    create_passive_thread(env, &server, (helper_fn_t) sched_context_0009_server_fn, ep2,
                          (seL4_Word) &state, server_reply, 0);

    create_passive_thread(env, &proxy, (helper_fn_t) sched_context_0011_proxy_fn, ep, ep2, proxy_reply, 0);

    create_helper_thread(env, &client);
    start_helper(env, &client, (helper_fn_t) sched_context_0009_client_fn, ep, 0, 0, 0);

    /* let client call proxy which calls the server, let the server run a bit */
    sel4test_sleep(env, 0.2 * NS_IN_S);
    test_ge(state, 0);

    /* nuke the scheduling context */
    vka_free_object(&env->vka, &client.thread.sched_context);

    /* check server stopped running */
    int prev_state = state;
    sel4test_sleep(env, 0.2 * NS_IN_S);
    test_eq(prev_state, state);

    /* nuke the reply cap */
    error = cnode_delete(env, server_reply);
    test_eq(error, seL4_NoError);

    /* give the proxy a scheduling context */
    seL4_CPtr sched_context = vka_alloc_sched_context_leaky(&env->vka);
    test_neq(sched_context, (seL4_CPtr) seL4_CapNull);

    error = api_sc_bind(sched_context, proxy.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0), sched_context,
                                        5 * US_IN_S, 5 * US_IN_S, 0, 0);
    test_eq(error, seL4_NoError);

    restart_after_syscall(env, &proxy);

    ZF_LOGD("Waiting for proxy\n");
    wait_for_helper(&proxy);

    error = api_sc_unbind_object(sched_context, proxy.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    /* give the client a scheduling context */
    error = api_sc_bind(sched_context, client.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    restart_after_syscall(env, &client);

    ZF_LOGD("Waiting for Client\n");
    wait_for_helper(&client);

    return sel4test_get_result();
}
DEFINE_TEST(SCHED_CONTEXT_0013, "Test revoking a scheduling context on a call chain", test_revoke_sched_context_on_call_chain, config_set(CONFIG_KERNEL_RT) && config_set(CONFIG_KERNEL_RT))
