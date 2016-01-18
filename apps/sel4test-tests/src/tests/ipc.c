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
#include <stdlib.h>
#include <sel4/sel4.h>
#include <vka/object.h>
#include <vka/capops.h>
#include "../helpers.h"

#define MIN_LENGTH 0
#define MAX_LENGTH (seL4_MsgMaxLength + 1)

#define FOR_EACH_LENGTH(len_var) \
    for(int len_var = MIN_LENGTH; len_var <= MAX_LENGTH; len_var++)

typedef int (*test_func_t)(seL4_Word /* endpoint */, seL4_Word /* seed */, seL4_Word /* extra */);

static int
send_func(seL4_Word endpoint, seL4_Word seed, seL4_Word arg2)
{
    FOR_EACH_LENGTH(length) {
        seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, length);
        for (int i = 0; i < length; i++) {
            seL4_SetMR(i, seed);
            seed++;
        }
        seL4_Send(endpoint, tag);
    }

    return sel4test_get_result();
}

static int
nbsend_func(seL4_Word endpoint, seL4_Word seed, seL4_Word arg2)
{
    FOR_EACH_LENGTH(length) {
        seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, length);
        for (int i = 0; i < length; i++) {
            seL4_SetMR(i, seed);
            seed++;
        }
        seL4_NBSend(endpoint, tag);
    }

    return sel4test_get_result();
}

static int
call_func(seL4_Word endpoint, seL4_Word seed, seL4_Word arg2)
{
    FOR_EACH_LENGTH(length) {
        seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, length);

        /* Construct a message. */
        for (int i = 0; i < length; i++) {
            seL4_SetMR(i, seed);
            seed++;
        }

        tag = seL4_Call(endpoint, tag);

        seL4_Word actual_len = length;
        /* Sanity check the received message. */
        if (actual_len <= seL4_MsgMaxLength) {
            test_assert(seL4_MessageInfo_get_length(tag) == actual_len);
        } else {
            actual_len = seL4_MsgMaxLength;
        }

        for (int i = 0; i < actual_len; i++) {
            seL4_Word mr = seL4_GetMR(i);
            test_check(mr == seed);
            seed++;
        }
    }

    return sel4test_get_result();
}

static int
wait_func(seL4_Word endpoint, seL4_Word seed, seL4_Word arg2)
{
    FOR_EACH_LENGTH(length) {
        seL4_MessageInfo_t tag;
        seL4_Word sender_badge = 0;

        tag = seL4_Recv(endpoint, &sender_badge);
        seL4_Word actual_len = length;
        if (actual_len <= seL4_MsgMaxLength) {
            test_assert(seL4_MessageInfo_get_length(tag) == actual_len);
        } else {
            actual_len = seL4_MsgMaxLength;
        }

        for (int i = 0; i < actual_len; i++) {
            seL4_Word mr = seL4_GetMR(i);
            test_check(mr == seed);
            seed++;
        }
    }

    return sel4test_get_result();
}

static int
nbwait_func(seL4_Word endpoint, seL4_Word seed, seL4_Word nbwait_should_wait)
{
    if (!nbwait_should_wait) {
        return sel4test_get_result();
    }

    FOR_EACH_LENGTH(length) {
        seL4_MessageInfo_t tag;
        seL4_Word sender_badge = 0;

        tag = seL4_Recv(endpoint, &sender_badge);
        seL4_Word actual_len = length;
        if (actual_len <= seL4_MsgMaxLength) {
            test_assert(seL4_MessageInfo_get_length(tag) == actual_len);
        } else {
            actual_len = seL4_MsgMaxLength;
        }

        for (int i = 0; i < actual_len; i++) {
            seL4_Word mr = seL4_GetMR(i);
            test_check(mr == seed);
            seed++;
        }
    }

    return sel4test_get_result();
}

static int
replywait_func(seL4_Word endpoint, seL4_Word seed, seL4_Word arg2)
{
    int first = 1;

    seL4_MessageInfo_t tag;
    FOR_EACH_LENGTH(length) {
        seL4_Word sender_badge = 0;

        /* First reply/wait can't reply. */
        if (first) {
            tag = seL4_Recv(endpoint, &sender_badge);
            first = 0;
        } else {
            tag = seL4_ReplyRecv(endpoint, tag, &sender_badge);
        }

        seL4_Word actual_len = length;
        /* Sanity check the received message. */
        if (actual_len <= seL4_MsgMaxLength) {
            test_assert(seL4_MessageInfo_get_length(tag) == actual_len);
        } else {
            actual_len = seL4_MsgMaxLength;
        }

        for (int i = 0; i < actual_len; i++) {
            seL4_Word mr = seL4_GetMR(i);
            test_check(mr == seed);
            seed++;
        }
        /* Seed will have changed more if the message was truncated. */
        for (int i = actual_len; i < length; i++) {
            seed++;
        }

        /* Construct a reply. */
        for (int i = 0; i < actual_len; i++) {
            seL4_SetMR(i, seed);
            seed++;
        }
    }

    /* Need to do one last reply to match call. */
    seL4_Reply(tag);

    return sel4test_get_result();
}

