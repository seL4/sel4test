/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <vka/object.h>

#include "../helpers.h"


#define MAGIC1 42
#define MAGIC2 0xDEADBEEF
#define MAGIC3 666

static int check_recv(seL4_CPtr ep, seL4_Word val, seL4_CPtr reply)
{
    api_recv(ep, NULL, reply);
    test_check(val == seL4_GetMR(0));
    // This one is just for Rendez-vous
    api_recv(ep, NULL, reply);
    return sel4test_get_result();
}

static int test_send_needs_write(env_t env)
{
    vka_t *vka = &env->vka;
    seL4_CPtr ep = vka_alloc_endpoint_leaky(vka);
    seL4_CPtr reply = vka_alloc_reply_leaky(vka);
    seL4_CPtr epMint;
    vka_cspace_alloc(vka, &epMint);
    helper_thread_t t;
    seL4_CapRights_t rights;
    for (seL4_Word i = 0 ; i < BIT(seL4_MsgExtraCapBits) ; i++) {
        rights.words[0] = i;
        create_helper_thread(env, &t);
        start_helper(env, &t, (helper_fn_t)check_recv, ep, MAGIC1, reply, 0);
        cnode_mint(env, ep, epMint, rights, 0);

        seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);
        if (!seL4_CapRights_get_capAllowWrite(rights)) {
            seL4_SetMR(0, MAGIC2);
            seL4_Send(epMint, tag);
        }
        seL4_SetMR(0, MAGIC1);
        seL4_Send(ep, tag);
        // This one is just for Rendez-vous
        seL4_Send(ep, tag);
        cleanup_helper(env, &t);
        cnode_delete(env, epMint);

    }
    return sel4test_get_result();
}

DEFINE_TEST(IPCRIGHTS0001, "seL4_Send needs write", test_send_needs_write, true)


static int
test_recv_needs_read(env_t env)
{
    vka_t *vka = &env->vka;
    seL4_CPtr ep = vka_alloc_endpoint_leaky(vka);
    seL4_CPtr reply = vka_alloc_reply_leaky(vka);
    seL4_CPtr fault_ep = vka_alloc_endpoint_leaky(vka);
    seL4_CPtr fault_reply = vka_alloc_reply_leaky(vka);
    seL4_CPtr epMint;
    vka_cspace_alloc(vka, &epMint);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);
    for (seL4_Word i = 0 ; i < BIT(seL4_MsgExtraCapBits) ; i++) {
        seL4_CapRights_t rights;
        rights.words[0] = i;
        cnode_mint(env, ep, epMint, rights, 0);
        helper_thread_t t;
        create_helper_thread(env, &t);
        int error;
        error = api_tcb_set_space(get_helper_tcb(&t),
                                  fault_ep,
                                  env->cspace_root,
                                  seL4_NilData,
                                  env->page_directory, seL4_NilData);
        test_error_eq(error, seL4_NoError);
        start_helper(env, &t, (helper_fn_t)check_recv, epMint, MAGIC1, reply, 0);

        if (seL4_CapRights_get_capAllowRead(rights)) {
            seL4_SetMR(0, MAGIC1);
            seL4_Send(ep, tag);
            seL4_Send(ep, tag);
        } else {
            api_recv(fault_ep, NULL, fault_reply);
        }
        cleanup_helper(env, &t);
        cnode_delete(env, epMint);
    }

    return sel4test_get_result();
}

DEFINE_TEST(IPCRIGHTS0002, "seL4_Recv needs read", test_recv_needs_read, true)

static int
check_recv_cap(env_t env, seL4_CPtr ep, bool should_recv_cap, seL4_CPtr reply)
{
    vka_t *vka = &env->vka;

    seL4_CPtr recvSlot;
    int vka_error = vka_cspace_alloc(vka, &recvSlot);
    test_error_eq(vka_error, seL4_NoError);
    set_cap_receive_path(env, recvSlot);
    api_recv(ep, NULL, reply);

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);

    test_assert(seL4_GetMR(0) == MAGIC1);

    if (should_recv_cap) {
        test_assert(!is_slot_empty(env, recvSlot));
        seL4_SetMR(0, MAGIC2);
        seL4_Send(recvSlot, tag);
        int error = cnode_delete(env, recvSlot);
        test_check(!error);
    }
    test_assert(is_slot_empty(env, recvSlot));
    vka_cspace_free(vka, recvSlot);
    seL4_SetMR(0, MAGIC3);
    seL4_Send(ep, tag);

    return sel4test_get_result();
}

