/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <sel4/sel4.h>

#include "../helpers.h"

static int test_retype(env_t env)
{
    int error;
    int i;
    vka_object_t untyped;
    vka_object_t cnode;

    error = vka_alloc_cnode_object(&env->vka, 2, &cnode);
    test_error_eq(error, 0);

    error = vka_alloc_untyped(&env->vka, seL4_TCBBits + 3, &untyped);
    test_error_eq(error, 0);

    /* Try to insert 0. */
    error = seL4_Untyped_Retype(untyped.cptr,
                                seL4_TCBObject, 0,
                                env->cspace_root, cnode.cptr, seL4_WordBits,
                                1, 0);
    test_error_eq(error, seL4_RangeError);

    /* Check we got useful min/max error codes. */
    test_eq(seL4_GetMR(0), 1ul);
    test_eq(seL4_GetMR(1), (unsigned long)CONFIG_RETYPE_FAN_OUT_LIMIT);

    /* Try to drop two caps in, at the end of the cnode, overrunning it. */
    error = seL4_Untyped_Retype(untyped.cptr,
                                seL4_TCBObject, 0,
                                env->cspace_root, cnode.cptr, seL4_WordBits,
                                (1 << 2) - 1, 2);
    test_error_eq(error, seL4_RangeError);

    /* Drop some caps in. This should be successful. */
    for (i = 0; i < (1 << 2); i++) {
        error = seL4_Untyped_Retype(untyped.cptr,
                                    seL4_TCBObject, 0,
                                    env->cspace_root, cnode.cptr, seL4_WordBits,
                                    i, 1);
        test_error_eq(error, seL4_NoError);
    }

    /* Try to drop one in beyond the end of the cnode. */
    error = seL4_Untyped_Retype(untyped.cptr,
                                seL4_TCBObject, 0,
                                env->cspace_root, cnode.cptr, seL4_WordBits,
                                i, 2);
    test_error_eq(error, seL4_RangeError);

    /* Try putting caps over the top. */
    for (i = 0; i < (1 << 2); i++) {
        error = seL4_Untyped_Retype(untyped.cptr,
                                    seL4_TCBObject, 0,
                                    env->cspace_root, cnode.cptr, seL4_WordBits,
                                    i, 1);
        test_error_eq(error, seL4_DeleteFirst);
    }

    /* Delete them all. */
    for (i = 0; i < (1 << 2); i++) {
        error = seL4_CNode_Delete(cnode.cptr, i, 2);
        test_error_eq(error, seL4_NoError);
    }

    /* Try to insert too many. */
    error = seL4_Untyped_Retype(untyped.cptr,
                                seL4_TCBObject, 0,
                                env->cspace_root, cnode.cptr, seL4_WordBits,
                                0, 1U << 31);
    test_error_eq(error, seL4_RangeError);

    error = seL4_Untyped_Retype(untyped.cptr,
                                seL4_TCBObject, 0,
                                env->cspace_root, cnode.cptr, seL4_WordBits,
                                0, (1 << 2) + 1);
    test_error_eq(error, seL4_RangeError);

    /* Insert them in one fell swoop but one. */
    error = seL4_Untyped_Retype(untyped.cptr,
                                seL4_TCBObject, 0,
                                env->cspace_root, cnode.cptr, seL4_WordBits,
                                0, (1 << 2) - 1);
    test_error_eq(error, seL4_NoError);

    /* Try inserting over the top. Only the last should succeed. */
    for (i = 0; i < (1 << 2); i++) {
        error = seL4_Untyped_Retype(untyped.cptr,
                                    seL4_TCBObject, 0,
                                    env->cspace_root, cnode.cptr, seL4_WordBits,
                                    i, 1);
        if (i == (1 << 2) - 1) {
            test_error_eq(error, seL4_NoError);
        } else {
            test_error_eq(error, seL4_DeleteFirst);
        }
    }

    vka_free_object(&env->vka, &untyped);
    vka_free_object(&env->vka, &cnode);
    return sel4test_get_result();
}
DEFINE_TEST(RETYPE0000, "Retype test", test_retype, true)

static int
test_incretype(env_t env)
{
    int error;
    vka_object_t untyped;
    int size_bits;

    /* Find an untyped of some size. */
    for (size_bits = 13; size_bits > 0; size_bits--) {
        error = vka_alloc_untyped(&env->vka, size_bits, &untyped);
        if (error == 0) {
            break;
        }
    }
    test_error_eq(error, 0);
    test_assert(untyped.cptr != 0);

    /* Try retyping anything bigger than the object into it. */
    int i;
    for (i = 40; i > 0; i--) {
        error = seL4_Untyped_Retype(untyped.cptr,
                                    seL4_CapTableObject, size_bits - seL4_SlotBits + i,
                                    env->cspace_root, env->cspace_root, seL4_WordBits,
                                    0, 1);
        test_assert(error);
    }

    /* Try retyping an object of the correct size in. */
    error = seL4_Untyped_Retype(untyped.cptr,
                                seL4_CapTableObject, size_bits - seL4_SlotBits + 0,
                                env->cspace_root, env->cspace_root, seL4_WordBits,
                                0, 1);
    test_error_eq(error, seL4_NoError);

    /* clean up */
    vka_free_object(&env->vka, &untyped);

    return sel4test_get_result();
}
DEFINE_TEST(RETYPE0001, "Incremental retype test", test_incretype, true)

static int
test_incretype2(env_t env)
{
    int error;
    seL4_Word slot[17];
    vka_object_t untyped;

    /* Get a bunch of free slots. */
    for (int i = 0; i < sizeof(slot) / sizeof(slot[0]); i++) {
        error = vka_cspace_alloc(&env->vka, &slot[i]);
        test_error_eq(error, 0);
    }

    /* And an untyped big enough to allocate 16 4-k pages into. */
    error = vka_alloc_untyped(&env->vka, 16, &untyped);
    test_error_eq(error, 0);
    test_assert(untyped.cptr != 0);

    /* Try allocating precisely 16 pages. These should all work. */
    int i;
    for (i = 0; i < 16; i++) {
        error = seL4_Untyped_Retype(untyped.cptr,
                                    seL4_ARCH_4KPage, 0,
                                    env->cspace_root, env->cspace_root, seL4_WordBits,
                                    slot[i], 1);
        test_error_eq(error, seL4_NoError);
    }

    /* An obscenely large allocation should fail (note that's 2^(2^20)). */
    error = seL4_Untyped_Retype(untyped.cptr,
                                seL4_ARCH_4KPage, 0,
                                env->cspace_root, env->cspace_root, seL4_WordBits,
                                slot[i], 1024 * 1024);
    test_error_eq(error, seL4_RangeError);

    /* Allocating to an existing slot should fail. */
    error = seL4_Untyped_Retype(untyped.cptr,
                                seL4_ARCH_4KPage, 0,
                                env->cspace_root, env->cspace_root, seL4_WordBits,
                                slot[0], 8);
    test_error_eq(error, seL4_DeleteFirst);

    /* Allocating another item should also fail as the untyped is full. */
    error = seL4_Untyped_Retype(untyped.cptr,
                                seL4_ARCH_4KPage, 0,
                                env->cspace_root, env->cspace_root, seL4_WordBits,
                                slot[i++], 1);
    test_error_eq(error, seL4_NotEnoughMemory);

    vka_free_object(&env->vka, &untyped);

    return sel4test_get_result();
}
DEFINE_TEST(RETYPE0002, "Incremental retype test #2", test_incretype2, true)