static int
reply_and_wait_func(seL4_Word endpoint, seL4_Word seed, seL4_Word arg2)
{
    int first = 1;

    seL4_MessageInfo_t tag;
    FOR_EACH_LENGTH(length) {
        seL4_Word sender_badge = 0;

        /* First reply/wait can't reply. */
        if (!first) {
            seL4_Reply(tag);
        } else {
            first = 0;
        }

        tag = seL4_Recv(endpoint, &sender_badge);

        seL4_Word actual_len = length;
        /* Sanity check the received message. */
        if (actual_len <= seL4_MsgMaxLength) {
            test_assert(seL4_MessageInfo_get_length(tag) == actual_len);
        } else {
            actual_len = seL4_MsgMaxLength;
        }

        for (int i = 0; i < actual_len; i++) {
            seL4_Word mr = seL4_GetMR(i);
            test_check(mr == seed);
            seed++;
        }
        /* Seed will have changed more if the message was truncated. */
        for (int i = actual_len; i < length; i++) {
            seed++;
        }

        /* Construct a reply. */
        for (int i = 0; i < actual_len; i++) {
            seL4_SetMR(i, seed);
            seed++;
        }
    }

    /* Need to do one last reply to match call. */
    seL4_Reply(tag);

    return sel4test_get_result();
}

/* this function is expected to talk to another version of itself, second implies
 * that this is the second to be executed */
static int
nbsendrecv_func(seL4_Word endpoint, seL4_Word seed, seL4_Word arg2, seL4_Word second)
{
    FOR_EACH_LENGTH(length) {
        seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, length);
 
        /* Construct a message. */
        if (length > MIN_LENGTH || second) {
            /* the first of the nbsendrecv pair will not 
             * send a message as the second is not waiting on the endpoint
             * yet and the nbsend will fail, to keep the seeds in sync
             * skip this for the first thread on the first loop */
            seL4_Word actual_len = length > seL4_MsgMaxLength ? seL4_MsgMaxLength : length;
            for (int i = 0; i < actual_len; i++) {
                seL4_SetMR(i, seed);
                seed++;
            }
        }

        tag = seL4_NBSendRecv(endpoint, tag, endpoint, NULL);

        seL4_Word actual_len = seL4_MessageInfo_get_length(tag);
        if (length < seL4_MsgMaxLength) {
            test_geq(actual_len, length);
        }
        
        /* skip the last check for the second thread */
        if (!(second && length == MAX_LENGTH)) {
            for (int i = 0; i < seL4_MessageInfo_get_length(tag); i++) {
                seL4_Word mr = seL4_GetMR(i);
                test_eq(mr, seed);
                seed++;
            }
        }
    }
   
    /* signal we're done to hanging second thread */
    seL4_NBSend(endpoint, seL4_MessageInfo_new(0, 0, 0, MAX_LENGTH));

    return sel4test_get_result();
}

static int
test_ipc_pair(env_t env, test_func_t fa, test_func_t fb, bool inter_as)
{
    helper_thread_t thread_a, thread_b;
    vka_t *vka = &env->vka;

    int error;
    seL4_CPtr ep = vka_alloc_endpoint_leaky(vka);
    seL4_Word start_number = 0xabbacafe;

    /* Test sending messages of varying lengths. */
    /* Please excuse the awful indending here. */
    for (int sender_prio = 98; sender_prio <= 102; sender_prio++) {
        for (int waiter_prio = 100; waiter_prio <= 100; waiter_prio++) {
            for (int sender_first = 0; sender_first <= 1; sender_first++) {
                ZF_LOGD("%d %s %d\n",
                        sender_prio, sender_first ? "->" : "<-", waiter_prio);
                seL4_Word thread_a_arg0, thread_b_arg0;

                if (inter_as) {
                    create_helper_process(env, &thread_a);

                    cspacepath_t path;
                    vka_cspace_make_path(&env->vka, ep, &path);
                    thread_a_arg0 = sel4utils_copy_cap_to_process(&thread_a.process, path);
                    assert(thread_a_arg0 != -1);

                    create_helper_process(env, &thread_b);
                    thread_b_arg0 = sel4utils_copy_cap_to_process(&thread_b.process, path);
                    assert(thread_b_arg0 != -1);

                } else {
                    create_helper_thread(env, &thread_a);
                    create_helper_thread(env, &thread_b);
                    thread_a_arg0 = ep;
                    thread_b_arg0 = ep;
                }

                set_helper_priority(&thread_a, sender_prio);
                set_helper_priority(&thread_b, waiter_prio);

                /* Set the flag for nbwait_func that tells it whether or not it really
                 * should wait. */
                int nbwait_should_wait;
                nbwait_should_wait =
                    (sender_prio < waiter_prio);

                /* Threads are enqueued at the head of the scheduling queue, so the
                 * thread enqueued last will be run first, for a given priority. */
                if (sender_first) {
                    start_helper(env, &thread_b, (helper_fn_t) fb, thread_b_arg0, start_number,
                                 nbwait_should_wait, waiter_prio <= sender_prio);
                    start_helper(env, &thread_a, (helper_fn_t) fa, thread_a_arg0, start_number,
                                 nbwait_should_wait, sender_prio < waiter_prio);
                } else {
                    start_helper(env, &thread_a, (helper_fn_t) fa, thread_a_arg0, start_number,
                                 nbwait_should_wait, sender_prio <= waiter_prio);
                    start_helper(env, &thread_b, (helper_fn_t) fb, thread_b_arg0, start_number,
                                 nbwait_should_wait, waiter_prio < sender_prio);
                }

                wait_for_helper(&thread_a);
                wait_for_helper(&thread_b);

                cleanup_helper(env, &thread_a);
                cleanup_helper(env, &thread_b);

                start_number += 0x71717171;
            }
        }
    }

    error = cnode_delete(env, ep);
    test_assert(!error);
    return sel4test_get_result();
}