static int test_send_cap_needs_grant(env_t env)
{
    vka_t *vka = &env->vka;
    seL4_CPtr ep = vka_alloc_endpoint_leaky(vka);
    seL4_CPtr reply = vka_alloc_reply_leaky(vka);
    seL4_CPtr return_ep = vka_alloc_endpoint_leaky(vka);
    seL4_CPtr return_reply = vka_alloc_reply_leaky(vka);
    seL4_CPtr epMint;
    vka_cspace_alloc(vka, &epMint);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 1, 1);

    for (seL4_Word i = 0; i < BIT(seL4_MsgExtraCapBits); i++) {
        seL4_CapRights_t rights;
        rights.words[0] = i;
        if (!seL4_CapRights_get_capAllowWrite(rights)) {
            continue;
        }
        bool grant = seL4_CapRights_get_capAllowGrant(rights);

        cnode_mint(env, ep, epMint, rights, 0);

        helper_thread_t t;
        create_helper_thread(env, &t);
        start_helper(env, &t, (helper_fn_t)check_recv_cap, (seL4_Word)env, ep, grant, reply);

        seL4_SetMR(0, MAGIC1);
        seL4_SetCap(0, return_ep);
        seL4_Send(epMint, tag);

        if (grant) {
            api_recv(return_ep, NULL, return_reply);
            test_check(seL4_GetMR(0) == MAGIC2);
        }
        api_recv(ep, NULL, reply);
        test_check(seL4_GetMR(0) == MAGIC3);
        cleanup_helper(env, &t);
        cnode_delete(env, epMint);
    }

    /* test_check(seL4_GetMR(0) == 42); */
    return sel4test_get_result();
}

DEFINE_TEST(IPCRIGHTS0003, "seL4_Send with caps needs grant", test_send_cap_needs_grant, true)

#ifndef CONFIG_KERNEL_MCS

static int
check_call(env_t env, seL4_CPtr ep, bool should_call, bool reply_recv)
{
    vka_t *vka = &env->vka;


    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, MAGIC1);
    seL4_Call(ep, tag);
    if (reply_recv) {
        seL4_Call(ep, tag);
    }


    /* if we are here, the call should have worked (or else we would have been inactive) */
    test_assert(should_call);
    test_assert(seL4_GetMR(0) == MAGIC2);
    seL4_Send(ep, tag);

    return sel4test_get_result();
}



static int test_call_needs_grant_or_grant_reply(env_t env)
{
    vka_t *vka = &env->vka;
    seL4_CPtr ep = vka_alloc_endpoint_leaky(vka);
    seL4_CPtr return_ep = vka_alloc_endpoint_leaky(vka);
    seL4_CPtr reply = get_free_slot(env);
    seL4_CPtr epMint = get_free_slot(env);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);

    enum {
        nothing,
        save_caller,
        reply_recv
    };

    for (int mode = nothing; mode <= reply_recv; mode++) {
        for (seL4_Word i = 0; i < BIT(seL4_MsgExtraCapBits); i++) {
            seL4_CapRights_t rights;
            rights.words[0] = i;
            if (!seL4_CapRights_get_capAllowWrite(rights)) {
                continue;
            }
            bool grant = seL4_CapRights_get_capAllowGrant(rights);
            bool grant_reply = seL4_CapRights_get_capAllowGrantReply(rights);

            cnode_copy(env, ep, epMint, rights);

            helper_thread_t t;
            create_helper_thread(env, &t);
            start_helper(env, &t, (helper_fn_t)check_call, (seL4_Word)env, epMint,
                         grant || grant_reply, mode == reply_recv);


            seL4_Recv(ep, NULL);
            if (grant || grant_reply) {
                test_check(seL4_GetMR(0) == MAGIC1);
                switch (mode) {
                case nothing:
                    seL4_SetMR(0, MAGIC2);
                    seL4_Reply(tag);
                    cnode_savecaller(env, reply);
                    test_assert(is_slot_empty(env, reply));
                    break;
                case save_caller:
                    cnode_savecaller(env, reply);
                    test_assert(!is_slot_empty(env, reply));
                    seL4_SetMR(0, MAGIC2);
                    seL4_Send(reply, tag);
                    test_assert(is_slot_empty(env, reply));
                    break;
                case reply_recv:
                    seL4_ReplyRecv(ep, tag, NULL);
                    seL4_SetMR(0, MAGIC2);
                    seL4_Reply(tag);
                    cnode_savecaller(env, reply);
                    test_assert(is_slot_empty(env, reply));
                    break;
                }
                seL4_Recv(ep, NULL);
                test_check(seL4_GetMR(0) == MAGIC2);
            } else {
                cnode_savecaller(env, reply);
                test_assert(is_slot_empty(env, reply));
                cnode_delete(env, epMint);
                cnode_copy(env, ep, epMint, seL4_AllRights);
                seL4_TCB_Resume(get_helper_tcb(&t));
                seL4_Recv(ep, NULL);
                cnode_savecaller(env, reply);
                test_assert(!is_slot_empty(env, reply));
                seL4_TCB_Suspend(get_helper_tcb(&t));
                test_assert(is_slot_empty(env, reply));
            }
            cleanup_helper(env, &t);
            cnode_delete(env, epMint);
        }
    }

    return sel4test_get_result();
}

