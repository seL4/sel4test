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
DEFINE_TEST(IPC0001, "Test seL4_Send + seL4_Recv", test_send_wait)

static int
test_call_replywait(env_t env)
{
    return test_ipc_pair(env, call_func, replywait_func, false, env->cores);
}
DEFINE_TEST(IPC0002, "Test seL4_Call + seL4_ReplyRecv", test_call_replywait)

static int
test_call_reply_and_wait(env_t env)
{
    return test_ipc_pair(env, call_func, reply_and_wait_func, false, env->cores);
}
DEFINE_TEST(IPC0003, "Test seL4_Send + seL4_Reply + seL4_Recv", test_call_reply_and_wait)

static int
test_nbsend_wait(env_t env)
{
    return test_ipc_pair(env, nbsend_func, nbwait_func, false, 1);
}
DEFINE_TEST(IPC0004, "Test seL4_NBSend + seL4_Recv", test_nbsend_wait)

static int
test_send_wait_interas(env_t env)
{
    return test_ipc_pair(env, send_func, wait_func, true, env->cores);
}
DEFINE_TEST(IPC1001, "Test inter-AS seL4_Send + seL4_Recv", test_send_wait_interas)

static int
test_call_replywait_interas(env_t env)
{
    return test_ipc_pair(env, call_func, replywait_func, true, env->cores);
}
DEFINE_TEST(IPC1002, "Test inter-AS seL4_Call + seL4_ReplyRecv", test_call_replywait_interas)

static int
test_call_reply_and_wait_interas(env_t env)
{
    return test_ipc_pair(env, call_func, reply_and_wait_func, true, env->cores);
}
DEFINE_TEST(IPC1003, "Test inter-AS seL4_Send + seL4_Reply + seL4_Recv", test_call_reply_and_wait_interas)

static int
test_nbsend_wait_interas(env_t env)
{
    return test_ipc_pair(env, nbsend_func, nbwait_func, true, 1);
}
DEFINE_TEST(IPC1004, "Test inter-AS seL4_NBSend + seL4_Recv", test_nbsend_wait_interas)

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
DEFINE_TEST(IPC0010, "Test suspending an IPC mid-Call()", test_ipc_abort_in_call)
