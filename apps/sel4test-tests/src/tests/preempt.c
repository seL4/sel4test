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
        wait_for_timer_interrupt(env);
        if (revoking == 1) {
            preempt_count++;
        }
    }
    return 0;
}

static int
test_preempt_revoke_actual(env_t env, int num_cnode_bits)
{
    helper_thread_t revoke_thread, preempt_thread;
    int error;

    /* Create an endpoint cap that will be derived many times. */
    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);

    create_helper_thread(env, &revoke_thread);
    create_helper_thread(env, &preempt_thread);

    set_helper_priority(env, &preempt_thread, 101);
    set_helper_priority(env, &revoke_thread, 100);

    /* Now create as many cnodes as possible. We will copy the cap into all
     * those cnodes. */
    int num_caps = 0;

#define CNODE_SIZE_BITS 12

    ZF_LOGD("    Creating %d caps .", (1 << (num_cnode_bits + CNODE_SIZE_BITS)));
    for (int i = 0; i < (1 << num_cnode_bits); i++) {
        seL4_CPtr ctable = vka_alloc_cnode_object_leaky(&env->vka, CNODE_SIZE_BITS);

        for (int j = 0; j < (1 << CNODE_SIZE_BITS); j++) {
            error = seL4_CNode_Copy(
                        ctable, j, CNODE_SIZE_BITS,
                        env->cspace_root, ep, seL4_WordBits,
                        seL4_AllRights);

            test_assert(!error);
            num_caps++;
        }
        ZF_LOGD(".");
    }
    test_check(num_caps > 0);
    test_check((num_caps == (1 << (num_cnode_bits + CNODE_SIZE_BITS))));

    ltimer_set_timeout(&env->timer.ltimer, NS_IN_MS, TIMEOUT_PERIODIC);
    wait_for_timer_interrupt(env);

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

    ltimer_reset(&env->timer.ltimer);

    return preempt_count;
}

static int
test_preempt_revoke(env_t env)
{
    for (int num_cnode_bits = 1; num_cnode_bits < 8; num_cnode_bits++) {
        if (test_preempt_revoke_actual(env, num_cnode_bits) > 1) {
            return sel4test_get_result();
        }
    }

    ZF_LOGD("Couldn't trigger preemption point with millions of caps!\n");
    test_assert(0);
}
DEFINE_TEST(PREEMPT_REVOKE, "Test preemption path in revoke", test_preempt_revoke, config_set(CONFIG_HAVE_TIMER))