static int
test_send_wait(env_t env)
{
    return test_ipc_pair(env, send_func, wait_func, false);
}
DEFINE_TEST(IPC0001, "Test seL4_Send + seL4_Recv", test_send_wait)

static int
test_call_replywait(env_t env)
{
    return test_ipc_pair(env, call_func, replywait_func, false);
}
DEFINE_TEST(IPC0002, "Test seL4_Call + seL4_ReplyRecv", test_call_replywait)

static int
test_call_reply_and_wait(env_t env)
{
    return test_ipc_pair(env, call_func, reply_and_wait_func, false);
}
DEFINE_TEST(IPC0003, "Test seL4_Send + seL4_Reply + seL4_Recv", test_call_reply_and_wait)

static int
test_nbsend_wait(env_t env)
{
    return test_ipc_pair(env, nbsend_func, nbwait_func, false);
}
DEFINE_TEST(IPC0004, "Test seL4_NBSend + seL4_Recv", test_nbsend_wait)

static int
test_send_wait_interas(env_t env)
{
    return test_ipc_pair(env, send_func, wait_func, true);
}
DEFINE_TEST(IPC1001, "Test inter-AS seL4_Send + seL4_Recv", test_send_wait_interas)

static int
test_call_replywait_interas(env_t env)
{
    return test_ipc_pair(env, call_func, replywait_func, true);
}
DEFINE_TEST(IPC1002, "Test inter-AS seL4_Call + seL4_ReplyRecv", test_call_replywait_interas)

static int
test_call_reply_and_wait_interas(env_t env)
{
    return test_ipc_pair(env, call_func, reply_and_wait_func, true);
}
DEFINE_TEST(IPC1003, "Test inter-AS seL4_Send + seL4_Reply + seL4_Recv", test_call_reply_and_wait_interas)

static int
test_nbsend_wait_interas(env_t env)
{
    return test_ipc_pair(env, nbsend_func, nbwait_func, true);
}
DEFINE_TEST(IPC1004, "Test inter-AS seL4_NBSend + seL4_Recv", test_nbsend_wait_interas)

static int
test_ipc_abort_in_call(env_t env)
{
    helper_thread_t thread_a;
    vka_t * vka = &env->vka;

    seL4_CPtr ep = vka_alloc_endpoint_leaky(vka);

    seL4_Word start_number = 0xabbacafe;

    create_helper_thread(env, &thread_a);
    set_helper_priority(&thread_a, 100);

    start_helper(env, &thread_a, (helper_fn_t) call_func, ep, start_number, 0, 0);

    /* Wait for the endpoint that it's going to call. */
    seL4_Word sender_badge = 0;

    seL4_Recv(ep, &sender_badge);

    /* Now suspend the thread. */
    seL4_TCB_Suspend(thread_a.thread.tcb.cptr);

    /* Now resume the thread. */
    seL4_TCB_Resume(thread_a.thread.tcb.cptr);

    /* Now suspend it again for good measure. */
    seL4_TCB_Suspend(thread_a.thread.tcb.cptr);

    /* And delete it. */
    cleanup_helper(env, &thread_a);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0010, "Test suspending an IPC mid-Call()", test_ipc_abort_in_call)

static void
server_fn(seL4_CPtr endpoint, int runs, volatile int *state)
{

    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);

    /* signal the intialiser that we are done */
    ZF_LOGD("Server call");
    *state = *state + 1;
    seL4_NBSendRecv(endpoint, info, endpoint, NULL);
    /* from here on we are running on borrowed time */

    int i = 0;
    while (i < runs) {
        test_assert_fatal(seL4_GetMR(0) == 12345678);

        uint32_t length = seL4_GetMR(1);
        seL4_SetMR(0, 0xdeadbeef);
        info = seL4_MessageInfo_new(0, 0, 0, length);

        *state = *state + 1;
        ZF_LOGD("Server replyRecv\n");
        seL4_ReplyRecv(endpoint, info, NULL);
        i++;
    }

}

static void
proxy_fn(seL4_CPtr receive_endpoint, seL4_CPtr call_endpoint, int runs, volatile int *state)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);

    /* signal the initialiser that we are awake */
    ZF_LOGD("Proxy Call");
    *state = *state + 1;
    seL4_NBSendRecv(call_endpoint, info, receive_endpoint, NULL);
    /* when we get here we are running on a donated scheduling context, 
       as the initialiser has taken ours away */

    int i = 0;
    while (i < runs) {
        test_assert_fatal(seL4_GetMR(0) == 12345678);

        uint32_t length = seL4_GetMR(1);
        seL4_SetMR(0, 12345678);
        seL4_SetMR(1, length);
        info = seL4_MessageInfo_new(0, 0, 0, length);

        ZF_LOGD("Proxy call\n");
        seL4_Call(call_endpoint, info);

        test_assert_fatal(seL4_GetMR(0) == 0xdeadbeef);

        seL4_SetMR(0, 0xdeadbeef);
        ZF_LOGD("Proxy replyRecv\n");
        *state = *state + 1;
        seL4_ReplyRecv(receive_endpoint, info, NULL);
        i++;
    }

}

