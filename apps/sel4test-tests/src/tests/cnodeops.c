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
#include <sel4/sel4.h>
#include <vka/vka.h>
#include <vka/object.h>

#include "../test.h"
#include "../helpers.h"

/* Get a cap that we can move/copy/delete/etc and compare without causing chaos.
 */
static seL4_CPtr
get_cap(vka_t *vka)
{
    return vka_alloc_tcb_leaky(vka);
}
static int
test_cnode_copy(env_t env)
{
    int error;
    seL4_Word src, dest;

    /* A call that should succeed. */
    src = get_cap(&env->vka);
    dest = get_free_slot(env);
    test_assert(is_slot_empty(env, dest));
    error = cnode_copy(env, src, dest, seL4_AllRights);
    test_assert(!error);
    test_assert(are_tcbs_distinct(src, dest) == 0);

    /* Copy to an occupied slot (should fail). */
    src = get_cap(&env->vka);
    dest = get_cap(&env->vka);
    error = cnode_copy(env, src, dest, seL4_AllRights);
    test_assert(error == seL4_DeleteFirst);

    /* Copy from a free slot to an occupied slot (should fail). */
    src = get_free_slot(env);
    test_assert(is_slot_empty(env, src));
    dest = get_cap(&env->vka);
    error = cnode_copy(env, src, dest, seL4_AllRights);
    test_assert(error == seL4_DeleteFirst);

    /* Copy from a free slot to a free slot (should fail). */
    src = get_free_slot(env);
    test_assert(is_slot_empty(env, src));
    dest = get_free_slot(env);
    test_assert(is_slot_empty(env, dest));
    error = cnode_copy(env, src, dest, seL4_AllRights);
    test_assert(error == seL4_FailedLookup);

    return sel4test_get_result();
}
DEFINE_TEST(CNODEOP0001, "Basic seL4_CNode_Copy() testing", test_cnode_copy)

static int
test_cnode_delete(env_t env)
{
    int error;
    seL4_Word slot;

    /* A call that should succeed. */
    slot = get_cap(&env->vka);
    error = cnode_delete(env, slot);
    test_assert(!error);
    test_assert(is_slot_empty(env, slot));

    /* Deleting a free slot (should succeed). */
    slot = get_free_slot(env);
    error = cnode_delete(env, slot);
    test_assert(!error);
    test_assert(is_slot_empty(env, slot));

    return sel4test_get_result();
}
DEFINE_TEST(CNODEOP0002, "Basic seL4_CNode_Delete() testing", test_cnode_delete)

static int
test_cnode_mint(env_t env)
{
    int error;
    seL4_Word src, dest;

    /* A call that should succeed. */
    src = get_cap(&env->vka);
    dest = get_free_slot(env);
    error = cnode_mint(env, src, dest, seL4_AllRights, seL4_NilData);
    test_assert(!error);
    test_assert(are_tcbs_distinct(src, dest) == 0);

    /* Mint to an occupied slot (should fail). */
    src = get_cap(&env->vka);
    dest = get_cap(&env->vka);
    error = cnode_mint(env, src, dest, seL4_AllRights, seL4_NilData);
    test_assert(error == seL4_DeleteFirst);

    /* Mint from an empty slot (should fail). */
    src = get_free_slot(env);
    dest = get_free_slot(env);
    error = cnode_mint(env, src, dest, seL4_AllRights, seL4_NilData);
    test_assert(error == seL4_FailedLookup);

    return sel4test_get_result();
}
DEFINE_TEST(CNODEOP0003, "Basic seL4_CNode_Mint() testing", test_cnode_mint)

static int
test_cnode_move(env_t env)
{
    int error;
    seL4_Word src, dest;

    /* A call that should succeed. */
    src = get_cap(&env->vka);
    dest = get_free_slot(env);
    error = cnode_move(env, src, dest);
    test_assert(!error);
    test_assert(is_slot_empty(env, src));
    test_assert(!is_slot_empty(env, dest));

    /* Move from an empty slot (should fail). */
    src = get_free_slot(env);
    dest = get_free_slot(env);
    error = cnode_move(env, src, dest);
    test_assert(error = seL4_FailedLookup);
    test_assert(is_slot_empty(env, dest));

    /* Move to an occupied slot (should fail). */
    src = get_cap(&env->vka);
    dest = get_cap(&env->vka);
    error = cnode_move(env, src, dest);
    test_assert(error == seL4_DeleteFirst);
    test_assert(!is_slot_empty(env, src));
    test_assert(!is_slot_empty(env, dest));

    return sel4test_get_result();
}
DEFINE_TEST(CNODEOP0004, "Basic seL4_CNode_Move() testing", test_cnode_move)

