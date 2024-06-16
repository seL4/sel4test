/*
 * Copyright 2024, UNSW
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/* This file contains tests related to vCPU syscalls. */

#include <sel4/sel4.h>

#include "../helpers.h"

#ifdef CONFIG_ARM_HYPERVISOR_SUPPORT
int test_vcpu_inject_without_tcb(env_t env)
{
    vka_object_t vcpu;
    int error = vka_alloc_vcpu(&env->vka, &vcpu);
    assert(!error);

    error = seL4_ARM_VCPU_InjectIRQ(vcpu.cptr, 0, 0, 0, 0);
    test_eq(error, seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(VCPU0001, "Inject IRQ without TCB associated with vCPU", test_vcpu_inject_without_tcb, true)
#endif
