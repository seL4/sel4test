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
