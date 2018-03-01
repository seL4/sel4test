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
#include <stdlib.h>
#include <sel4/sel4.h>
#include <vka/object.h>

#include "../helpers.h"

#define MIN_LENGTH 0
#define MAX_LENGTH (seL4_MsgMaxLength)

#define FOR_EACH_LENGTH(len_var) \
    for(int len_var = MIN_LENGTH; len_var <= MAX_LENGTH; len_var++)

typedef int (*test_func_t)(seL4_Word /* endpoint */, seL4_Word /* seed */, seL4_Word /* reply */,
                           seL4_CPtr /* extra */);

static int
send_func(seL4_Word endpoint, seL4_Word seed, seL4_Word reply, seL4_Word extra)
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
nbsend_func(seL4_Word endpoint, seL4_Word seed, seL4_Word reply, seL4_Word extra)
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
call_func(seL4_Word endpoint, seL4_Word seed, seL4_Word reply, seL4_Word extra)
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
wait_func(seL4_Word endpoint, seL4_Word seed, seL4_Word reply, seL4_Word extra)
{
    FOR_EACH_LENGTH(length) {
        seL4_MessageInfo_t tag;
        seL4_Word sender_badge = 0;

        tag = api_recv(endpoint, &sender_badge, reply);
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
nbwait_func(seL4_Word endpoint, seL4_Word seed, seL4_Word reply, seL4_Word nbwait_should_wait)
{
    if (!nbwait_should_wait) {
        return sel4test_get_result();
    }

    FOR_EACH_LENGTH(length) {
        seL4_MessageInfo_t tag;
        seL4_Word sender_badge = 0;

        tag = api_recv(endpoint, &sender_badge, reply);
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
replywait_func(seL4_Word endpoint, seL4_Word seed, seL4_CPtr reply, seL4_Word extra)
{
    int first = 1;
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 0);

    FOR_EACH_LENGTH(length) {
        seL4_Word sender_badge = 0;

        /* First reply/wait can't reply. */
        if (first) {
#ifdef CONFIG_KERNEL_RT
            tag = seL4_NBSendRecv(endpoint, tag, endpoint, &sender_badge, reply);
#else
            tag = seL4_Recv(endpoint, &sender_badge);
#endif
            first = 0;
        } else {
            tag = api_reply_recv(endpoint, tag, &sender_badge, reply);
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
    api_reply(reply, tag);

    return sel4test_get_result();
}

static int
reply_and_wait_func(seL4_Word endpoint, seL4_Word seed, seL4_CPtr reply, seL4_Word unused)
{
    int first = 1;

    seL4_MessageInfo_t tag;
    FOR_EACH_LENGTH(length) {
        seL4_Word sender_badge = 0;

        /* First reply/wait can't reply. */
        if (!first) {
            api_reply(reply, tag);
        } else {
            first = 0;
        }

        tag = api_recv(endpoint, &sender_badge, reply);

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
    api_reply(reply, tag);

    return sel4test_get_result();
}

#ifdef CONFIG_KERNEL_RT
/* this function is expected to talk to another version of itself, second implies
 * that this is the second to be executed */
static int
nbsendrecv_func(seL4_Word endpoint, seL4_Word seed, seL4_Word reply, seL4_Word unused)
{
    FOR_EACH_LENGTH(length) {
        api_nbsend_recv(endpoint, seL4_MessageInfo_new(0, 0, 0, MAX_LENGTH), endpoint, NULL, reply);
    }

    /* signal we're done to hanging second thread */
    seL4_NBSend(endpoint, seL4_MessageInfo_new(0, 0, 0, MAX_LENGTH));

    return sel4test_get_result();
}
#endif /* CONFIG_KERNEL_RT */

static int
test_ipc_pair(env_t env, test_func_t fa, test_func_t fb, bool inter_as, seL4_Word nr_cores)
{
    helper_thread_t thread_a, thread_b;
    vka_t *vka = &env->vka;

    UNUSED int error;
    seL4_CPtr ep = vka_alloc_endpoint_leaky(vka);
    seL4_Word start_number = 0xabbacafe;

    /* Test sending messages of varying lengths. */
    /* Please excuse the awful indending here. */
    for (int core_a = 0; core_a < nr_cores; core_a++) {
        for (int core_b = 0; core_b < nr_cores; core_b++) {
            for (int sender_prio = 98; sender_prio <= 102; sender_prio++) {
                for (int waiter_prio = 100; waiter_prio <= 100; waiter_prio++) {
                    for (int sender_first = 0; sender_first <= 1; sender_first++) {
                        ZF_LOGD("%d %s %d\n",
                                sender_prio, sender_first ? "->" : "<-", waiter_prio);
                        seL4_Word thread_a_arg0, thread_b_arg0;
                        seL4_CPtr thread_a_reply, thread_b_reply;

                        if (inter_as) {
                            create_helper_process(env, &thread_a);

                            cspacepath_t path;
                            vka_cspace_make_path(&env->vka, ep, &path);
                            thread_a_arg0 = sel4utils_copy_path_to_process(&thread_a.process, path);
                            assert(thread_a_arg0 != -1);

                            create_helper_process(env, &thread_b);
                            thread_b_arg0 = sel4utils_copy_path_to_process(&thread_b.process, path);
                            assert(thread_b_arg0 != -1);

                            thread_a_reply = SEL4UTILS_REPLY_SLOT;
                            thread_b_reply = SEL4UTILS_REPLY_SLOT;

                        } else {
                            create_helper_thread(env, &thread_a);
                            create_helper_thread(env, &thread_b);
                            thread_a_arg0 = ep;
                            thread_b_arg0 = ep;
                            thread_a_reply = get_helper_reply(&thread_a);
                            thread_b_reply = get_helper_reply(&thread_b);
                        }

                        set_helper_priority(env, &thread_a, sender_prio);
                        set_helper_priority(env, &thread_b, waiter_prio);

                        set_helper_affinity(env, &thread_a, core_a);
                        set_helper_affinity(env, &thread_b, core_b);

                        /* Set the flag for nbwait_func that tells it whether or not it really
                         * should wait. */
                        int nbwait_should_wait;
                        nbwait_should_wait =
                            (sender_prio < waiter_prio);

                        /* Threads are enqueued at the head of the scheduling queue, so the
                         * thread enqueued last will be run first, for a given priority. */
                        if (sender_first) {
                            start_helper(env, &thread_b, (helper_fn_t) fb, thread_b_arg0, start_number,
                                         thread_b_reply, nbwait_should_wait);
                            start_helper(env, &thread_a, (helper_fn_t) fa, thread_a_arg0, start_number,
                                        thread_a_reply, nbwait_should_wait);
                        } else {
                            start_helper(env, &thread_a, (helper_fn_t) fa, thread_a_arg0, start_number,
                                        thread_a_reply, nbwait_should_wait);
                            start_helper(env, &thread_b, (helper_fn_t) fb, thread_b_arg0, start_number,
                                         thread_b_reply, nbwait_should_wait);
                        }

                        wait_for_helper(&thread_a);
                        wait_for_helper(&thread_b);

                        cleanup_helper(env, &thread_a);
                        cleanup_helper(env, &thread_b);

                        start_number += 0x71717171;
                    }
                }
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
    return test_ipc_pair(env, send_func, wait_func, false, env->cores);
}
DEFINE_TEST(IPC0001, "Test seL4_Send + seL4_Recv", test_send_wait, true)

static int
test_call_replywait(env_t env)
{
    return test_ipc_pair(env, call_func, replywait_func, false, env->cores);
}
DEFINE_TEST(IPC0002, "Test seL4_Call + seL4_ReplyRecv", test_call_replywait, true)

static int
test_call_reply_and_wait(env_t env)
{
    return test_ipc_pair(env, call_func, reply_and_wait_func, false, env->cores);
}
DEFINE_TEST(IPC0003, "Test seL4_Send + seL4_Reply + seL4_Recv", test_call_reply_and_wait, true)

static int
test_nbsend_wait(env_t env)
{
    return test_ipc_pair(env, nbsend_func, nbwait_func, false, 1);
}
DEFINE_TEST(IPC0004, "Test seL4_NBSend + seL4_Recv", test_nbsend_wait, true)

static int
test_send_wait_interas(env_t env)
{
    return test_ipc_pair(env, send_func, wait_func, true, env->cores);
}
DEFINE_TEST(IPC1001, "Test inter-AS seL4_Send + seL4_Recv", test_send_wait_interas, true)

static int
test_call_replywait_interas(env_t env)
{
    return test_ipc_pair(env, call_func, replywait_func, true, env->cores);
}
DEFINE_TEST(IPC1002, "Test inter-AS seL4_Call + seL4_ReplyRecv", test_call_replywait_interas, true)

static int
test_call_reply_and_wait_interas(env_t env)
{
    return test_ipc_pair(env, call_func, reply_and_wait_func, true, env->cores);
}
DEFINE_TEST(IPC1003, "Test inter-AS seL4_Send + seL4_Reply + seL4_Recv", test_call_reply_and_wait_interas, true)

static int
test_nbsend_wait_interas(env_t env)
{
    return test_ipc_pair(env, nbsend_func, nbwait_func, true, 1);
}
DEFINE_TEST(IPC1004, "Test inter-AS seL4_NBSend + seL4_Recv", test_nbsend_wait_interas, true)

static int
test_ipc_abort_in_call(env_t env)
{
    helper_thread_t thread_a;
    vka_t * vka = &env->vka;

    seL4_CPtr ep = vka_alloc_endpoint_leaky(vka);
    seL4_CPtr reply = vka_alloc_reply_leaky(vka);

    seL4_Word start_number = 0xabbacafe;

    create_helper_thread(env, &thread_a);
    set_helper_priority(env, &thread_a, 100);

    start_helper(env, &thread_a, (helper_fn_t) call_func, ep, start_number, 0, 0);

    /* Wait for the endpoint that it's going to call. */
    seL4_Word sender_badge = 0;

    api_recv(ep, &sender_badge, reply);

    /* Now suspend the thread. */
    seL4_TCB_Suspend(get_helper_tcb(&thread_a));

    /* Now resume the thread. */
    seL4_TCB_Resume(get_helper_tcb(&thread_a));

    /* Now suspend it again for good measure. */
    seL4_TCB_Suspend(get_helper_tcb(&thread_a));

    /* And delete it. */
    cleanup_helper(env, &thread_a);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0010, "Test suspending an IPC mid-Call()", test_ipc_abort_in_call, true)

#ifdef CONFIG_KERNEL_RT
#define RUNS 10
static void
server_fn(seL4_CPtr endpoint, seL4_CPtr reply, volatile int *state)
{
    /* signal the intialiser that we are done */
    ZF_LOGD("Server call");
    *state = *state + 1;
    seL4_MessageInfo_t info = api_nbsend_recv(endpoint, info, endpoint, NULL, reply);
    /* from here on we are running on borrowed time */
    ZF_LOGD("Server awake!\n");
    int i = 0;
    while (i < RUNS) {
        test_eq(seL4_GetMR(0), (seL4_Word) 12345678);
        seL4_SetMR(0, 0xdeadbeef);
        *state = *state + 1;
        ZF_LOGD("Server replyRecv\n");
        info = seL4_ReplyRecv(endpoint, info, NULL, reply);
        i++;
    }
}

static void
proxy_fn(seL4_CPtr receive_endpoint, seL4_CPtr call_endpoint, seL4_Word reply, volatile int *state)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);

    /* signal the initialiser that we are awake */
    ZF_LOGD("Proxy nbsendrecv, sending on ep %lu, receiving on ep %lu, reply is %lu\n", call_endpoint, receive_endpoint, reply);
    *state = *state + 1;
    info = api_nbsend_recv(call_endpoint, info, receive_endpoint, NULL, reply);
    /* when we get here we are running on a donated scheduling context,
       as the initialiser has taken ours away */

    int i = 0;
    while (i < RUNS) {
        test_eq(seL4_GetMR(0), (seL4_Word) 12345678);
        seL4_SetMR(0, 12345678);

        ZF_LOGD("Proxy call\n");
        seL4_Call(call_endpoint, info);

        test_eq(seL4_GetMR(0), (seL4_Word) 0xdeadbeef);

        seL4_SetMR(0, 0xdeadbeef);
        ZF_LOGD("Proxy replyRecv\n");
        *state = *state + 1;
        info = seL4_ReplyRecv(receive_endpoint, info, NULL, reply);
        i++;
    }
}

static void
client_fn(seL4_CPtr endpoint, bool fastpath, int unused, volatile int *state)
{

    /* make the message greater than 4 in size if we do not want to hit the fastpath */
    uint32_t length = fastpath ? 1 : 8;

    int i = 0;
    while (i < RUNS) {
        seL4_SetMR(0, 12345678);
        seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, length);

        ZF_LOGD("Client calling on ep %lu\n", endpoint);
        info = seL4_Call(endpoint, info);

        test_eq(seL4_GetMR(0), (seL4_Word) 0xdeadbeef);
        i++;
        *state = *state + 1;
    }
}

static int
single_client_server_chain_test(env_t env, int fastpath, int prio_diff)
{
    const int num_proxies = 5;
    int client_prio = 10;
    int server_prio = client_prio + (prio_diff * num_proxies);
    helper_thread_t client, server;
    helper_thread_t proxies[num_proxies];
    volatile int client_state = 0;
    volatile int server_state = 0;
    volatile int proxy_state[num_proxies];

    create_helper_thread(env, &client);
    set_helper_priority(env, &client, client_prio);

    seL4_CPtr receive_endpoint = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr first_endpoint = receive_endpoint;

	/* create proxies */
    for (int i = 0; i < num_proxies; i++) {
        int prio = server_prio + (prio_diff * i);
        proxy_state[i] = 0;
        seL4_CPtr call_endpoint = vka_alloc_endpoint_leaky(&env->vka);
        create_helper_thread(env, &proxies[i]);
        set_helper_priority(env, &proxies[i], prio);
        ZF_LOGD("Start proxy\n");
        start_helper(env, &proxies[i], (helper_fn_t) proxy_fn, receive_endpoint,
                     call_endpoint, proxies[i].thread.reply.cptr, (seL4_Word) &proxy_state[i]);

        /* wait for proxy to initialise */
        ZF_LOGD("Recv for proxy\n");
        seL4_Wait(call_endpoint, NULL);
        test_eq(proxy_state[i], 1);
        /* now take away its scheduling context */
        int error = api_sc_unbind(proxies[i].thread.sched_context.cptr);
        test_eq(error, seL4_NoError);
        receive_endpoint = call_endpoint;
    }

    /* create the server */
    create_helper_thread(env, &server);
    set_helper_priority(env, &server, server_prio);
    ZF_LOGD("Start server");
    start_helper(env, &server, (helper_fn_t) server_fn, receive_endpoint, server.thread.reply.cptr,
                   (seL4_Word) &server_state, 0);
    /* wait for server to initialise on our time */
    ZF_LOGD("Recv for server");
    seL4_Wait(receive_endpoint, NULL);
    test_eq(server_state, 1);

    /* now take it's scheduling context away */
    int error = api_sc_unbind(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    ZF_LOGD("Start client");
    start_helper(env, &client, (helper_fn_t) client_fn, first_endpoint,
                 fastpath, RUNS, (seL4_Word) &client_state);

    /* sleep and let the testrun */
    ZF_LOGD("wait_for_helper() for client");
    wait_for_helper(&client);

    test_eq(server_state, RUNS + 1);
    test_eq(client_state, RUNS);
    for (int i = 0; i < num_proxies; i++) {
        test_eq(proxy_state[i], RUNS + 1);
    }

    return sel4test_get_result();
}

int test_single_client_slowpath_same_prio(env_t env)
{
    return single_client_server_chain_test(env, 0, 0);
}
DEFINE_TEST(IPC0011, "Client-server inheritance: slowpath, same prio", test_single_client_slowpath_same_prio, config_set(CONFIG_KERNEL_RT))

int test_single_client_slowpath_higher_prio(env_t env)
{
    return single_client_server_chain_test(env, 0, 1);
}
DEFINE_TEST(IPC0012, "Client-server inheritance: slowpath, client higher prio",
            test_single_client_slowpath_higher_prio, config_set(CONFIG_KERNEL_RT))

int test_single_client_slowpath_lower_prio(env_t env)
{
    return single_client_server_chain_test(env, 0, -1);
}
DEFINE_TEST(IPC0013, "Client-server inheritance: slowpath, client lower prio",
test_single_client_slowpath_lower_prio, config_set(CONFIG_KERNEL_RT))

int test_single_client_fastpath_higher_prio(env_t env)
{
    return single_client_server_chain_test(env, 1, 1);
}
DEFINE_TEST(IPC0014, "Client-server inheritance: fastpath, client higher prio", test_single_client_fastpath_higher_prio, config_set(CONFIG_KERNEL_RT))

int
test_single_client_fastpath_same_prio(env_t env)
{
    return single_client_server_chain_test(env, 1, 0);
}
DEFINE_TEST(IPC0015, "Client-server inheritance: fastpath, client same prio", test_single_client_fastpath_same_prio, config_set(CONFIG_KERNEL_RT))

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
ipc0016_reply_once_fn(seL4_CPtr endpoint, seL4_CPtr reply)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);

    /* send initialisation context back */
    ZF_LOGD("seL4_nbsendrecv\n");
    api_nbsend_recv(endpoint, info, endpoint, NULL, reply);

    /* reply */
    ZF_LOGD("Reply\n");
    seL4_Send(reply, info);

    /* wait (keeping sc) */
    ZF_LOGD("Recv\n");
    seL4_Wait(endpoint, NULL);

    test_check(!"should not get here");
}

static int test_transfer_on_reply(env_t env)
{
    volatile int state = 1;
    helper_thread_t client, server;

    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    create_helper_thread(env, &client);
    create_helper_thread(env, &server);

    set_helper_priority(env, &client, 10);
    set_helper_priority(env, &server, 11);

    start_helper(env, &server, (helper_fn_t) ipc0016_reply_once_fn, endpoint, server.thread.reply.cptr, 0, 0);

    /* wait for server to initialise */
    seL4_Wait(endpoint, NULL);
    /* now remove the schedluing context */
    int error = api_sc_unbind(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* start the client */
    start_helper(env, &client, (helper_fn_t) ipc0016_call_once_fn, endpoint,
                 (seL4_Word) &state, 0, 0);
    /* the server will attempt to steal the clients scheduling context
     * by using seL4_Send instead of seL4_ReplyWait. However,
     * a reply cap is a guarantee that a scheduling context will be returned,
     * so it does return to the client and the server hangs.
     */
    wait_for_helper(&client);

    test_eq(state, 3);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0016, "Test reply returns scheduling context",
            test_transfer_on_reply, config_set(CONFIG_KERNEL_RT));

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
wait_server(seL4_CPtr endpoint, int messages)
{
    /* signal test that we are initialised */
    seL4_Send(endpoint, seL4_MessageInfo_new(0, 0, 0, 0));
    int i = 0;
    while (i < messages) {
        ZF_LOGD("Server wait\n");
        seL4_Wait(endpoint, NULL);
        i++;
    }
}

static int
test_send_to_no_sc(env_t env)
{
    /* sends should block until the server gets a scheduling context.
     * nb sends should not block */
    helper_thread_t server, client1, client2;

    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &server);
    create_helper_thread(env, &client1);
    create_helper_thread(env, &client2);

    set_helper_priority(env, &server, 10);
    set_helper_priority(env, &client1, 9);
    set_helper_priority(env, &client2, 9);

    const int num_server_messages = 4;
    start_helper(env, &server, (helper_fn_t) wait_server, endpoint, num_server_messages, 0, 0);
    seL4_Wait(endpoint, NULL);

    int error = api_sc_unbind(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* this message should not result in the server being scheduled */
    ZF_LOGD("NBSend");
    seL4_SetMR(0, 12345678);
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_NBSend(endpoint, info);
    test_eq(seL4_GetMR(0), (seL4_Word)12345678);

    /* start clients */
    volatile int state1 = 0;
    volatile int state2 = 0;
    start_helper(env, &client1, (helper_fn_t) sender, endpoint, (seL4_Word) &state1, 0, 0);
    start_helper(env, &client2, (helper_fn_t) sender, endpoint, (seL4_Word) &state2, 0 , 0);

    /* set our prio down, both clients should block as the server cannot
     * run without a schedluing context */
    error = seL4_TCB_SetPriority(env->tcb, env->tcb, 8);
    test_eq(error, seL4_NoError);
    test_eq(state1, 1);
    test_eq(state2, 1);

    /* restore the servers schedluing context */
    error = api_sc_bind(server.thread.sched_context.cptr, server.thread.tcb.cptr);
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
DEFINE_TEST(IPC0017, "Test seL4_Send/seL4_NBSend to a server with no scheduling context", test_send_to_no_sc, config_set(CONFIG_KERNEL_RT))

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
    volatile int state = 0;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    create_helper_thread(env, &client);
    set_helper_priority(env, &client, 10);
    int error = seL4_TCB_SetPriority(env->tcb, env->tcb, 9);
    test_eq(error, seL4_NoError);

      /* start the client, it will increment state and send a message */
    start_helper(env, &client, (helper_fn_t) ipc0018_helper, endpoint,
                 (seL4_Word) &state, 0, 0);

    test_eq(state, 1);

    /* clear the clients scheduling context */
    ZF_LOGD("Unbind scheduling context");
    error = api_sc_unbind(client.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* now we should be able to receive the message, but since the client
     * no longer has a schedluing context it should not run
     */
    ZF_LOGD("Recv");
    seL4_Wait(endpoint, NULL);

    /* check thread has not run */
    test_eq(state, 1);

    /* now set the schedluing context again */
    error = api_sc_bind(client.thread.sched_context.cptr,
                                   client.thread.tcb.cptr);
    test_eq(error, seL4_NoError);
    test_eq(state, 2);

    /* now get another message */
    seL4_Wait(endpoint, NULL);
    test_eq(state, 3);

    /* and another, to check client is well and truly running */
    seL4_Wait(endpoint, NULL);
    test_eq(state, 4);

     return sel4test_get_result();
}
DEFINE_TEST(IPC0018, "Test receive from a client with no scheduling context",
            test_receive_no_sc, config_set(CONFIG_KERNEL_RT));

static int
delete_sc_client_sending_on_endpoint(env_t env)
{
    helper_thread_t client;
    volatile int state = 0;

    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &client);
    set_helper_priority(env, &client, 10);

    /* set our prio below the helper */
    int error = seL4_TCB_SetPriority(env->tcb, env->tcb, 9);
    test_eq(error, seL4_NoError);

    start_helper(env, &client, (helper_fn_t) sender, endpoint, (seL4_Word) &state, 0, 0);

    /* the client will run and send on the endpoint */
    test_eq(state, 1);

    /* now delete the scheduling context - this should unbind the client
     * but not remove the message */

    ZF_LOGD("Destroying schedluing context");
    vka_free_object(&env->vka, &client.thread.sched_context);

    ZF_LOGD("seL4_Wait");
    seL4_Wait(endpoint, NULL);
    test_eq(state, 1);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0019, "Test deleteing the scheduling context while a client is sending on an endpoint", delete_sc_client_sending_on_endpoint, config_set(CONFIG_KERNEL_RT));

static void
ipc0020_helper(seL4_CPtr endpoint, volatile int *state)
{
    *state = 1;
    while (1) {
        ZF_LOGD("Recv");
        seL4_Wait(endpoint, NULL);
        *state = *state + 1;
    }
}

static int
delete_sc_client_waiting_on_endpoint(env_t env)
{
    helper_thread_t waiter;
    volatile int state = 0;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &waiter);
    set_helper_priority(env, &waiter, 10);
    int error = seL4_TCB_SetPriority(env->tcb, env->tcb, 9);
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
    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0), sched_context,
                                        1000 * US_IN_S, 1000 * US_IN_S, 0, 0);
    test_eq(error, seL4_NoError);

    error = api_sc_bind(sched_context, waiter.thread.tcb.cptr);
    test_eq(error, seL4_NoError);

    /* now the thread should run and receive the message */
    test_eq(state, 2);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0020, "test deleting a scheduling context while the client is waiting on an endpoint",
delete_sc_client_waiting_on_endpoint, config_set(CONFIG_KERNEL_RT));

static void ipc21_faulter_fn(int *addr)
{
    ZF_LOGD("Fault at %p\n", addr);
    *addr = 0xdeadbeef;
    ZF_LOGD("Resumed\n");
}

static void ipc21_fault_handler_fn(seL4_CPtr endpoint, vspace_t *vspace, reservation_t *res, seL4_CPtr reply)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);
    info = api_nbsend_recv(endpoint, info, endpoint, NULL, reply);

    while (1) {
        test_check(seL4_isVMFault_tag(info));
        void *addr = (void *) seL4_GetMR(seL4_VMFault_Addr);
        ZF_LOGD("Handling fault at %p\n", addr);
        int error = vspace_new_pages_at_vaddr(vspace, addr, 1, seL4_PageBits, *res);
        test_eq(error, seL4_NoError);
        seL4_ReplyRecv(endpoint, info, NULL, reply);
    }
}

static int test_fault_handler_donated_sc(env_t env)
{
    helper_thread_t handler, faulter;
    void *vaddr = NULL;

    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    reservation_t res = vspace_reserve_range(&env->vspace, PAGE_SIZE_4K, seL4_AllRights, 1, &vaddr);
    test_check(vaddr != NULL);

    create_helper_thread(env, &handler);
    create_helper_thread(env, &faulter);

    /* start fault handler */
    start_helper(env, &handler, (helper_fn_t) ipc21_fault_handler_fn,
                 endpoint, (seL4_Word) &env->vspace, (seL4_Word) &res,
                 handler.thread.reply.cptr);

    /* wait for it to initialise */
    seL4_Wait(endpoint, NULL);

    /* now remove its scheduling context */
    int error = api_sc_unbind(handler.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* set fault handler */
    seL4_Word data = api_make_guard_skip_word(seL4_WordBits - env->cspace_size_bits);
    error = api_tcb_set_space(faulter.thread.tcb.cptr, endpoint,
                                env->cspace_root, data, env->page_directory, seL4_NilData);
    test_eq(error, seL4_NoError);

    /* start the fault handler */
    start_helper(env, &faulter, (helper_fn_t) ipc21_faulter_fn, (seL4_Word) vaddr, 0, 0, 0);
      /* the faulter handler will restore the faulter and we should not block here */
    wait_for_helper(&faulter);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0021, "Test fault handler on donated scheduling context",
            test_fault_handler_donated_sc, config_set(CONFIG_KERNEL_RT));

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
    api_nbsend_wait(init_ep, info, init_ep, NULL);

    ZF_LOGD("Server reply to fwded cap\n");
    seL4_Send(reply_cap, info);
}

static void
ipc22_stack_spawner_fn(env_t env, seL4_CPtr endpoint, int server_prio, seL4_Word unused)
{
    helper_thread_t servers[RUNS];
    seL4_CPtr init_ep = vka_alloc_endpoint_leaky(&env->vka);

    /* first we signal to endpoint to tell the test runner we are ready */
    seL4_CPtr first_ep = endpoint;
    ZF_LOGD("Stack spawner init");

    for (int i = 0; i < RUNS; i++) {
        create_helper_thread(env, &servers[i]);
        set_helper_priority(env, &servers[i], server_prio);

        api_nbsend_recv(first_ep, seL4_MessageInfo_new(0, 0, 0, 0), endpoint, NULL,
                    servers[i].thread.reply.cptr);

        /* after the first nbsend, we want to signal the clients via init_ep */
        first_ep = init_ep;

        ZF_LOGD("Got another client\n");
        /* start helper and allow to initialise */
        ZF_LOGD("Spawn server\n");
        start_helper(env, &servers[i], (helper_fn_t) ipc22_server_fn, init_ep,
                     servers[i].thread.reply.cptr, 0, 0);
        /* wait for it to block */
        seL4_Wait(init_ep, NULL);

        /*  now remove the schedling context */
        int error =  api_sc_unbind(servers[i].thread.sched_context.cptr);
        test_eq(error, seL4_NoError);
    }

    /* signal the last client */
    api_nbsend_wait(first_ep, seL4_MessageInfo_new(0, 0, 0, 0), endpoint, NULL);
}

static int test_stack_spawning_server(env_t env)
{
    helper_thread_t clients[RUNS];
    helper_thread_t stack_spawner;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    ipc22_go = vka_alloc_endpoint_leaky(&env->vka);
    volatile int state = 0;
    int our_prio = 10;

    create_helper_thread(env, &stack_spawner);
    set_helper_mcp(env, &stack_spawner, seL4_MaxPrio);
    start_helper(env, &stack_spawner, (helper_fn_t) ipc22_stack_spawner_fn,
                  (seL4_Word) env, endpoint, our_prio + 1, RUNS);

    /* wait for stack spawner to init */
    ZF_LOGD("Wait for stack spawner to init");
    seL4_Wait(endpoint, NULL);

    /* take away scheduling context */
    int error = api_sc_unbind(stack_spawner.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    set_helper_priority(env, &stack_spawner, our_prio + 2);

    error = seL4_TCB_SetPriority(env->tcb, env->tcb, our_prio);
    test_eq(error, seL4_NoError);

    set_helper_priority(env, &stack_spawner, our_prio + 2);
    set_helper_mcp(env, &stack_spawner, seL4_MaxPrio - 1);

    ZF_LOGD("Starting clients");
    /* create and start clients */
    for (int i = 0; i < RUNS; i++) {
        create_helper_thread(env, &clients[i]);
        set_helper_priority(env, &clients[i], our_prio + 1);
        start_helper(env, &clients[i], (helper_fn_t) ipc22_client_fn, endpoint, (seL4_Word) &state, i, 0);
    }

    /* set our priority down so servers can run */
    error = seL4_TCB_SetPriority(env->tcb, env->tcb, our_prio - 2);
    test_eq(error, seL4_NoError);

    for (int i = 0; i < RUNS; i++) {
        wait_for_helper(&clients[i]);
    }

    ZF_LOGD("Done");

    /* make sure all the clients got served */
    test_eq(state, RUNS);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0022, "Test stack spawning server with scheduling context donation",
test_stack_spawning_server, config_set(CONFIG_KERNEL_RT));

static void ipc23_client_fn(seL4_CPtr ep, volatile int *state)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);

    *state = 1;

    ZF_LOGD("Call");
    seL4_Call(ep, info);
    /* should not get here */
    *state = 2;
}

