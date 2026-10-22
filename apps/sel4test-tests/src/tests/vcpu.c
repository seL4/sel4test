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
 * Status register (SPSR/CPSR) mode bit access for the tests below. The mode
 * field is in bits [4:0] on both AArch64 and AArch32.
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

/* Set privileged mode in the status register of tcb */
static int set_priv_mode(env_t env, seL4_CPtr tcb)
{
    seL4_UserContext regs = {0};
    int error;

    regs.MODE_REG = MODE_PRIV;
    error = seL4_TCB_WriteRegisters(tcb, 0, 0, 3, &regs);
    test_eq(error, seL4_NoError);

    return sel4test_get_result();
}

/* Check that the status register of tcb is in user mode. */
static int check_user_mode(env_t env, seL4_CPtr tcb)
{
    seL4_UserContext regs = {0};
    int error;

    error = seL4_TCB_ReadRegisters(tcb, 0, 0, 3, &regs);
    test_eq(error, seL4_NoError);
    test_eq(regs.MODE_REG & MODE_MASK, (seL4_Word)MODE_USER);

    return sel4test_get_result();
}

/* Set tcb to privileged mode and check that it took effect. */
static int set_and_check_priv_mode(env_t env, seL4_CPtr tcb)
{
    seL4_UserContext regs = {0};
    int error;

    error = set_priv_mode(env, tcb);
    test_eq(error, SUCCESS);
    error = seL4_TCB_ReadRegisters(tcb, 0, 0, 3, &regs);
    test_eq(error, seL4_NoError);
    test_eq(regs.MODE_REG & MODE_MASK, (seL4_Word)MODE_PRIV);

    return sel4test_get_result();
}

/*
 * VCPU0002: check that seL4_TCB_CopyRegisters sanitises the status register, so
 * that native destination threads can never get set to privileged mode via
 * content from VCPU thread registers.
 */
int test_copyregisters_sanitise(env_t env)
{
    vka_object_t src, dst, vcpu;
    int error;

    error = vka_alloc_tcb(&env->vka, &src);
    test_eq(error, 0);
    error = vka_alloc_tcb(&env->vka, &dst);
    test_eq(error, 0);
    error = vka_alloc_vcpu(&env->vka, &vcpu);
    test_eq(error, 0);

    /* src has no VCPU yet, so WriteRegisters must sanitise the mode to user. */
    error = set_priv_mode(env, src.cptr);
    test_eq(error, SUCCESS);
    error = check_user_mode(env, src.cptr);
    test_eq(error, SUCCESS);

    error = seL4_ARM_VCPU_SetTCB(vcpu.cptr, src.cptr);
    test_eq(error, seL4_NoError);

    /* src now has a VCPU, so WriteRegisters allows setting privileged mode. */
    error = set_and_check_priv_mode(env, src.cptr);
    test_eq(error, SUCCESS);

    /* dst has no VCPU, so CopyRegisters must sanitise the mode to user. */
    error = seL4_TCB_CopyRegisters(dst.cptr, src.cptr, 0, 0, 1, 0, 0);
    test_eq(error, seL4_NoError);
    error = check_user_mode(env, dst.cptr);
    test_eq(error, SUCCESS);

    vka_free_object(&env->vka, &vcpu);
    vka_free_object(&env->vka, &dst);
    vka_free_object(&env->vka, &src);
    return sel4test_get_result();
}
DEFINE_TEST(VCPU0002, "CopyRegisters sanitises the status register for the destination TCB",
            test_copyregisters_sanitise, true)

/*
 * VCPU0003: check that the status register of a TCB is sanitised when its VCPU
 * is dissociated, both when the VCPU is associated with another TCB and when
 * the VCPU is deleted.
 */
int test_dissociate_sanitise(env_t env)
{
    vka_object_t tcb1, tcb2, vcpu;
    int error;

    error = vka_alloc_tcb(&env->vka, &tcb1);
    test_eq(error, 0);
    error = vka_alloc_tcb(&env->vka, &tcb2);
    test_eq(error, 0);
    error = vka_alloc_vcpu(&env->vka, &vcpu);
    test_eq(error, 0);

    /* Associate the VCPU with tcb1 and set tcb1 to privileged mode. */
    error = seL4_ARM_VCPU_SetTCB(vcpu.cptr, tcb1.cptr);
    test_eq(error, seL4_NoError);
    error = set_and_check_priv_mode(env, tcb1.cptr);
    test_eq(error, SUCCESS);

    /* Moving the VCPU to tcb2 dissociates it from tcb1, which must sanitise tcb1. */
    error = seL4_ARM_VCPU_SetTCB(vcpu.cptr, tcb2.cptr);
    test_eq(error, seL4_NoError);
    error = check_user_mode(env, tcb1.cptr);
    test_eq(error, SUCCESS);

    /* Set tcb2 to privileged mode, then delete the VCPU, which must sanitise tcb2. */
    error = set_and_check_priv_mode(env, tcb2.cptr);
    test_eq(error, SUCCESS);
    vka_free_object(&env->vka, &vcpu);
    error = check_user_mode(env, tcb2.cptr);
    test_eq(error, SUCCESS);

    vka_free_object(&env->vka, &tcb2);
    vka_free_object(&env->vka, &tcb1);
    return sel4test_get_result();
}
DEFINE_TEST(VCPU0003, "Dissociating a VCPU sanitises the status register of the TCB",
            test_dissociate_sanitise, true)
#endif
