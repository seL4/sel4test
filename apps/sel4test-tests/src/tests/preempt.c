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

#include "../helpers.h"

static volatile int revoking = 0;
static volatile int preempt_count = 0;

static int
revoke_func(seL4_CNode service, seL4_Word index, seL4_Word depth)
{
    revoking = 1;
    seL4_CNode_Revoke(service, index, depth);
    revoking = 2;
    return 0;
}

static int
preempt_count_func(env_t env)
{
    while (revoking < 2) {
        sel4test_ntfn_timer_wait(env);
        if (revoking == 1) {
            preempt_count++;
        }
    }
    return 0;
}

static int
create_cnode_table(env_t env, int num_cnode_bits, seL4_CPtr ep) {
    /* Create as many cnodes as possible. We will copy the cap into all
     * those cnodes. */
#define CNODE_SIZE_BITS 12
    int num_caps = 0;
    int error = 0;

    ZF_LOGD("    Creating %d caps .", BIT((num_cnode_bits + CNODE_SIZE_BITS)));
    for (int i = 0; i < (1 << num_cnode_bits); i++) {
        seL4_CPtr ctable = vka_alloc_cnode_object_leaky(&env->vka, CNODE_SIZE_BITS);

        for (int j = 0; j < BIT(CNODE_SIZE_BITS); j++) {
            error = seL4_CNode_Copy(
                        ctable, j, CNODE_SIZE_BITS,
                        env->cspace_root, ep, seL4_WordBits,
                        seL4_AllRights);

            test_check(!error);
            num_caps++;
        }
        ZF_LOGD(".");
    }

    if (num_caps != BIT(num_cnode_bits + CNODE_SIZE_BITS)) {
        ZF_LOGD("Created %d caps. Couldn't create the required number of caps %d",
           num_caps,
           BIT(num_cnode_bits + CNODE_SIZE_BITS));
        return -1;
    }

    return error;
}

static int
test_preempt_revoke_actual(env_t env, int num_cnode_bits)
{
    helper_thread_t revoke_thread, preempt_thread;
    int error;
    uint64_t timeout_val = 0;

    /* Create an endpoint cap that will be derived many times. */
    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &revoke_thread);
    create_helper_thread(env, &preempt_thread);

    set_helper_priority(env, &preempt_thread, 101);
    set_helper_priority(env, &revoke_thread, 100);

    /* First create a ctable and fill it with copied ep caps
     * and time the revoke operation
     */
    error = create_cnode_table(env, num_cnode_bits, ep);
    test_assert(!error);

    uint64_t start, end, diff;
    /* meaure revoke_func time.
     * Note that we don't take caching and other effects (e.g. function calls vs.
     * jmp costs into consideration, however, this should get us a better
     * timeout value per target compared to setting a fixed timeout value that may
     * fail on different targets.
     */
    start = sel4test_timestamp(env);
    revoke_func(env->cspace_root, ep, seL4_WordBits);
    end = sel4test_timestamp(env);

    test_geq(end, start);

    diff = end - start;

    /* Set a timeout value to half of the revoke operation time to allow
     * preemption in the middle, as the test expects.
     */
    timeout_val = diff / 2;

    /* Now, actually create a ctable that is gonna be used in the revoke
     * test.
     */
    error = create_cnode_table(env, num_cnode_bits, ep);
    test_assert(!error);
    sel4test_periodic_start(env, timeout_val);

    /* Last thread to start runs first. */
    revoking = 0;
    preempt_count = 0;
    start_helper(env, &preempt_thread, (helper_fn_t) preempt_count_func, (seL4_Word) env, 0, 0, 0);
    start_helper(env, &revoke_thread, (helper_fn_t) revoke_func, env->cspace_root,
                 ep, seL4_WordBits, 0);

    wait_for_helper(&revoke_thread);

    cleanup_helper(env, &preempt_thread);
    cleanup_helper(env, &revoke_thread);

    ZF_LOGD("    %d preemptions\n", preempt_count);

    return preempt_count;
}

static int
test_preempt_revoke(env_t env)
{
    for (int num_cnode_bits = 1; num_cnode_bits < 32; num_cnode_bits++) {
        int result = test_preempt_revoke_actual(env, num_cnode_bits);
        if (result > 1) {
            return sel4test_get_result();
        } else if (result == -1) {
            /* At this point we assume this error is a problem where the time of
             *  revoc_func < timer_resolution, which will cause set timeout to faule
             *  return an error. Skip such small cnodes and only work with cnode sizes where
             *  revoc_func > timer_resolution
             */
            continue;
        }
    }

    ZF_LOGD("Couldn't trigger preemption point with millions of caps!\n");
    test_assert(0);
}
DEFINE_TEST(PREEMPT_REVOKE, "Test preemption path in revoke", test_preempt_revoke, config_set(CONFIG_HAVE_TIMER))