/* used by ipc0023 and 0024 */
static void ipc23_server_fn(seL4_CPtr client_ep, seL4_CPtr wait_ep, seL4_CPtr reply)
{
    /* send to the wait_ep to tell the test we are initialised,
     * then wait on the client_ep to receive a scheduling context */
    api_nbsend_recv(wait_ep, seL4_MessageInfo_new(0, 0, 0, 0), client_ep, NULL, reply);

    /* now block */
    seL4_Wait(wait_ep, NULL);
}

static int test_delete_reply_cap_sc(env_t env)
{
    helper_thread_t client, server;
    volatile int state = 0;
    seL4_CPtr client_ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr server_ep = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &client);
    create_helper_thread(env, &server);

    start_helper(env, &client, (helper_fn_t) ipc23_client_fn, client_ep,
                 (seL4_Word) &state, 0, 0);
    start_helper(env, &server, (helper_fn_t) ipc23_server_fn, client_ep,
                 server_ep, server.thread.reply.cptr, 0);

    /* wait for server to init */
    seL4_Wait(server_ep, NULL);
    /* take its scheduling context away */
    int error = api_sc_unbind(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    /* set the client and server prio higher than ours so they can run */
    error = seL4_TCB_SetPriority(env->tcb, env->tcb, 10);
    test_eq(error, seL4_NoError);

    set_helper_priority(env, &client, 11);
    set_helper_priority(env, &server, 11);

    /* now the client should have run and called the server*/
    test_eq(state, 1);

    /* delete scheduling context */
    vka_free_object(&env->vka,& client.thread.sched_context);

    /* try to reply, the client should not run as it has no scheduling context */
    seL4_Signal(server.thread.reply.cptr);
    test_eq(state, 1);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0023, "Test deleting the scheduling context tracked in a reply cap",
            test_delete_reply_cap_sc, config_set(CONFIG_KERNEL_RT))