static void
client_fn(seL4_CPtr endpoint, bool fastpath, int runs, volatile int *state)
{

    /* make the message greater than 4 in size if we do not want to hit the fastpath */
    uint32_t length = fastpath ? 2 : 5;

    int i = 0;
    while (i < runs) {
        seL4_SetMR(0, 12345678);
        seL4_SetMR(1, length);
        seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, length);

        ZF_LOGD("Client call\n");
        info = seL4_Call(endpoint, info);

        test_assert_fatal(seL4_GetMR(0) == 0xdeadbeef);
        i++;
        *state = *state + 1;
    }
}

static int
single_client_server_chain_test(env_t env, int fastpath, int prio_diff)
{
    int error;
    const int runs = 10;
    const int num_proxies = 5;
    int client_prio = 10;
    int server_prio = client_prio + (prio_diff * num_proxies);
    helper_thread_t client, server;
    helper_thread_t proxies[num_proxies];
    volatile int client_state = 0;
    volatile int server_state = 0;
    volatile int proxy_state[num_proxies];


    /* create client */
    create_helper_thread(env, &client);
    set_helper_sched_params(env, &client, 1000llu, 1000llu, 0);
    set_helper_priority(&client, client_prio);

    
    seL4_CPtr receive_endpoint = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr first_endpoint = receive_endpoint;
    /* create proxies */
    for (int i = 0; i < num_proxies; i++) {
        int prio = server_prio + (prio_diff * i);
        proxy_state[i] = 0;
        seL4_CPtr call_endpoint = vka_alloc_endpoint_leaky(&env->vka);
        create_helper_thread(env, &proxies[i]);
        set_helper_priority(&proxies[i], prio);
        ZF_LOGD("Start proxy\n");
        start_helper(env, &proxies[i], (helper_fn_t) proxy_fn, receive_endpoint,
                     call_endpoint, runs, (seL4_Word) &proxy_state[i]);
        
        /* wait for proxy to initialise */
        ZF_LOGD("Recv for proxy\n");
        seL4_Recv(call_endpoint, NULL);
        test_eq(proxy_state[i], 1);
        /* now take away its scheduling context */
        error = seL4_SchedContext_UnbindTCB(proxies[i].thread.sched_context.cptr);
        test_eq(error, seL4_NoError);

        receive_endpoint = call_endpoint;
    }

    /* create the server */
    create_helper_thread(env, &server);
    set_helper_priority(&server, server_prio); 
    ZF_LOGD("Start server");
    start_helper(env, &server, (helper_fn_t) server_fn, receive_endpoint, runs, 
                 (seL4_Word) &server_state, 0);
    /* wait for server to initialise on our time */
    ZF_LOGD("Recv for server");
    seL4_Recv(receive_endpoint, NULL);
    test_eq(server_state, 1);
    /* now take it's scheduling context away */
    error = seL4_SchedContext_UnbindTCB(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    ZF_LOGD("Start client");
    start_helper(env, &client, (helper_fn_t) client_fn, first_endpoint, 
                 fastpath, runs, (seL4_Word) &client_state); 
    
    /* sleep and let the testrun */
    ZF_LOGD("Recv for client");
    wait_for_helper(&client);

    test_eq(server_state, runs + 1);
    test_eq(client_state, runs);
    for (int i = 0; i < num_proxies; i++) {
        test_eq(proxy_state[i], runs + 1);
    }

    return sel4test_get_result();
}

int
test_single_client_slowpath_same_prio(env_t env)
{
    return single_client_server_chain_test(env, 0, 0);
}
DEFINE_TEST(IPC0011, "Client-server inheritance: slowpath, same prio", test_single_client_slowpath_same_prio)

int
test_single_client_slowpath_higher_prio(env_t env)
{
    return single_client_server_chain_test(env, 0, 1);
}
DEFINE_TEST(IPC0012, "Client-server inheritance: slowpath, client higher prio", test_single_client_slowpath_higher_prio)

int
test_single_client_slowpath_lower_prio(env_t env)
{
    return single_client_server_chain_test(env, 0, -1);
}
DEFINE_TEST(IPC0013, "Client-server inheritance: slowpath, client lower prio", test_single_client_slowpath_lower_prio)

int
test_single_client_fastpath_higher_prio(env_t env)
{
    return single_client_server_chain_test(env, 1, 1);
}
DEFINE_TEST(IPC0014, "Client-server inheritance: fastpath, client higher prio", test_single_client_fastpath_higher_prio)

int
test_single_client_fastpath_same_prio(env_t env)
{
    return single_client_server_chain_test(env, 1, 0);
}
DEFINE_TEST(IPC0015, "Client-server inheritance: fastpath, client same prio", test_single_client_fastpath_same_prio)

static void
ipc0016_call_once_fn(seL4_CPtr endpoint, volatile int *state)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);
  
    *state = *state + 1;
    ZF_LOGD("Call %d\n", *state);
    seL4_Call(endpoint, info);
    ZF_LOGD("Resumed with reply\n");
    *state = *state + 1;
 
}