DEFINE_TEST(IPCRIGHTS0004, "seL4_Call needs grant or grant-reply",
            test_call_needs_grant_or_grant_reply, true)

static int
check_call_return_cap(env_t env, seL4_CPtr ep,
                      bool should_recv_cap, bool reply_recv)
{
    vka_t *vka = &env->vka;

    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_CPtr recvSlot = get_free_slot(env);
    set_cap_receive_path(env, recvSlot);
    seL4_SetMR(0, MAGIC1);
    seL4_Call(ep, tag);
    if (reply_recv) {
        seL4_Call(ep, tag);
    }

    test_check(seL4_GetMR(0) == MAGIC2);

    if (should_recv_cap) {
        test_assert(!is_slot_empty(env, recvSlot));
        seL4_SetMR(0, MAGIC3);
        seL4_Send(recvSlot, tag);
        int error = cnode_delete(env, recvSlot);
        test_check(!error);
    }
    test_assert(is_slot_empty(env, recvSlot));
    vka_cspace_free(vka, recvSlot);
    seL4_SetMR(0, MAGIC3);
    seL4_Send(ep, tag);
    return sel4test_get_result();
}

static int test_reply_grant_receiver(env_t env)
{
    vka_t *vka = &env->vka;
    seL4_CPtr ep = vka_alloc_endpoint_leaky(vka);
    seL4_CPtr return_ep = vka_alloc_endpoint_leaky(vka);
    seL4_CPtr reply = get_free_slot(env);
    seL4_CPtr epMint = get_free_slot(env);
    seL4_MessageInfo_t tag_no_cap = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 1, 1);

    enum {
        nothing,
        save_caller,
        reply_recv
    };

    for (int mode = nothing; mode <= reply_recv; mode++) {
        for (seL4_Word i = 0; i < BIT(seL4_MsgExtraCapBits); i++) {
            seL4_CapRights_t rights;
            rights.words[0] = i;
            if (!seL4_CapRights_get_capAllowRead(rights)) {
                continue;
            }
            bool grant = seL4_CapRights_get_capAllowGrant(rights);

            cnode_copy(env, ep, epMint, rights);

            helper_thread_t t;
            create_helper_thread(env, &t);
            start_helper(env, &t, (helper_fn_t)check_call_return_cap,
                         (seL4_Word)env, ep, grant, mode == reply_recv);

            seL4_Recv(epMint, NULL);
            seL4_Word callmsg = seL4_GetMR(0);
            test_check(callmsg == MAGIC1);
            if (mode == save_caller) {
                cnode_savecaller(env, reply);
                test_assert(!is_slot_empty(env, reply));
                seL4_SetMR(0, MAGIC2);
                seL4_SetCap(0, return_ep);
                seL4_Send(reply, tag);
                test_assert(is_slot_empty(env, reply));
            } else {
                if (mode == reply_recv) {
                    seL4_ReplyRecv(epMint, tag_no_cap, NULL);
                }
                seL4_SetMR(0, MAGIC2);
                seL4_SetCap(0, return_ep);
                seL4_Reply(tag);
            }

            if (grant) {
                seL4_Recv(return_ep, NULL);
                test_check(seL4_GetMR(0) == MAGIC3);
            }
            seL4_Recv(ep, NULL);
            test_check(seL4_GetMR(0) == MAGIC3);
            cleanup_helper(env, &t);
            cnode_delete(env, epMint);
        }
    }

    return sel4test_get_result();
}

DEFINE_TEST(IPCRIGHTS0005, "seL4_Reply grant depends of the grant of previous seL4_Recv",
            test_reply_grant_receiver, true)




#endif /* CONFIG_KERNEL_MCS */