static int
test_cnode_mutate(env_t env)
{
    int error;
    seL4_Word src, dest;

    /* A call that should succeed. */
    src = get_cap(&env->vka);
    dest = get_free_slot(env);
    error = cnode_mutate(env, src, dest);
    test_assert(!error);
    test_assert(is_slot_empty(env, src));
    test_assert(!is_slot_empty(env, dest));

    /* Mutating to an occupied slot (should fail). */
    src = get_cap(&env->vka);
    dest = get_cap(&env->vka);
    error = cnode_mutate(env, src, dest);
    test_assert(error == seL4_DeleteFirst);
    test_assert(!is_slot_empty(env, src));
    test_assert(!is_slot_empty(env, dest));

    /* Mutating an empty slot (should fail). */
    src = get_free_slot(env);
    dest = get_free_slot(env);
    error = cnode_mutate(env, src, dest);
    test_assert(error == seL4_FailedLookup);
    test_assert(is_slot_empty(env, src));
    test_assert(is_slot_empty(env, dest));

    return sel4test_get_result();
}
DEFINE_TEST(CNODEOP0005, "Basic seL4_CNode_Mutate() testing", test_cnode_mutate)

static int
test_cnode_recycle(env_t env)
{
    int error;
    seL4_Word slot;

    /* A call that should succeed. */
    slot = get_cap(&env->vka);
    error = cnode_recycle(env, slot);
    test_assert(!error);
    test_assert(!is_slot_empty(env, slot));

    /* Recycling an empty slot (should fail). */
    slot = get_free_slot(env);
    error = cnode_recycle(env, slot);
    test_assert(error == seL4_IllegalOperation);
    test_assert(is_slot_empty(env, slot));

    return sel4test_get_result();
}
DEFINE_TEST(CNODEOP0006, "Basic seL4_CNode_Recycle() testing", test_cnode_recycle)

static int
test_cnode_revoke(env_t env)
{
    int error;
    seL4_Word slot;

    /* A call that should succeed. */
    slot = get_cap(&env->vka);
    error = cnode_revoke(env, slot);
    test_assert(!error);
    test_assert(!is_slot_empty(env, slot));

    /* Revoking a null cap (should fail). */
    slot = get_free_slot(env);
    error = cnode_revoke(env, slot);
    test_assert(!error);
    test_assert(is_slot_empty(env, slot));

    return sel4test_get_result();
}
DEFINE_TEST(CNODEOP0007, "Basic seL4_CNode_Revoke() testing", test_cnode_revoke)

static int
test_cnode_rotate(env_t env)
{
    int error;
    seL4_Word src, pivot, dest;

    /* A call that should succeed. */
    src = get_cap(&env->vka);
    pivot = get_cap(&env->vka);
    dest = get_free_slot(env);
    error = cnode_rotate(env, src, pivot, dest);
    test_assert(!error);
    test_assert(is_slot_empty(env, src));
    test_assert(!is_slot_empty(env, pivot));
    test_assert(!is_slot_empty(env, dest));

    /* Destination occupied (should fail). */
    src = get_cap(&env->vka);
    pivot = get_cap(&env->vka);
    dest = get_cap(&env->vka);
    error = cnode_rotate(env, src, pivot, dest);
    test_assert(error == seL4_DeleteFirst);
    test_assert(!is_slot_empty(env, src));
    test_assert(!is_slot_empty(env, pivot));
    test_assert(!is_slot_empty(env, dest));

    /* Swapping two caps (should succeed). */
    src = get_cap(&env->vka);
    pivot = get_cap(&env->vka);
    dest = src;
    error = cnode_rotate(env, src, pivot, dest);
    test_assert(!error);
    test_assert(are_tcbs_distinct(src, dest) == 0);
    test_assert(!is_slot_empty(env, pivot));

    /* Moving a cap onto itself (should fail). */
    src = get_cap(&env->vka);
    pivot = src;
    dest = get_free_slot(env);
    error = cnode_rotate(env, src, pivot, dest);
    test_assert(error == seL4_IllegalOperation);
    test_assert(!is_slot_empty(env, src));
    test_assert(is_slot_empty(env, dest));

    /* Moving empty slots (should fail). */
    src = get_free_slot(env);
    pivot = get_free_slot(env);
    dest = get_free_slot(env);
    error = cnode_rotate(env, src, pivot, dest);
    test_assert(error == seL4_FailedLookup);
    test_assert(is_slot_empty(env, src));
    test_assert(is_slot_empty(env, pivot));
    test_assert(is_slot_empty(env, dest));

    return sel4test_get_result();
}
DEFINE_TEST(CNODEOP0008, "Basic seL4_CNode_Rotate() testing", test_cnode_rotate)

static NORETURN int 
call_fn(seL4_CPtr endpoint) 
{
    while (1) {
        seL4_Call(endpoint, seL4_MessageInfo_new(0, 0, 0, 0));
    }
}