static void
ipc0016_reply_once_fn(seL4_CPtr endpoint)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);
  
    /* send initialisation context back */
    ZF_LOGD("seL4_NBSendRecv\n");
    seL4_NBSendRecv(endpoint, info, endpoint, NULL);
  
    /* reply */
    ZF_LOGD("Reply\n");
    seL4_Reply(info);
      
    /* wait (keeping sc) */
    ZF_LOGD("Recv\n");
    seL4_Recv(endpoint, NULL);

    test_assert_fatal(!"should not get here");
}

static int
test_transfer_on_reply(env_t env)
{
    volatile int state = 1;
    int error;
    helper_thread_t client, server;
    seL4_CPtr endpoint;

    endpoint = vka_alloc_endpoint_leaky(&env->vka);
    create_helper_thread(env, &client); 
    create_helper_thread(env, &server);

    set_helper_priority(&client, 10);
    set_helper_priority(&server, 11);

    start_helper(env, &server, (helper_fn_t) ipc0016_reply_once_fn, endpoint, 0, 0, 0);

    /* wait for server to initialise */
    seL4_Recv(endpoint, NULL);
    /* now remove the schedluing context */
    error = seL4_SchedContext_UnbindTCB(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* start the client */
    start_helper(env, &client, (helper_fn_t) ipc0016_call_once_fn, endpoint, 
                 (seL4_Word) &state, 0, 0);
    
    /* the server will attempt to steal the clients scheduling context 
     * by using seL4_Reply instead of seL4_ReplyWait. However, 
     * a reply cap is a guarantee that a scheduling context will be returned,
     * so it does return to the client and the server hangs.
     */
    wait_for_helper(&client);

    test_eq(state, 3);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0016, "Test reply returns scheduling context", 
        test_transfer_on_reply);

/* used by ipc0017 and ipc0019 */
static void
sender(seL4_CPtr endpoint, volatile int *state)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);
    ZF_LOGD("Client send\n");
    *state = 1;
    seL4_Send(endpoint, info);
    *state = 2;
}

static void
wait_server(seL4_CPtr endpoint, int runs)
{
    /* signal test that we are initialised */
    seL4_Send(endpoint, seL4_MessageInfo_new(0, 0, 0, 0));

    int i = 0;
    while (i < runs) {
        ZF_LOGD("Server wait\n");
        seL4_Recv(endpoint, NULL);
        i++;
    }
}

