/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdio.h>
#include <sel4/sel4.h>
#include <stdlib.h>
#include <string.h>
#include <vka/object.h>
#include <sel4test/macros.h>

#include "../helpers.h"

/* declare some per thread variables for our tests. both bss and data */
static __thread seL4_Word bss_array[6] = {0};
static __thread seL4_Word data_array[6] = {1, 2, 3, 4, 5, 6};

static int test_root_tls(env_t env)
{
    seL4_Word i;
    for (i = 0; i < ARRAY_SIZE(bss_array); i++) {
        test_eq(bss_array[i], (seL4_Word)0);
    }

    for (i = 0; i < ARRAY_SIZE(data_array); i++) {
        test_eq(data_array[i], i + 1);
    }
    /* very the bss and data arrays containg the correct thing */
    return sel4test_get_result();
}
DEFINE_TEST(
    TLS0001,
    "Test root thread accessing __thread variables",
    test_root_tls,
    true
)

static int
tls_helper(seL4_Word helper, seL4_Word done_ep, seL4_Word start_ep, seL4_Word arg4)
{
    seL4_Word i;
    /* first verify all our initial data */
    for (i = 0; i < ARRAY_SIZE(bss_array); i++) {
        test_eq(bss_array[i], (seL4_Word)0);
    }

    for (i = 0; i < ARRAY_SIZE(data_array); i++) {
        test_eq(data_array[i], i + 1);
    }
    /* now update based on our thread */
    for (i = 0; i < ARRAY_SIZE(bss_array); i++) {
        bss_array[i] = i + helper;
    }
    for (i = 0; i < ARRAY_SIZE(data_array); i++) {
        data_array[i] = helper * ARRAY_SIZE(data_array) + i;
    }
    /* signal we are ready */
    seL4_Signal(done_ep);
    /* wait for all threads are done and we are signaled to start */
    seL4_Wait(start_ep, NULL);
    /* verify our arrays are still the same */
    for (i = 0; i < ARRAY_SIZE(bss_array); i++) {
        test_eq(bss_array[i], i + helper);
    }
    for (i = 0; i < ARRAY_SIZE(data_array); i++) {
        test_eq(data_array[i], helper * ARRAY_SIZE(data_array) + i);
    }
    return sel4test_get_result();
}

static int test_threads_tls(env_t env)
{
    /* create endpoints for synchronization */
    seL4_CPtr done_ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr start_ep = vka_alloc_endpoint_leaky(&env->vka);
    /* spawn some helper threads for manipulating the __thread variables */
    helper_thread_t helper_threads[4];
    int i;
    for (i = 0; i < ARRAY_SIZE(helper_threads); i++) {
        create_helper_thread(env, &helper_threads[i]);
        start_helper(env, &helper_threads[i], tls_helper, i, done_ep, start_ep, 0);
    }
    /* wait for all threads to be done */
    for (i = 0; i < ARRAY_SIZE(helper_threads); i++) {
        seL4_Wait(done_ep, NULL);
    }
    /* signal all threads to do the verification step */
    for (i = 0; i < ARRAY_SIZE(helper_threads); i++) {
        seL4_Signal(start_ep);
    }
    /* wait for them all to complete */
    for (i = 0; i < ARRAY_SIZE(helper_threads); i++) {
        wait_for_helper(&helper_threads[i]);
        cleanup_helper(env, &helper_threads[i]);
    }
    return sel4test_get_result();
}
DEFINE_TEST(
    TLS0002,
    "Test multiple threads using __thread variables",
    test_threads_tls,
    true
)

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
)
{
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
DEFINE_TEST(
    TLS0006,
    "sel4utils_thread with distinct TLS should not interfere",
    test_sel4utils_thread_tls,
    true
)