static int
test_cnode_swapcaller(env_t env)
{
    int error;
    seL4_Word slot;
    vka_object_t endpoint = {0};

    error = vka_alloc_endpoint(&env->vka, &endpoint);
    test_eq(error, 0);

    /* A call that should succeed. */
    slot = get_free_slot(env);
    error = cnode_swapcaller(env, slot);
    test_assert(!error);

    /* Swap to an occupied slot (should fail). */
    slot = get_cap(&env->vka);
    error = cnode_swapcaller(env, slot);
    test_assert(error == seL4_DeleteFirst);
    test_assert(!is_slot_empty(env, slot));
    /* clean the slot */
    cnode_delete(env, slot);

    /* now set up a helper thread to call us and we'll try saving our reply cap */
    helper_thread_t helper;
    create_helper_thread(env, &helper);
    start_helper(env, &helper, (helper_fn_t) call_fn, endpoint.cptr, 0, 0, 0);

    /* let helper run */
    ZF_LOGD("Wait"); 
    seL4_Wait(endpoint.cptr, NULL);
    ZF_LOGD("Back");

    /* first swap it to a full slot */
    error = cnode_swapcaller(env, endpoint.cptr);
    test_eq(error, seL4_DeleteFirst);

    /* now swap it properly (should suceed) */
    error = cnode_swapcaller(env, slot);
    test_eq(error, seL4_NoError);

    /* now reply to it */
    seL4_Send(slot, seL4_MessageInfo_new(0, 0, 0, 0));
    
    /* and wait for the caller to respond so we know it worked */
    seL4_Wait(endpoint.cptr, NULL);
    seL4_Reply(seL4_MessageInfo_new(0, 0, 0, 0));

    ZF_LOGD("Set up second helper thread");
    helper_thread_t second;
    create_helper_thread(env, &second);
    start_helper(env, &second, (helper_fn_t) call_fn, endpoint.cptr, 0, 0, 0);

    ZF_LOGD("Let a helper run");
    seL4_Recv(endpoint.cptr, NULL);

    ZF_LOGD("Save the reply cap caller");
    error = cnode_swapcaller(env, slot);
    test_eq(error, seL4_NoError);

    ZF_LOGD("Let other helper run");
    seL4_Recv(endpoint.cptr, NULL);
    
    ZF_LOGD("Swap in previously saved reply cap");
    error = cnode_swapcaller(env, slot);
    test_eq(error, seL4_NoError);

    ZF_LOGD("reply to swapped in client");
    seL4_Reply(seL4_MessageInfo_new(0, 0, 0, 0));

    ZF_LOGD("now swap the other one back in");
    error = cnode_swapcaller(env, slot);
    test_eq(error, seL4_NoError);
    seL4_Reply(seL4_MessageInfo_new(0, 0, 0, 0));

    /* check both clients are alive */
    ZF_LOGD("Wait for one helper");
    seL4_Recv(endpoint.cptr, NULL);
    ZF_LOGD("Wait for second helper");
    seL4_Recv(endpoint.cptr, NULL);

    /* finally clean up */
    cleanup_helper(env, &helper);
    cleanup_helper(env, &second);
    vka_free_object(&env->vka, &endpoint);

    return SUCCESS;
}
DEFINE_TEST(CNODEOP0009, "seL4_CNode_SwapCaller() testing", test_cnode_swapcaller)

static int
test_cnode_swapTCBcaller(env_t env)
{
    vka_object_t endpoint = {0};
    int error;
    seL4_Word slot;

    /* first test saving the reply cap of something that isn't a tcb */
    error = vka_alloc_endpoint(&env->vka, &endpoint);
    test_eq(error, 0);

    slot = get_free_slot(env);
    test_neq(slot, 0);

    error = cnode_swapTCBcaller(env, slot, &endpoint);
    test_eq(error, seL4_InvalidArgument);

    /* now set up a helper thread to call us and we'll try saving its reply cap */
    helper_thread_t helper;
    create_helper_thread(env, &helper);
    start_helper(env, &helper, (helper_fn_t) call_fn, endpoint.cptr, 0, 0, 0);

    /* let helper run */
    ZF_LOGD("Wait"); 
    seL4_Wait(endpoint.cptr, NULL);
    ZF_LOGD("Back");

    cspacepath_t path;

    /* first swap it to a full slot */
    vka_cspace_make_path(&env->vka, endpoint.cptr, &path);
    error = seL4_CNode_SwapTCBCaller(path.root, path.capPtr, path.capDepth, env->tcb);
    test_eq(error, seL4_DeleteFirst);

    /* now swap it properly (should suceed) */
    vka_cspace_make_path(&env->vka, slot, &path);
    error = seL4_CNode_SwapTCBCaller(path.root, path.capPtr, path.capDepth, env->tcb);
    test_eq(error, seL4_NoError);

    /* now reply to it */
    seL4_Send(slot, seL4_MessageInfo_new(0, 0, 0, 0));
    
    /* and wait for the caller to respond so we know it worked */
    seL4_Wait(endpoint.cptr, NULL);

    /* finally clean up */
    cleanup_helper(env, &helper);
    vka_free_object(&env->vka, &endpoint);

    return sel4test_get_result();
}
DEFINE_TEST(CNODEOP0010, "seL4_CNode_SwapTCBCaller() testing", test_cnode_swapTCBcaller)