static int test_delete_reply_cap_then_sc(env_t env)
{
    helper_thread_t client, server;
    volatile int state = 0;

    seL4_CPtr client_ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr server_ep = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &client);
    create_helper_thread(env, &server);

    set_helper_priority(env, &client, 11);
    set_helper_priority(env, &server, 11);
     /* start server */
    start_helper(env, &server, (helper_fn_t) ipc23_server_fn, client_ep,
                   server_ep, server.thread.reply.cptr, 0);

    ZF_LOGD("Waiting for server");
    /* wait for server to init */
    seL4_Wait(server_ep, NULL);

    /* set our prio down so client can run */
    int error = seL4_TCB_SetPriority(env->tcb, env->tcb, 10);
    test_eq(error, 0);

    ZF_LOGD("Removed sc\n");
    /* remove schedluing context */
    error = api_sc_unbind(server.thread.sched_context.cptr);
    test_eq(error, seL4_NoError);

    ZF_LOGD("Start client");
    /* start client */
    start_helper(env, &client, (helper_fn_t) ipc23_client_fn, client_ep,
                   (seL4_Word) &state, 0, 0);
    /* client should have started */
    test_eq(state, 1);

    ZF_LOGD("Steal reply cap ");
    /* nuke the reply cap */
    vka_free_object(&env->vka, &server.thread.reply);
    /* nuke the sc */
    vka_free_object(&env->vka, &client.thread.sched_context);

    ZF_LOGD("Done");
    /* caller should not run */
    test_eq(1, state);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0024, "Test deleting the reply cap in the scheduling context",
            test_delete_reply_cap_then_sc, config_set(CONFIG_KERNEL_RT));