static int
test_send_to_no_sc(env_t env)
{
    /* sends should block until the server gets a scheduling context.
     * nb sends should not block */
    int error;
    helper_thread_t server, client1, client2;
    volatile int state1, state2;
    seL4_CPtr endpoint;

    endpoint = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &server);
    create_helper_thread(env, &client1);
    create_helper_thread(env, &client2);

    set_helper_priority(&server, 10);
    set_helper_priority(&client1, 9);
    set_helper_priority(&client2, 9);
    
    const int num_server_messages = 4;
    start_helper(env, &server, (helper_fn_t) wait_server, endpoint, num_server_messages, 0, 0);
    seL4_Recv(endpoint, NULL);

    error = seL4_SchedContext_UnbindTCB(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* this message should not result in the server being scheduled */
    ZF_LOGD("NBSend");
    seL4_SetMR(0, 12345678);
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_NBSend(endpoint, info);
    test_eq(seL4_GetMR(0), 12345678);

    /* start clients */
    state1 = 0;
    state2 = 0;
    start_helper(env, &client1, (helper_fn_t) sender, endpoint, (seL4_Word) &state1, 0, 0);
    start_helper(env, &client2, (helper_fn_t) sender, endpoint, (seL4_Word) &state2, 0 , 0);
    
    /* set our prio down, both clients should block as the server cannot 
     * run without a schedluing context */
    error = seL4_TCB_SetPriority(env->tcb, 8);
    test_eq(error, seL4_NoError);
    test_eq(state1, 1);
    test_eq(state2, 1);

    /* restore the servers schedluing context */
    error = seL4_SchedContext_BindTCB(server.thread.sched_context.cptr, server.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

   /* now the clients should be unblocked */
    test_eq(state1, 2);
    test_eq(state2, 2);

    /* this should work */
    seL4_NBSend(endpoint, info);
   
    /* and so should this */
    seL4_Send(endpoint, info);

    /* if the server received the correct number of messages it should now be done */
    wait_for_helper(&server);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0017, "Test seL4_Send/seL4_NBSend to a server with no scheduling context", test_send_to_no_sc)

static void
ipc0018_helper(seL4_CPtr endpoint, volatile int *state) 
{
    *state = 1;

    while (1) {
        ZF_LOGD("Send");
        seL4_Send(endpoint, seL4_MessageInfo_new(0, 0, 0, 0));
        *state = *state + 1;
    }
}

static int
test_receive_no_sc(env_t env) 
{
    helper_thread_t client;
    int error; 
    volatile int state;
    seL4_CPtr endpoint;

    endpoint = vka_alloc_endpoint_leaky(&env->vka);
    create_helper_thread(env, &client);
    set_helper_priority(&client, 10);
    error = seL4_TCB_SetPriority(env->tcb, 9);
    test_eq(error, seL4_NoError);

    /* start the client, it will increment state and send a message */
    start_helper(env, &client, (helper_fn_t) ipc0018_helper, endpoint, 
                 (seL4_Word) &state, 0, 0);

    test_eq(state, 1);

    /* clear the clients scheduling context */
    ZF_LOGD("Unbind scheduling context");
    error = seL4_SchedContext_UnbindTCB(client.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* now we should be able to receive the message, but since the client
     * no longer has a schedluing context it should not run 
     */
    ZF_LOGD("Recv");
    seL4_Recv(endpoint, NULL);
    
    /* check thread has not run */
    test_eq(state, 1);

    /* now set the schedluing context again */
    error = seL4_SchedContext_BindTCB(client.thread.sched_context.cptr, 
                                      client.thread.tcb.cptr);
    test_eq(error, seL4_NoError);
    test_eq(state, 2);

    /* now get another message */
    seL4_Recv(endpoint, NULL);
    test_eq(state, 3);

    /* and another, to check client is well and truly running */
    seL4_Recv(endpoint, NULL);
    test_eq(state, 4);

    return sel4test_get_result();
}   
DEFINE_TEST(IPC0018, "Test receive from a client with no scheduling context", 
            test_receive_no_sc);

static int 
delete_sc_client_sending_on_endpoint(env_t env) 
{
    helper_thread_t client;
    seL4_CPtr endpoint;
    volatile int state = 0;
    int error;

    endpoint = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &client);
    set_helper_priority(&client, 10);
    
    /* set our prio below the helper */
    error = seL4_TCB_SetPriority(env->tcb, 9);
    test_eq(error, seL4_NoError);

    start_helper(env, &client, (helper_fn_t) sender, endpoint, (seL4_Word) &state, 0, 0);
       
    /* the client will run and send on the endpoint */
    test_eq(state, 1);

    /* now delete the scheduling context - this should unbind the client 
     * but not remove the message */

    ZF_LOGD("Destroying schedluing context");
    vka_free_object(&env->vka, &client.thread.sched_context);

    ZF_LOGD("seL4_Recv");
    seL4_Recv(endpoint, NULL);
    test_eq(state, 1);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0019, "Test deleteing the scheduling context while a client is sending on an endpoint", delete_sc_client_sending_on_endpoint);

static void
ipc0020_helper(seL4_CPtr endpoint, volatile int *state)
{
    *state = 1;
    while (1) {
        ZF_LOGD("Recv");
        seL4_Recv(endpoint, NULL);
        *state = *state + 1;
    }
}

static int
delete_sc_client_waiting_on_endpoint(env_t env)
{
    helper_thread_t waiter;
    volatile int state = 0;
    int error;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &waiter);
    set_helper_priority(&waiter, 10);
    error = seL4_TCB_SetPriority(env->tcb, 9);
    start_helper(env, &waiter, (helper_fn_t) ipc0020_helper, endpoint, (seL4_Word) &state, 0, 0);

    /* helper should run and block receiving on endpoint */
    test_eq(state, 1);
 
    /* destroy scheduling context */
    vka_free_object(&env->vka, &waiter.thread.sched_context);

    /* send message */
    seL4_Send(endpoint, seL4_MessageInfo_new(0, 0, 0, 0));
    
    /* thread should not have moved */
    test_eq(state, 1);

    /* now create a new scheduling context and give it to the thread */
    seL4_CPtr sched_context = vka_alloc_sched_context_leaky(&env->vka);
    error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple), sched_context,
                                        1000 * US_IN_S, 1000 * US_IN_S, 0); 
    test_eq(error, seL4_NoError);

    error = seL4_SchedContext_BindTCB(sched_context, waiter.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    /* now the thread should run and receive the message */
    test_eq(state, 2);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0020, "test deleting a scheduling context while the client is waiting on an endpoint", delete_sc_client_waiting_on_endpoint);

static void 
ipc21_faulter_fn(int *addr) 
{
    ZF_LOGD("Fault at %p\n", addr);
    *addr = 0xdeadbeef;
                   
    ZF_LOGD("Resumed\n");
}

static void
ipc21_fault_handler_fn(seL4_CPtr endpoint, vspace_t *vspace, reservation_t *res)
{
    int error;
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);
    
    info = seL4_NBSendRecv(endpoint, info, endpoint, NULL);
    while (1) { 
        test_check(seL4_isPageFault_Tag(info));
        void *addr = (void *) seL4_PF_Addr();
        ZF_LOGD("Handling fault at %p\n", addr);
  
        error = vspace_new_pages_at_vaddr(vspace, addr, 1, seL4_PageBits, *res);
        test_eq(error, seL4_NoError);
  
        seL4_ReplyRecv(endpoint, info, NULL);
    }
}

