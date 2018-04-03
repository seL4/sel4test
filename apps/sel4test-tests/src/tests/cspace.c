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
#include <sel4/sel4.h>
#include <vka/object.h>

#include "../test.h"
#include "../helpers.h"
#include <utils/util.h>

#define READY_MAGIC 0x12374153
#define SUCCESS_MAGIC 0x12374151

static int
ipc_caller(seL4_Word ep0, seL4_Word ep1, seL4_Word word_bits, seL4_Word arg4)
{
    /* Let our parent know we are ready. */
    seL4_MessageInfo_t tag = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, READY_MAGIC);
    seL4_Send(ep0, tag);
    /*
     * The parent has changed our cspace on us. Check that it makes sense.
     *
     * Basically the entire cspace should be empty except for the cap at ep0.
     * We should still test that various points in the cspace resolve correctly.
     */

    /* Check that none of the typical endpoints are valid. */
    for (unsigned long i = 0; i < word_bits; i++) {
        seL4_MessageInfo_ptr_new(&tag, 0, 0, 0, 0);
        tag = seL4_Call(i, tag);
        test_assert(seL4_MessageInfo_get_label(tag) == seL4_InvalidCapability);
    }

    /* Check that changing one bit still gives an invalid cap. */
    for (unsigned long i = 0; i < word_bits; i++) {
        seL4_MessageInfo_ptr_new(&tag, 0, 0, 0, 0);
        tag = seL4_Call(ep1 ^ BIT(i), tag);
        test_assert(seL4_MessageInfo_get_label(tag) == seL4_InvalidCapability);
    }

    /* And we're done. This should be a valid cap and get us out of here! */
    seL4_MessageInfo_ptr_new(&tag, 0, 0, 0, 1);
    seL4_SetMR(0, SUCCESS_MAGIC);
    seL4_Send(ep1, tag);

    return sel4test_get_result();
}

static int
test_full_cspace(env_t env)
{
    int error;
    seL4_CPtr cnode[CONFIG_WORD_SIZE];
    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_Word ep_pos = 1;

    /* Create 32 or 64 cnodes, each resolving one bit. */
    for (unsigned int i = 0; i < CONFIG_WORD_SIZE; i++) {
        cnode[i] = vka_alloc_cnode_object_leaky(&env->vka, 1);
        assert(cnode[i]);
    }

    /* Copy cnode i to alternating slots in cnode i-1. */
    seL4_Word slot = 0;
    for (unsigned int i = 1; i < CONFIG_WORD_SIZE; i++) {
        error = seL4_CNode_Copy(
                    cnode[i - 1], slot, 1,
                    env->cspace_root, cnode[i], seL4_WordBits,
                    seL4_AllRights);
        test_assert(!error);
        ep_pos |= (slot << i);
        slot ^= 1;
    }
    /* In the final cnode, put an IPC endpoint in slot 1. */
    error = seL4_CNode_Copy(
                cnode[CONFIG_WORD_SIZE - 1], slot, 1,
                env->cspace_root, ep, seL4_WordBits,
                seL4_AllRights);
    test_assert(!error);

    /* Start a helper thread in our own cspace, to let it get set up. */
    helper_thread_t t;

    create_helper_thread(env, &t);
    start_helper(env, &t, ipc_caller, ep, ep_pos, CONFIG_WORD_SIZE, 0);

    /* Wait for it. */
    seL4_MessageInfo_t tag;
    seL4_Word sender_badge = 0;
    tag = api_wait(ep, &sender_badge);
    test_assert(seL4_MessageInfo_get_length(tag) == 1);
    test_assert(seL4_GetMR(0) == READY_MAGIC);

    /* Now switch its cspace. */
    error = api_tcb_set_space(get_helper_tcb(&t), t.fault_endpoint,
                              cnode[0], seL4_NilData, env->page_directory,
                              seL4_NilData);

    test_assert(!error);

    /* And now wait for it to do some tests and return to us. */
    tag = api_wait(ep, &sender_badge);
    test_assert(seL4_MessageInfo_get_length(tag) == 1);
    test_assert(seL4_GetMR(0) == SUCCESS_MAGIC);

    cleanup_helper(env, &t);
    return sel4test_get_result();
}
DEFINE_TEST(CSPACE0001, "Test full cspace resolution", test_full_cspace, true)