static int test_nbsendrecv(env_t env)
{
    return test_ipc_pair(env, (test_func_t) nbsendrecv_func, (test_func_t) nbsendrecv_func, false, env->cores);
}
DEFINE_TEST(IPC0025, "Test seL4_nbsendrecv + seL4_nbsendrecv", test_nbsendrecv, config_set(CONFIG_KERNEL_RT))

static int test_nbsendrecv_interas(env_t env)
{
    return test_ipc_pair(env, (test_func_t) nbsendrecv_func, (test_func_t) nbsendrecv_func, false, env->cores);
}
DEFINE_TEST(IPC0026, "Test interas seL4_nbsendrecv + seL4_nbsendrecv", test_nbsendrecv_interas, config_set(CONFIG_KERNEL_RT))

static int
test_sched_donation_low_prio_server(env_t env)
{
    helper_thread_t client, server, server2;
    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &client);
    create_helper_thread(env, &server);
    create_helper_thread(env, &server2);

    int error = create_passive_thread(env, &server, (helper_fn_t) replywait_func, ep, 0, server.thread.reply.cptr, 0);
    test_eq(error, seL4_NoError);

    /* make client higher prio than server */
    set_helper_priority(env, &server, 1);
    set_helper_priority(env, &client, 2);

    ZF_LOGD("Start client");
    start_helper(env, &client, (helper_fn_t) call_func, ep, 0, 0, 0);

    ZF_LOGD("Wait for helper");
    wait_for_helper(&client);

    /* give server a sc to finish on */
    error = api_sc_bind(server.thread.sched_context.cptr,
                                        server.thread.tcb.cptr);
    test_eq(error, 0);
    wait_for_helper(&server);

    /* now try again, but start the client first */
    start_helper(env, &client, (helper_fn_t) call_func, ep, 0, 0, 0);
    error = create_passive_thread(env, &server2, (helper_fn_t) replywait_func, ep, 0,
                                  server2.thread.reply.cptr, 0);

    test_eq(error, seL4_NoError);

    ZF_LOGD("Wait for helper");
    wait_for_helper(&client);
    error = api_sc_bind(server2.thread.sched_context.cptr,
                                        server2.thread.tcb.cptr);
    test_eq(error, 0);
    wait_for_helper(&server2);

    return sel4test_get_result();
}
DEFINE_TEST(IPC0027, "Test sched donation to low prio server", test_sched_donation_low_prio_server, config_set(CONFIG_KERNEL_RT))

