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

#include <stdio.h>
#include <sel4/sel4.h>
#include <vka/object.h>
#include <vka/capops.h>

#include "../test.h"
#include "../helpers.h"

static int
send_func(seL4_CPtr ep, seL4_Word msg, UNUSED seL4_Word arg4, UNUSED seL4_Word arg3)
{
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);

    /* Send the given message once. */
    seL4_SetMR(0, msg);
    seL4_Send(ep, tag);

    return sel4test_get_result();
}

static int
test_nbwait(env_t env)
{

    vka_object_t notification = {0};
    vka_object_t endpoint = {0};
    vka_object_t reply = {0};
    int error;
    seL4_MessageInfo_t info = {{0}};
    seL4_Word badge = 0;

    /* allocate an endpoint and a notification */
    error = vka_alloc_endpoint(&env->vka, &endpoint);
    test_eq(error, 0);

    error = vka_alloc_notification(&env->vka, &notification);
    test_eq(error, 0);

    if (config_set(CONFIG_KERNEL_RT)) {
        error = vka_alloc_reply(&env->vka, &reply);
        test_eq(error, 0);
    }

    /* bind the notification to the current thread */
    error = seL4_TCB_BindNotification(env->tcb, notification.cptr);
    test_eq(error, 0);

    /* create cspace paths for both objects */
    cspacepath_t notification_path, endpoint_path;
    vka_cspace_make_path(&env->vka, notification.cptr, &notification_path);
    vka_cspace_make_path(&env->vka, endpoint.cptr, &endpoint_path);

    /* badge both caps */
    cspacepath_t badged_notification, badged_endpoint;
    error = vka_cspace_alloc_path(&env->vka, &badged_notification);
    test_eq(error, 0);
    vka_cspace_alloc_path(&env->vka, &badged_endpoint);
    test_eq(error, 0);

    error = vka_cnode_mint(&badged_endpoint, &endpoint_path, seL4_AllRights, 1);
    test_eq(error, 0);

    error = vka_cnode_mint(&badged_notification, &notification_path, seL4_AllRights, 2);
    test_eq(error, 0);

    /* NBRecv on endpoint with no messages should return 0 immediately */
    info = api_nbrecv(endpoint.cptr, &badge, reply.cptr);
    test_eq(badge, (seL4_Word)0);

    /* Poll on a notification with no messages should return 0 immediately */
    info = seL4_Poll(notification.cptr, &badge);
    test_eq(badge, (seL4_Word)0);

    /* send a signal to the notification */
    seL4_Signal(badged_notification.capPtr);

    /* Polling should return the badge from the signal we just sent */
    info = seL4_Poll(notification.cptr, &badge);
    test_eq(badge, (seL4_Word)2);

    /* Polling again should return nothign */
    info = seL4_Poll(notification.cptr, &badge);
    test_eq(badge, (seL4_Word)0);

    /* send a signal to the notification */
    seL4_Signal(badged_notification.capPtr);

    /* This time NBRecv the endpoint - since we are bound, we should get the badge from the signal again */
    info = api_nbrecv(endpoint.cptr, &badge, reply.cptr);
    test_eq(badge, (seL4_Word)2);

    /* NBRecving again should return nothign */
    info = api_nbrecv(endpoint.cptr, &badge, reply.cptr);
    test_eq(badge, (seL4_Word)0);

    /* now start a helper to send a message on the badged endpoint */
    helper_thread_t thread;
    create_helper_thread(env, &thread);
    start_helper(env, &thread, send_func, badged_endpoint.capPtr, 0xdeadbeef, 0, 0);
    set_helper_priority(env, &thread, env->priority);
    /* let helper run */
    seL4_TCB_SetPriority(env->tcb, env->tcb, env->priority - 1);

    /* NBRecving should return helpers message */
    info = api_nbrecv(endpoint.cptr, &badge, reply.cptr);
    test_eq(badge, (seL4_Word)1);
    test_eq(seL4_MessageInfo_get_length(info), (seL4_Word)1);
    test_eq(seL4_GetMR(0), (seL4_Word)0xdeadbeef);

    /* NBRecving again should return nothign */
    info = api_nbrecv(endpoint.cptr, &badge, reply.cptr);
    test_eq(badge, (seL4_Word)0);

    /* clean up */
    wait_for_helper(&thread);
    cleanup_helper(env, &thread);
    seL4_TCB_UnbindNotification(env->tcb);
    vka_cnode_delete(&badged_notification);
    vka_cnode_delete(&badged_endpoint);
    vka_cspace_free(&env->vka, badged_endpoint.capPtr);
    vka_cspace_free(&env->vka, badged_notification.capPtr);
    vka_free_object(&env->vka, &endpoint);
    vka_free_object(&env->vka, &notification);

    return sel4test_get_result();
}
DEFINE_TEST(NBWAIT0001, "Test seL4_NBRecv", test_nbwait, true)
