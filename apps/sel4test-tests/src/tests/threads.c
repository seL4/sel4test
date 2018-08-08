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

/* This file contains tests related to TCB syscalls. */

#include <stdio.h>
#include <sel4/sel4.h>
#include <sel4test/macros.h>

#include "../helpers.h"

int test_tcb_null_cspace_configure(env_t env)
{
    helper_thread_t thread;
    int error;

    create_helper_thread(env, &thread);

    /* This should fail because we're passing an invalid CSpace cap. */
    error = api_tcb_configure(get_helper_tcb(&thread), 0, seL4_CapNull,
                               seL4_CapNull, seL4_CapNull,
                               0, env->page_directory,
                               0, 0, 0);

    cleanup_helper(env, &thread);

    return error ? sel4test_get_result() : FAILURE;
}
DEFINE_TEST(THREADS0004, "seL4_TCB_Configure with a NULL CSpace should fail", test_tcb_null_cspace_configure, true)

int test_tcb_null_cspace_setspace(env_t env)
{
    helper_thread_t thread;
    int error;

    create_helper_thread(env, &thread);

    /* This should fail because we're passing an invalid CSpace cap. */
    error = api_tcb_set_space(get_helper_tcb(&thread), 0, seL4_CapNull,
                              0, env->page_directory,
                              0);

    cleanup_helper(env, &thread);

    return error ? sel4test_get_result() : FAILURE;
}
DEFINE_TEST(THREADS0005, "seL4_TCB_SetSpace with a NULL CSpace should fail", test_tcb_null_cspace_setspace, true)


// Thread local storage value.
#define INITIAL_TLS_VALUE 42
#define TLS_INCREMENT_ITERATIONS 10000
#define NUM_PARALLEL_THREADS 8

static __thread int tls_value = INITIAL_TLS_VALUE;

// Thread that competes.
static int simple_tls_test_thread(
	UNUSED seL4_Word arg1,
	UNUSED seL4_Word arg2,
	UNUSED seL4_Word arg3,
	UNUSED seL4_Word arg4
) {
    // Each thread should start with the same value.
    if (tls_value != INITIAL_TLS_VALUE) {
        sel4test_failure("TLS started with incorrect value");
        return -1;
    }

    // First try increment atomically.
    int initial = tls_value;
    int last = initial;
    for (int i = 0; i < TLS_INCREMENT_ITERATIONS; i++) {
        int next = __sync_add_and_fetch(&tls_value, 1);
        if (next != last + 1) {
            sel4test_failure("TLS did not increment atomically");
            return -1;
        }
        last = next;
    }

    if (tls_value != initial + TLS_INCREMENT_ITERATIONS) {
        sel4test_failure("TLS did not increment atomically");
        return -1;
    }

    // Then try non-atomic.
    // First try increment atomically.
    initial = tls_value;
    last = initial;
    for (int i = 0; i < TLS_INCREMENT_ITERATIONS; i++) {
        int next = ++tls_value;
        if (next != last + 1) {
            sel4test_failure("TLS did not increment correctly.");
            return -1;
        }
        last = next;
    }

    if (tls_value != initial + TLS_INCREMENT_ITERATIONS) {
        sel4test_failure("TLS did not increment correctly");
        return -1;
    }

    return 0;
}

int test_sel4utils_thread_tls(env_t env)
{
    helper_thread_t threads[NUM_PARALLEL_THREADS];

    for (int t = 0; t < NUM_PARALLEL_THREADS; t++) {
        create_helper_thread(env, &threads[t]);
        start_helper(
            env, &threads[t],
            simple_tls_test_thread,
            0, 0, 0, 0
        );
    }

    for (int t = 0; t < NUM_PARALLEL_THREADS; t++) {
        wait_for_helper(&threads[t]);
        cleanup_helper(env, &threads[t]);
    }

    return sel4test_get_result();
}
DEFINE_TEST(THREADS0006, "sel4utils_thread with distinct TLS should not interfere",
        test_sel4utils_thread_tls, !config_set(CONFIG_ARCH_RISCV))
