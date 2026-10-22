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

/*
 * VCPU0002: check that seL4_TCB_CopyRegisters sanitises the status register, so
 * that native destination threads can never get set to privileged mode via
 * content from VCPU thread registers.
 *
 * The mode field is bits [4:0] of the status register on both AArch64 and AArch32.
 */
#ifdef CONFIG_ARCH_AARCH64
#define MODE_REG  spsr
#define MODE_PRIV 5     /* EL1h */
#define MODE_USER 0     /* EL0 */
#else
#define MODE_REG  cpsr
#define MODE_PRIV 0x13  /* SVC */
#define MODE_USER 0x10  /* USR */
#endif
#define MODE_MASK 0x1f

int test_copyregisters_sanitise(env_t env)
{
    vka_object_t src, dst, vcpu;
    seL4_UserContext regs = {0};
    int error;

    error = vka_alloc_tcb(&env->vka, &src);
    test_eq(error, 0);
    error = vka_alloc_tcb(&env->vka, &dst);
    test_eq(error, 0);
    error = vka_alloc_vcpu(&env->vka, &vcpu);
    test_eq(error, 0);

    /* src has no VCPU yet, so WriteRegisters must sanitise the mode to user. */
    regs.MODE_REG = MODE_PRIV;
    error = seL4_TCB_WriteRegisters(src.cptr, 0, 0, 3, &regs);
    test_eq(error, seL4_NoError);
    error = seL4_TCB_ReadRegisters(src.cptr, 0, 0, 3, &regs);
    test_eq(error, seL4_NoError);
    test_eq(regs.MODE_REG & MODE_MASK, (seL4_Word)MODE_USER);

    error = seL4_ARM_VCPU_SetTCB(vcpu.cptr, src.cptr);
    test_eq(error, seL4_NoError);

    /* src now has a VCPU, so WriteRegisters allows setting privileged mode. */
    regs.MODE_REG = MODE_PRIV;
    error = seL4_TCB_WriteRegisters(src.cptr, 0, 0, 3, &regs);
    test_eq(error, seL4_NoError);
    error = seL4_TCB_ReadRegisters(src.cptr, 0, 0, 3, &regs);
    test_eq(error, seL4_NoError);
    test_eq(regs.MODE_REG & MODE_MASK, (seL4_Word)MODE_PRIV);

    /* dst has no VCPU, so CopyRegisters must sanitise the mode to user. */
    error = seL4_TCB_CopyRegisters(dst.cptr, src.cptr, 0, 0, 1, 0, 0);
    test_eq(error, seL4_NoError);
    error = seL4_TCB_ReadRegisters(dst.cptr, 0, 0, 3, &regs);
    test_eq(error, seL4_NoError);
    test_eq(regs.MODE_REG & MODE_MASK, (seL4_Word)MODE_USER);

    vka_free_object(&env->vka, &vcpu);
    vka_free_object(&env->vka, &dst);
    vka_free_object(&env->vka, &src);
    return sel4test_get_result();
}
DEFINE_TEST(VCPU0002, "CopyRegisters sanitises the status register for the destination TCB",
            test_copyregisters_sanitise, true)
#endif
