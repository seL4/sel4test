/*
 * Copyright 2026, Proofcraft Pty Ltd
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Add tests that show kernel calls preserve user-controllable flags and
 * that kernel behaviour is not influenced by these flags.
 *
 * Currently only checks "interesting" flags.
 */

#include <autoconf.h>
#include <sel4/sel4.h>
#include <utils/attribute.h>

#include "../../../helpers.h"
#include "../../../test.h"

#define X86_FLAGS_NT   BIT(14)
#define X86_FLAGS_AC   BIT(18)
#define X86_FLAGS_ID   BIT(21)

#define X86_FLAGS_TESTED (X86_FLAGS_NT | X86_FLAGS_AC | X86_FLAGS_ID)

static inline seL4_Word read_flags(void)
{
    seL4_Word flags;
    asm volatile("pushf\n\tpop %0" : "=r"(flags) : : "memory");
    return flags;
}

static inline void write_flags(seL4_Word flags)
{
    asm volatile("push %0\n\tpopf" : : "r"(flags) : "memory", "cc");
}

/* Perform a syscall with the flags in `set` set and the rest of
 * X86_FLAGS_TESTED clear. Returns the flags after a Yield syscall. */
static seL4_Word syscall_with_flags(seL4_Word set)
{
    seL4_Word saved = read_flags();
    write_flags((saved & ~(seL4_Word)X86_FLAGS_TESTED) | set);
    seL4_Yield();
    seL4_Word result = read_flags();
    write_flags(saved);
    return result;
}

static const seL4_Word flag_patterns[] = {
    0,
    X86_FLAGS_NT,
    X86_FLAGS_AC,
    X86_FLAGS_ID,
    X86_FLAGS_TESTED,
};

/* Use volatile parameter pointer to make the compiler actually emit a spin loop. */
static int spin_helper(volatile int *stop)
{
    while (!*stop) {
        /* spin */
    }
    return 0;
}

static int set_flag_helper(volatile int *flag)
{
    *flag = 1;
    return 0;
}

/* Enter the kernel via syscall with various user flags set. The kernel must
 * not be affected by the flags and must preserve them. */
static int test_syscall_flags(env_t env)
{
    for (int i = 0; i < ARRAY_SIZE(flag_patterns); i++) {
        seL4_Word set = flag_patterns[i];
        seL4_Word result = syscall_with_flags(set);
        test_eq(result & X86_FLAGS_TESTED, set);
    }

    return sel4test_get_result();
}
DEFINE_TEST(X86_FLAGS_0001, "Syscall with NT, AC, ID set preserves flags", test_syscall_flags, true)

/* Enter the kernel via syscall with NT set and make the kernel switch to
 * another thread that is resumed with IRET. If the kernel does not clear NT,
 * the kernel will fault on IRET. */
static int test_syscall_flags_switch(env_t env)
{
    helper_thread_t helper;
    volatile int stop = 0;

    create_helper_thread(env, &helper);
    set_helper_priority(env, &helper, OUR_PRIO);
    start_helper(env, &helper, (helper_fn_t) spin_helper, (seL4_Word) &stop, 0, 0, 0);

    /* The helper is at the same priority, so the kernel switches between the
     * helper and the test thread. The helper is resumed via IRET, because it
     * is preempted by a timer interrupt. */
    for (int i = 0; i < ARRAY_SIZE(flag_patterns); i++) {
        seL4_Word set = flag_patterns[i];
        seL4_Word result = syscall_with_flags(set);
        test_eq(result & X86_FLAGS_TESTED, set);
    }

    stop = 1;
    wait_for_helper(&helper);
    cleanup_helper(env, &helper);

    return sel4test_get_result();
}
DEFINE_TEST(X86_FLAGS_0002, "Syscall with NT set followed by thread switch", test_syscall_flags_switch, true)

/* Get preempted by a timer interrupt with NT, AC, ID set. Check that the kernel
 * restores the flags on return. */
static int test_interrupt_flags(env_t env)
{
    helper_thread_t helper;
    volatile int flag = 0;

    create_helper_thread(env, &helper);
    set_helper_priority(env, &helper, OUR_PRIO);
    start_helper(env, &helper, (helper_fn_t) set_flag_helper, (seL4_Word) &flag, 0, 0, 0);

    /* The helper is at the same priority and only runs once we are preempted. */
    seL4_Word saved = read_flags();
    write_flags(saved | X86_FLAGS_TESTED);
    while (!flag) {
        /* spin */
    }
    seL4_Word result = read_flags();
    write_flags(saved);
    test_eq(result & X86_FLAGS_TESTED, (seL4_Word) X86_FLAGS_TESTED);

    wait_for_helper(&helper);
    cleanup_helper(env, &helper);

    return sel4test_get_result();
}
DEFINE_TEST(X86_FLAGS_0003, "Interrupt with NT, AC, ID set preserves flags", test_interrupt_flags, true)