static void
ipc28_server_fn(seL4_CPtr ep, seL4_CPtr reply, volatile int *state)
{
    api_nbsend_recv(ep, seL4_MessageInfo_new(0, 0, 0, 0), ep, NULL, reply);
    while (1) {
        *state = *state + 1;
        seL4_ReplyRecv(ep, seL4_MessageInfo_new(0, 0, 0, 0), NULL, reply);
    }
}

static int
ipc28_client_fn(seL4_CPtr ep, volatile int *state) {

    while (*state < RUNS) {
        seL4_Call(ep, seL4_MessageInfo_new(0, 0, 0, 0));
        *state = *state + 1;
    }

    return 0;
}

static int
test_sched_donation_cross_core(env_t env)
{
    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);
    helper_thread_t clients[env->cores - 1];
    helper_thread_t server;
    volatile int states[env->cores - 1];
    volatile seL4_Word server_state = 0;

    /* start server on core 0 */
    create_helper_thread(env, &server);

    /* start a client on each other core */
    for (int i = 0; i < env->cores - 1; i++) {
        states[i] = 0;
        create_helper_thread(env, &clients[i]);
        set_helper_affinity(env, &clients[i], i + 1);
    }

    /* start server */
    start_helper(env, &server, (helper_fn_t) ipc28_server_fn, ep, get_helper_reply(&server),
                 (seL4_Word) &server_state, 0);

    /* wait for server to init */
    seL4_Wait(ep, NULL);

    /* convert to passive */
    int error = api_sc_unbind(get_helper_sched_context(&server));
    test_eq(error, seL4_NoError);

    /* start clients */
    for (int i = 0; i < env->cores - 1; i++) {
        start_helper(env, &clients[i], (helper_fn_t) ipc28_client_fn, ep, (seL4_Word) &states[i], 0, 0);
    }

    /* wait for the clients */
    for (int i = 0; i < env->cores - 1; i++) {
        error = wait_for_helper(&clients[i]);
        test_eq(error, 0);
        test_eq(states[i], RUNS);
    }

    test_eq(server_state, RUNS * (env->cores - 1));

    return sel4test_get_result();
}
DEFINE_TEST(IPC0028, "Cross core sched donation", test_sched_donation_cross_core, config_set(CONFIG_KERNEL_RT) && config_set(CONFIG_MAX_NUM_NODES) && CONFIG_MAX_NUM_NODES > 1);
#endif /* CONFIG_KERNEL_RT */