static int 
test_fault_handler_donated_sc(env_t env)
{
    helper_thread_t handler, faulter;
    void *vaddr = NULL;
    reservation_t res;
    seL4_CPtr endpoint;

    endpoint = vka_alloc_endpoint_leaky(&env->vka);
   
    res = vspace_reserve_range(&env->vspace, PAGE_SIZE_4K, seL4_AllRights, 1, &vaddr);
    test_check(vaddr != NULL);

    create_helper_thread(env, &handler);
    create_helper_thread(env, &faulter);

    /* start fault handler */
    start_helper(env, &handler, (helper_fn_t) ipc21_fault_handler_fn, 
                 endpoint, (seL4_Word) &env->vspace, (seL4_Word) &res, 
                 0);
    
    /* wait for it to initialise */ 
    seL4_Recv(endpoint, NULL);

    /* now remove its scheduling context */
    int error = seL4_SchedContext_UnbindTCB(handler.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* set fault handler */
    seL4_CapData_t data = seL4_CapData_Guard_new(0, seL4_WordBits - env->cspace_size_bits);
    seL4_CapData_t null = {{0}};
    error = seL4_TCB_SetSpace(faulter.thread.tcb.cptr, endpoint, seL4_CapNull,
                              env->cspace_root, data, env->page_directory, null);
    test_eq(error, seL4_NoError);

    /* start the fault handler */
    start_helper(env, &faulter, (helper_fn_t) ipc21_faulter_fn, (seL4_Word) vaddr, 0, 0, 0); 
    
    /* the faulter handler will restore the faulter and we should not block here */
    wait_for_helper(&faulter);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0021, "Test fault handler on donated scheduling context", 
        test_fault_handler_donated_sc);

static void
ipc22_client_fn(seL4_CPtr endpoint, volatile int *state) 
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);

    ZF_LOGD("Client init");
   
    seL4_Call(endpoint, info);
        *state = *state + 1;
    ZF_LOGD("Client receive reply");
}

static seL4_CPtr ipc22_go;

static void
ipc22_server_fn(seL4_CPtr init_ep, seL4_CPtr reply_cap)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);
    
    ZF_LOGD("Server init\n");
    
    /* wait for the signal to go from the test runner - 
     * we have to block here to wait for all the clients to 
     * start and queue up - otherwise they will all be served
     * by the same server and the point is to test stack spawning  */
    seL4_NBSendRecv(init_ep, info, init_ep, NULL);

    ZF_LOGD("Server reply to fwded cap\n");
    seL4_Send(reply_cap, info);
    
}

static void
ipc22_stack_spawner_fn(env_t env, seL4_CPtr endpoint, int server_prio, int runs)
{
    helper_thread_t servers[runs];
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);
    seL4_CPtr init_ep = vka_alloc_endpoint_leaky(&env->vka);

    ZF_LOGD("Stack spawner init");
    /* send init sched context back */
    seL4_NBSendRecv(endpoint, info, endpoint, NULL);
    /* we are now running on borrowed time */
    
    for (int i = 0; i < runs; i++) {
        cspacepath_t path;
        vka_cspace_alloc_path(&env->vka, &path);
        /* save the reply cap */
        vka_cnode_saveCaller(&path);

        create_helper_thread(env, &servers[i]);
        set_helper_priority(&servers[i], server_prio);

        /* start helper and allow to initialise */
        ZF_LOGD("Spawn server\n");
        start_helper(env, &servers[i], (helper_fn_t) ipc22_server_fn, init_ep,
                path.capPtr, 0, 0);
        /* wait for it to block */
        seL4_Recv(init_ep, NULL);
        
        /*  now remove the schedling context */
        int error =  seL4_SchedContext_UnbindTCB(servers[i].thread.sched_context.cptr);
        test_eq(error, seL4_NoError);
        
        /* and forward the one we have */
        ZF_LOGD("Send to server, wait for another client");
        seL4_NBSendRecv(init_ep, info, endpoint, NULL);
        ZF_LOGD("Got another client\n");
    }
}

static int
test_stack_spawning_server(env_t env) 
{
    const int runs = 3;
    helper_thread_t clients[runs];
    helper_thread_t stack_spawner;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    ipc22_go = vka_alloc_endpoint_leaky(&env->vka);
    volatile int state = 0;
    int error;
    int our_prio = 10;
    
    create_helper_thread(env, &stack_spawner);
    start_helper(env, &stack_spawner, (helper_fn_t) ipc22_stack_spawner_fn, 
                (seL4_Word) env, endpoint, our_prio + 1, runs);

    /* wait for stack spawner to init */
    ZF_LOGD("Wait for stack spawner to init");
    seL4_Recv(endpoint, NULL);
    
    /* take away scheduling context */
    error = seL4_SchedContext_UnbindTCB(stack_spawner.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    error = seL4_TCB_SetPriority(env->tcb, our_prio);
    test_eq(error, seL4_NoError);
    set_helper_priority(&stack_spawner, our_prio + 2);
    set_helper_max_priority(&stack_spawner, seL4_MaxPrio - 1);
    
    ZF_LOGD("Starting clients");
    /* create and start clients */
    for (int i = 0; i < runs; i++) {
        create_helper_thread(env, &clients[i]);
        set_helper_priority(&clients[i], our_prio + 1);
        start_helper(env, &clients[i], (helper_fn_t) ipc22_client_fn, endpoint, (seL4_Word) &state, 0, 0);
    }

    /* set our priority down so servers can run */
    error = seL4_TCB_SetPriority(env->tcb, our_prio - 2);
    test_eq(error, seL4_NoError);

    for (int i = 0; i < runs; i++) {
        wait_for_helper(&clients[i]);
    }

    ZF_LOGD("Done");
    /* make sure all the clients got served */
    test_eq(state, runs);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0022, "Test stack spawning server with scheduling context donation", test_stack_spawning_server);

/* used by ipc0023 and 0024 */
static void
ipc23_client_fn(seL4_CPtr ep, volatile int *state)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);

    *state = 1;

    ZF_LOGD("Call");
    seL4_Call(ep, info);

    /* should not get here */
    *state = 2;
}

