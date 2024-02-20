/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <sel4utils/arch/util.h>

#include <vka/object.h>
#include <vka/capops.h>

#include "../test.h"
#include "../helpers.h"
#include "sel4/simple_types.h"

#ifdef CONFIG_ALLOW_SMC_CALLS

#define ARM_STD_SVC_VERSION    0x8400ff03
#define UNASSIGNED_SMC         0x82000000

static int test_smc_calls(env_t env)
{
    seL4_ARM_SMCContext smc_args;
    seL4_ARM_SMCContext smc_results;
    int error;

    seL4_CPtr smc_cap = env->smc;
    seL4_CPtr badged_smc_cap = get_free_slot(env);

    error = cnode_mint(env, smc_cap, badged_smc_cap, seL4_AllRights, ARM_STD_SVC_VERSION);
    test_error_eq(error, seL4_NoError);

    /* Set function and arguments */
    smc_args.x0 = ARM_STD_SVC_VERSION;
    smc_args.x1 = 0;
    smc_args.x2 = 0;
    smc_args.x3 = 0;
    smc_args.x4 = 0;
    smc_args.x5 = 0;
    smc_args.x6 = 0;
    smc_args.x7 = 0;

    /* This should succeed */
    error = seL4_ARM_SMC_Call(badged_smc_cap, &smc_args, &smc_results);
    test_error_eq(error, seL4_NoError);

    /* Make sure the call returned something other than the input */
    test_neq(smc_results.x0, smc_args.x0);

    /* Check that the version returned is non-zero */
    seL4_Word version_sum = smc_results.x0 + smc_results.x1;
    test_geq(version_sum, 1UL);

    /* This should fail - SMC call with a different function id from badge */
    smc_args.x0 = UNASSIGNED_SMC;
    error = seL4_ARM_SMC_Call(badged_smc_cap, &smc_args, &smc_results);
    test_error_eq(error, seL4_IllegalOperation);

    /* This should fail - can't mint from cap with non-zero badge */
    seL4_CPtr bad_badged_cap = get_free_slot(env);
    error = cnode_mint(env, badged_smc_cap, bad_badged_cap, seL4_AllRights, UNASSIGNED_SMC);
    test_error_eq(error, seL4_IllegalOperation);

    return sel4test_get_result();
}
DEFINE_TEST(SMC0001, "Test SMC calls", test_smc_calls, true)


int test_smc_2(env_t env)
{

    seL4_CPtr dest = get_free_slot(env);
    int error = cnode_copy(env, env->smc, dest, seL4_AllRights);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest));

    return sel4test_get_result();
}
DEFINE_TEST(SMC0002, "SMC Caps can be copied", test_smc_2, true)

int test_smc_3(env_t env)
{

    seL4_CPtr dest = get_free_slot(env);
    int error = cnode_copy(env, env->smc, dest, seL4_AllRights);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest));

    error = cnode_revoke(env, env->smc);
    test_assert(!is_slot_empty(env, dest));

    return sel4test_get_result();
}

DEFINE_TEST(SMC0003, "Copied SMC Caps lose revocable authority", test_smc_3, true)

int test_smc_4(env_t env)
{

    seL4_CPtr dest = get_free_slot(env);
    int error = cnode_mint(env, env->smc, dest, seL4_AllRights, 1);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest));

    return sel4test_get_result();
}

DEFINE_TEST(SMC0004, "Unbadged SMC Caps can be badged", test_smc_4, true)


int test_smc_5(env_t env)
{

    seL4_CPtr dest = get_free_slot(env);
    int error = cnode_mint(env, env->smc, dest, seL4_AllRights, 1);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest));

    seL4_CPtr dest2 = get_free_slot(env);
    error = cnode_mint(env, dest, dest2, seL4_AllRights, 2);
    test_error_eq(error, seL4_IllegalOperation);
    test_assert(is_slot_empty(env, dest2));

    return sel4test_get_result();
}

DEFINE_TEST(SMC0005, "Badged SMC caps cannot change badge", test_smc_5, true)

int test_smc_6(env_t env)
{

    seL4_CPtr dest = get_free_slot(env);
    int error = cnode_mint(env, env->smc, dest, seL4_AllRights, 1);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest));

    seL4_CPtr dest2 = get_free_slot(env);
    error = cnode_copy(env, dest, dest2, seL4_AllRights);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest2));

    return sel4test_get_result();
}
DEFINE_TEST(SMC0006, "Badged SMC caps can be copied", test_smc_6, true)


int test_smc_7(env_t env)
{

    seL4_CPtr dest = get_free_slot(env);
    int error = cnode_mint(env, env->smc, dest, seL4_AllRights, 1);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest));

    seL4_CPtr dest2 = get_free_slot(env);
    error = cnode_copy(env, dest, dest2, seL4_AllRights);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest2));

    error = cnode_revoke(env, dest);
    test_assert(is_slot_empty(env, dest2));

    return sel4test_get_result();
}

DEFINE_TEST(SMC0007, "Original badged SMC caps can revoke copies", test_smc_7, true)

int test_smc_8(env_t env)
{

    seL4_CPtr dest = get_free_slot(env);
    int error = cnode_mint(env, env->smc, dest, seL4_AllRights, 1);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest));

    seL4_CPtr dest2 = get_free_slot(env);
    error = cnode_mint(env, env->smc, dest2, seL4_AllRights, 1);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest2));


    seL4_CPtr dest3 = get_free_slot(env);
    error = cnode_copy(env, dest2, dest3, seL4_AllRights);
    test_error_eq(error, seL4_NoError);
    test_assert(!is_slot_empty(env, dest3));

    error = cnode_revoke(env, dest2);
    test_assert(is_slot_empty(env, dest3));
    test_assert(!is_slot_empty(env, dest));

    return sel4test_get_result();
}


DEFINE_TEST(SMC0008, "Original badged SMC caps don't revoke other original badges", test_smc_8, true)

#endif
