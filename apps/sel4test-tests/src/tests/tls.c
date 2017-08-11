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

#include <autoconf.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <stdlib.h>
#include <string.h>
#include <vka/object.h>

#include "../helpers.h"

/* declare some per thread variables for our tests. both bss and data */
static __thread int bss_array[6] = {0};
static __thread int data_array[6] = {1, 2, 3, 4, 5, 6};

static int
test_root_tls(env_t env)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(bss_array); i++) {
        test_eq(bss_array[i], 0);
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
    !config_set(CONFIG_ARCH_RISCV)
)

static int
tls_helper(seL4_Word helper, seL4_Word done_ep, seL4_Word start_ep, seL4_Word arg4)
{
    int i;
    /* first verify all our initial data */
    for (i = 0; i < ARRAY_SIZE(bss_array); i++) {
        test_eq(bss_array[i], 0);
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

static int
test_threads_tls(env_t env)
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
    !config_set(CONFIG_ARCH_RISCV)
)