/* used by ipc0023 and 0024 */
static void 
ipc23_server_fn(seL4_CPtr client_ep, seL4_CPtr wait_ep)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);

    /* send to the wait_ep to tell the test we are initialised,
     * then wait on the client_ep to receive a scheduling context */
    seL4_NBSendRecv(wait_ep, info, client_ep, NULL);

    /* now block */
    seL4_Recv(wait_ep, NULL);
}

static int
test_delete_reply_cap_sc(env_t env) 
{
    helper_thread_t client, server;
    seL4_CPtr client_ep, server_ep;
    volatile int state = 0;
    int error;
    client_ep = vka_alloc_endpoint_leaky(&env->vka);
    server_ep = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &client);
    create_helper_thread(env, &server);

    start_helper(env, &client, (helper_fn_t) ipc23_client_fn, client_ep, 
                 (seL4_Word) &state, 0, 0);
    start_helper(env, &server, (helper_fn_t) ipc23_server_fn, client_ep, 
                 server_ep, 0, 0);

    /* wait for server to init */
    seL4_Recv(server_ep, NULL);
    /* take its scheduling context away */
    error = seL4_SchedContext_UnbindTCB(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* set the client and server prio higher than ours so they can run */
    error = seL4_TCB_SetPriority(env->tcb, 10);
    test_eq(error, seL4_NoError);

    set_helper_priority(&client, 11);
    set_helper_priority(&server, 11);

    /* now the client should have run and called the server*/
    test_eq(state, 1);

    /* steal the reply cap */
    cspacepath_t path;
    vka_cspace_alloc_path(&env->vka, &path);
    error = vka_cnode_saveTCBCaller(&path, &server.thread.tcb);
    test_eq(error, seL4_NoError);

    /* delete scheduling context */
    vka_free_object(&env->vka,& client.thread.sched_context);

    /* try to reply, the client should not run as it has no scheduling context */
    seL4_Signal(path.capPtr);

    test_eq(state, 1);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0023, "Test deleting the scheduling context tracked in a reply cap", 
        test_delete_reply_cap_sc)

static int 
test_delete_reply_cap_then_sc(env_t env) 
{

    helper_thread_t client, server;
    seL4_CPtr client_ep, server_ep;
    volatile int state = 0;
    int error;

    client_ep = vka_alloc_endpoint_leaky(&env->vka);
    server_ep = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &client);
    create_helper_thread(env, &server);

    set_helper_priority(&client, 11);
    set_helper_priority(&server, 11);
     /* start server */
    start_helper(env, &server, (helper_fn_t) ipc23_server_fn, client_ep, 
                 server_ep, 0, 0);

    ZF_LOGD("Waiting for server");
    /* wait for server to init */
    seL4_Recv(server_ep, NULL);

    /* set our prio down so client can run */
    error = seL4_TCB_SetPriority(env->tcb, 10);
    test_eq(error, 0);
    
    ZF_LOGD("Removed sc\n");
    /* remove schedluing context */
    error = seL4_SchedContext_UnbindTCB(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    ZF_LOGD("Start client");
    /* start client */
    start_helper(env, &client, (helper_fn_t) ipc23_client_fn, client_ep, 
                 (seL4_Word) &state, 0, 0);
    /* client should have started */
    test_eq(state, 1);
     
    ZF_LOGD("Steal reply cap ");
    /* steal the reply cap */
    cspacepath_t path;
    vka_cspace_alloc_path(&env->vka, &path);
    error = vka_cnode_saveTCBCaller(&path, &server.thread.tcb);
    test_eq(error, seL4_NoError);

    /* nuke the reply cap */
    vka_cnode_delete(&path);
    /* nuke the sc */
    vka_free_object(&env->vka, &client.thread.sched_context);

    ZF_LOGD("Done");
    /* caller should not run */
    test_eq(1, state);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0024, "Test deleting the reply cap in the scheduling context", 
            test_delete_reply_cap_then_sc);

static int
test_nbsendrecv(env_t env)
{
    return test_ipc_pair(env, (test_func_t) nbsendrecv_func, (test_func_t) nbsendrecv_func, false);
}
DEFINE_TEST(IPC0025, "Test seL4_NBSendRecv + seL4_NBSendRecv", test_nbsendrecv)

static int
test_nbsendrecv_interas(env_t env)
{
    return test_ipc_pair(env, (test_func_t) nbsendrecv_func, (test_func_t) nbsendrecv_func, false);
}
DEFINE_TEST(IPC0026, "Test interas seL4_NBSendRecv + seL4_NBSendRecv", test_nbsendrecv_interas)

