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
#ifdef HAVE_AUTOCONF
#include <autoconf.h>
#endif

#ifdef CONFIG_HARDWARE_DEBUG_API

#include <stdint.h>
#include <sel4/sel4.h>
#include <utils/attribute.h>
#include <vka/capops.h>
#include <sel4utils/thread.h>

#include <arch/debug.h>
#include "../helpers.h"
#include "../test.h"
#include "breakpoints.h"

#define BREAKPOINT_TEST_BAD_FAULT_MAGIC_ADDR (0xDEAD)

/* Other (receiver) end of the Endpoint object on which the kernel will queue
 * the Fault events triggered by the fault thread.
 */
cspacepath_t fault_ep_cspath = { 0 };
static cspacepath_t badged_fault_ep_cspath = { 0 };

int
setup_caps_for_test(struct env *env)
{
    seL4_CPtr fault_ep_cap;
    int error;

    fault_ep_cap = vka_alloc_endpoint_leaky(&env->vka);
    if (fault_ep_cap == 0) {
        ZF_LOGE("Failed to alloc endpoint object for faults.");
        return -1;
    }
    vka_cspace_make_path(&env->vka, fault_ep_cap, &fault_ep_cspath);
    /* Mint a badged copy of the fault ep. */
    error = vka_cspace_alloc_path(&env->vka, &badged_fault_ep_cspath);
    if (error != seL4_NoError) {
        ZF_LOGE("Failed to alloc cap slot for minted EP cap copy.");
        return -1;
    }
    error = vka_cnode_mint(&badged_fault_ep_cspath, &fault_ep_cspath, seL4_AllRights,
                           FAULT_EP_KERNEL_BADGE_VALUE);
    if (error != seL4_NoError) {
        ZF_LOGE("Failed to mint fault EP cap.");
        return -1;
    }
    return 0;
}

int
setup_faulter_thread_for_test(struct env *env, helper_thread_t *faulter_thread)
{
    int error;

    create_helper_thread(env, faulter_thread);
    NAME_THREAD(get_helper_tcb(faulter_thread), "Faulter");
    /* Make the kernel send all faults to the endpoint that the handler thread
     * will be told to listen on.
     */
    error = api_tcb_set_space(
                get_helper_tcb(faulter_thread),
                badged_fault_ep_cspath.capPtr,
                env->cspace_root,
                api_make_guard_skip_word(seL4_WordBits - env->cspace_size_bits),
                env->page_directory, seL4_NilData);
    if (error != 0) {
        ZF_LOGE("Failed to set fault EP for helper thread.");
        return -1;
    }

    set_helper_priority(env, faulter_thread, BREAKPOINT_TEST_FAULTER_PRIO);
    return 0;
}

/** Triggers an instruction breakpoint.
 *
 * Must not be inlined, or else we'll never trigger the breakpoint.
 *
 * This function will either trigger a breakpoint at its own address, or it will
 * trigger a segfault at 0xDEAD. In order to test if the breakpoint
 * triggered, test for the fault address, and if the fault address is not 0xDEAD,
 * and it's also the address of this function, then the breakpoint was triggered
 * successfully.
 */
NO_INLINE static void
breakpoint_code(void)
{
    volatile int *pi = (volatile int *)BREAKPOINT_TEST_BAD_FAULT_MAGIC_ADDR;
    *pi = 1;
}

/* This struct is what we will use in the data breakpoint tests: all the data
 * breakpoint tests will break on this struct's address.
 *
 * Volatile so that the compiler doesn't optimize away accesses to it since its
 * value is never used.
 */
static volatile uint32_t bpd;

/** Guides the faulter thread to create an event of the correct type.
 *
 * Generates an event of the type needed for the current test. The required
 * event may be the triggering of an instruction breakpoint, a data watchpoint,
 * or a software break request.
 */
static int
breakpoint_triggerer_main(seL4_Word type, seL4_Word size, seL4_Word rw, seL4_Word arg3)
{

    if (type == seL4_InstructionBreakpoint) {
        breakpoint_code();
    } else if (type == seL4_DataBreakpoint) {
        if (rw == seL4_BreakOnRead) {
            UNUSED uint32_t tmp;
            tmp = bpd;
        } else {
            bpd = 0;
        }

        /* If the breakpoint fails to trigger, we can force a fault either way
         * by dereferencing null. The purpose of this is to ensure that if this
         * test fails, the other tests will be allowed to continue.
         */
        volatile int *pi = (volatile int *)BREAKPOINT_TEST_BAD_FAULT_MAGIC_ADDR;
        *pi = 1;
    } else if (type == seL4_SoftwareBreakRequest) {
        TEST_SOFTWARE_BREAK_ASM();
    } else {
        ZF_LOGE("Unknown breakpoint type %zd requested.\n", type);
    }

    return 0;
}

/** Waits for a fault on the endpoint passed to it, returns the fault address.
 *
 * We expect seL4_Fault_DebugException fault type, and any other fault type is an
 * error condition for us. We just store the event data in a static local
 * struct and pass it back to the parent test function.
 */
static int
breakpoint_handler_main(seL4_Word _fault_ep_cspath, seL4_Word a1, seL4_Word a2, seL4_Word a3)
{
    seL4_Word sender_badge;
    seL4_MessageInfo_t tag;
    seL4_Word label;

    /* Wait for the faulter to trigger an event. */
    tag = api_wait(fault_ep_cspath.capPtr, &sender_badge);
    ZF_LOGV("Handler got a fault on the ep.\n");
    label = seL4_MessageInfo_get_label(tag);

    fault_data.vaddr = seL4_GetMR(seL4_DebugException_FaultIP);
    fault_data.reason = seL4_GetMR(seL4_DebugException_ExceptionReason);
    fault_data.vaddr2 = seL4_GetMR(seL4_DebugException_TriggerAddress);
    fault_data.bp_num = seL4_GetMR(seL4_DebugException_BreakpointNumber);

    switch (label) {
    case seL4_Fault_DebugException:
        return 0;
    default:
        ZF_LOGE("Fault of type %zd received. Vaddr %zu\n", label, fault_data.vaddr);
        fault_data.bp_num = 0;
        return -1;
    }
}

static void
setup_handler_thread_for_test(struct env *env, helper_thread_t *handler_thread)
{
    create_helper_thread(env, handler_thread);
    NAME_THREAD(get_helper_tcb(handler_thread), "Handler");
    set_helper_priority(env, handler_thread, BREAKPOINT_TEST_HANDLER_PRIO);
    start_helper(env, handler_thread, &breakpoint_handler_main,
                 (seL4_Word)&fault_ep_cspath,
                 0, 0, 0);
}

static void
cleanup_breakpoints_from_test(struct env *env)
{
    for (int i = TEST_FIRST_INSTR_BP; i < TEST_FIRST_INSTR_BP + TEST_NUM_INSTR_BPS; i++) {
        seL4_TCB_UnsetBreakpoint(env->tcb, i);
    }

    for (int i = TEST_FIRST_DATA_WP; i < TEST_FIRST_DATA_WP + TEST_NUM_DATA_WPS; i++) {
        seL4_TCB_UnsetBreakpoint(env->tcb, i);
    }
}

static int
test_debug_set_instruction_breakpoint(struct env *env)
{
    int error, result;
    helper_thread_t faulter_thread, handler_thread;

    for (seL4_Word i = TEST_FIRST_INSTR_BP; i < TEST_FIRST_INSTR_BP + TEST_NUM_INSTR_BPS;
         i++)
    {
        test_eq(setup_caps_for_test(env),  0);

        setup_handler_thread_for_test(env, &handler_thread);

        error = setup_faulter_thread_for_test(env, &faulter_thread);
        test_eq(error, seL4_NoError);

        ZF_LOGV("Setting BP reg %i to trigger at addr %p.\n", i, &breakpoint_code);
        error = seL4_TCB_SetBreakpoint(get_helper_tcb(&faulter_thread),
                                       i, (seL4_Word)&breakpoint_code,
                                       seL4_InstructionBreakpoint, 0,
                                       seL4_BreakOnRead);
        test_eq(error, seL4_NoError);
        start_helper(env, &faulter_thread, &breakpoint_triggerer_main,
                     seL4_InstructionBreakpoint, 0, seL4_BreakOnRead, 0);
        result = wait_for_helper(&handler_thread);

        cleanup_breakpoints_from_test(env);
        cleanup_helper(env, &faulter_thread);
        cleanup_helper(env, &handler_thread);
        /* Ensure the fault address is the address of the function */
        ZF_LOGV("Instr vaddr %x, reason %d, trigger vaddr %x, bpnum %d.\n",
               fault_data.vaddr, fault_data.reason, fault_data.vaddr2, fault_data.bp_num);
        test_eq(result, 0);
        test_eq(fault_data.bp_num, i);
        test_eq(fault_data.reason, (seL4_Word)seL4_InstructionBreakpoint);
        test_eq(fault_data.vaddr, (seL4_Word)&breakpoint_code);
    }

    return sel4test_get_result();
}
DEFINE_TEST(BREAKPOINT_001, "Attempt to set and trigger an instruction breakpoint",
            test_debug_set_instruction_breakpoint, config_set(CONFIG_HARDWARE_DEBUG_API))

static int
test_debug_set_data_breakpoint(struct env *env)
{
    int error, result;
    helper_thread_t faulter_thread, handler_thread;

    for (seL4_Word i = TEST_FIRST_DATA_WP; i < TEST_FIRST_DATA_WP + TEST_NUM_DATA_WPS;
         i++)
    {
        test_eq(setup_caps_for_test(env), 0);

        setup_handler_thread_for_test(env, &handler_thread);
        setup_faulter_thread_for_test(env, &faulter_thread);
        ZF_LOGV("Setting WP reg %i to trigger at addr %p.\n", i, &bpd);
        error = seL4_TCB_SetBreakpoint(get_helper_tcb(&faulter_thread),
                                       i, (seL4_Word)&bpd,
                                       seL4_DataBreakpoint, 4,
                                       seL4_BreakOnWrite);
        test_eq(error, seL4_NoError);
        start_helper(env, &faulter_thread, &breakpoint_triggerer_main,
                     seL4_DataBreakpoint, 4, seL4_BreakOnWrite, 0);
        result = wait_for_helper(&handler_thread);

        cleanup_breakpoints_from_test(env);
        cleanup_helper(env, &faulter_thread);
        cleanup_helper(env, &handler_thread);
        /* Ensure the fault address is the address of the data */
        ZF_LOGV("Instr vaddr %x, reason %d, trigger vaddr %x, bpnum %d.\n",
               fault_data.vaddr, fault_data.reason, fault_data.vaddr2, fault_data.bp_num);
        test_eq(result, 0);
        test_eq(fault_data.bp_num, i);
        test_eq(fault_data.reason, (seL4_Word)seL4_DataBreakpoint);
        test_eq(fault_data.vaddr2, (seL4_Word)&bpd);
    }
    return sel4test_get_result();
}
DEFINE_TEST(BREAKPOINT_002, "Attempt to set and trigger a data breakpoint",
            test_debug_set_data_breakpoint, config_set(CONFIG_HARDWARE_DEBUG_API))

static int
test_debug_get_instruction_breakpoint(struct env *env)
{
    seL4_Word type = seL4_InstructionBreakpoint,
              size = 0,
              access = seL4_BreakOnRead;
    int error;
    seL4_TCB_GetBreakpoint_t result;

    for (int i = TEST_FIRST_INSTR_BP; i < TEST_FIRST_INSTR_BP + TEST_NUM_INSTR_BPS;
         i++)
    {
        error = seL4_TCB_SetBreakpoint(env->tcb, i,
                                       (seL4_Word)&breakpoint_code,
                                       type, size, access);
        test_eq(error, seL4_NoError);

        result = seL4_TCB_GetBreakpoint(env->tcb, i);
        /* Ensure all parameters reflect what we called it with. */
        test_eq(result.error, seL4_NoError);
        test_eq(result.vaddr, (seL4_Word)&breakpoint_code);
        test_eq(result.type, type);
        test_eq(result.size, size);
        test_eq(result.rw, access);
        test_eq((int)result.is_enabled, true);

        cleanup_breakpoints_from_test(env);
    }
    return sel4test_get_result();
}
DEFINE_TEST(BREAKPOINT_003, "Attempt to set, then get, an instruction breakpoint "
            "expecting that the values returned by GetBreakpoint will match those "
            "set in SetBreakpoint.",
            test_debug_get_instruction_breakpoint, config_set(CONFIG_HARDWARE_DEBUG_API))

static int
test_debug_get_data_breakpoint(struct env *env)
{
    seL4_Word type = seL4_DataBreakpoint,
              size = 2,
              access = seL4_BreakOnWrite;
    int error;
    seL4_TCB_GetBreakpoint_t result;

    for (int i = TEST_FIRST_DATA_WP; i < TEST_FIRST_DATA_WP + TEST_NUM_DATA_WPS;
         i++)
    {
        error = seL4_TCB_SetBreakpoint(env->tcb, i,
                                       (seL4_Word)&bpd,
                                       type, size, access);
        test_eq(error, seL4_NoError);

        result = seL4_TCB_GetBreakpoint(env->tcb, i);
        /* Ensure all parameters reflect what we originally called it with. */
        test_eq(result.error, seL4_NoError);
        test_eq(result.vaddr, (seL4_Word)&bpd);
        test_eq(result.type, type);
        test_eq(result.size, size);
        test_eq(result.rw, access);
        test_eq((int)result.is_enabled, true);

        cleanup_breakpoints_from_test(env);
    }
    return sel4test_get_result();
}
DEFINE_TEST(BREAKPOINT_004, "Attempt to set, then get, a data breakpoint "
            "expecting that the values returned by GetBreakpoint will match those "
            "set in SetBreakpoint.",
            test_debug_get_data_breakpoint, config_set(CONFIG_HARDWARE_DEBUG_API))

static int
test_debug_unset_instruction_breakpoint(struct env *env)
{
    seL4_Word type = seL4_InstructionBreakpoint,
              size = 0,
              access = seL4_BreakOnRead;
    int error;
    seL4_TCB_GetBreakpoint_t result;

    for (int i = TEST_FIRST_INSTR_BP; i < TEST_FIRST_INSTR_BP + TEST_NUM_INSTR_BPS;
         i++)
    {
        error = seL4_TCB_SetBreakpoint(env->tcb, i,
                                       (seL4_Word)&breakpoint_code,
                                       type, size, access);
        test_eq(error, seL4_NoError);

        error = seL4_TCB_UnsetBreakpoint(env->tcb, i);
        test_eq(error, seL4_NoError);

        result = seL4_TCB_GetBreakpoint(env->tcb, i);
        test_eq(result.error, seL4_NoError);
        test_eq((int)result.is_enabled, false);

        cleanup_breakpoints_from_test(env);
    }
    return sel4test_get_result();
}
DEFINE_TEST(BREAKPOINT_005, "Attempt to set, then unset, then query the status of, an "
            "instruction breakpoint", test_debug_unset_instruction_breakpoint, config_set(CONFIG_HARDWARE_DEBUG_API))

static int
test_debug_unset_data_breakpoint(struct env *env)
{
    seL4_Word type = seL4_DataBreakpoint,
              size = 4,
              access = seL4_BreakOnWrite;
    int error;
    seL4_TCB_GetBreakpoint_t result;

    for (int i = TEST_FIRST_DATA_WP; i < TEST_FIRST_DATA_WP + TEST_NUM_DATA_WPS;
         i++)
    {
        error = seL4_TCB_SetBreakpoint(env->tcb, i,
                                       (seL4_Word)&bpd,
                                       type, size, access);
        test_eq(error, seL4_NoError);

        error = seL4_TCB_UnsetBreakpoint(env->tcb, i);
        test_eq(error, seL4_NoError);

        result = seL4_TCB_GetBreakpoint(env->tcb, i);
        test_eq(result.error, seL4_NoError);
        test_eq((int)result.is_enabled, false);

        cleanup_breakpoints_from_test(env);
    }
    return sel4test_get_result();
}
DEFINE_TEST(BREAKPOINT_006, "Attempt to set, then unset, then query the status of, a "
            "data breakpoint", test_debug_unset_data_breakpoint, config_set(CONFIG_HARDWARE_DEBUG_API))

static int
test_debug_api_setbp_invalid_values(struct env *env)
{
    int error;
    seL4_Word kernel_vaddrspace_pointer;

    /* It should be invalid to pass a breakpoint address that refers to data in
     * the kernel's vaddrspace.
     *
     * To construct a kernel vaddr in a "portable" way such that this test will
     * work on both 32 and 64 bit, we NEG a value several mibibytes large. The
     * number 64MiB was chosen arbitrarily.
     */
    kernel_vaddrspace_pointer = (seL4_Word) -(64 * 1024 * 1024);
    error = seL4_TCB_SetBreakpoint(env->tcb, TEST_FIRST_DATA_WP,
                                   kernel_vaddrspace_pointer,
                                   seL4_DataBreakpoint, 0, seL4_BreakOnWrite);
    test_neq(error, seL4_NoError);

    /* It should be invalid to set an instruction breakpoint with an operand
     * size other than 0.
     */
    error = seL4_TCB_SetBreakpoint(env->tcb, TEST_FIRST_INSTR_BP,
                                   (seL4_Word)&breakpoint_code, seL4_InstructionBreakpoint,
                                   2, seL4_BreakOnRead);
    test_neq(error, seL4_NoError);

    /* It should be invalid to set an instruction breakpoint with an access
     * trigger other than seL4_BreakOnRead.
     */
    error = seL4_TCB_SetBreakpoint(env->tcb, TEST_FIRST_INSTR_BP,
                                   (seL4_Word)&breakpoint_code, seL4_InstructionBreakpoint,
                                   0, seL4_BreakOnWrite);
    test_neq(error, seL4_NoError);

    /* It should be invalid to set a data breakpoint with an operand size of 0. */
    error = seL4_TCB_SetBreakpoint(env->tcb, TEST_FIRST_DATA_WP,
                                   (seL4_Word)&bpd, seL4_DataBreakpoint,
                                   0, seL4_BreakOnRead);
    test_neq(error, seL4_NoError);

    /* Can't really write input value tests for GetBreakpoint and UnsetBreakpoint
     * because they only take 2 arguments: a TCB CPtr and a breakpoint ID.
     */
    cleanup_breakpoints_from_test(env);
    return sel4test_get_result();
}
DEFINE_TEST(BREAKPOINT_007, "Attempt to pass various invalid values to the "
            "invocations, and expect error return values.",
            test_debug_api_setbp_invalid_values, config_set(CONFIG_HARDWARE_DEBUG_API))

static int
test_debug_api_software_break_request(struct env *env)
{
    int error, result;
    helper_thread_t faulter_thread, handler_thread;

    test_eq(setup_caps_for_test(env), 0);

    setup_handler_thread_for_test(env, &handler_thread);

    error = setup_faulter_thread_for_test(env, &faulter_thread);
    test_eq(error, seL4_NoError);

    test_eq(error, seL4_NoError);
    start_helper(env, &faulter_thread, &breakpoint_triggerer_main,
                 seL4_SoftwareBreakRequest, 0, seL4_BreakOnRead, 0);
    result = wait_for_helper(&handler_thread);

    cleanup_breakpoints_from_test(env);
    cleanup_helper(env, &faulter_thread);
    cleanup_helper(env, &handler_thread);
    /* Ensure the fault address is the address of the function */
    test_eq(result, 0);
    test_eq(fault_data.reason, (seL4_Word)seL4_SoftwareBreakRequest);
    test_eq(fault_data.vaddr,
            (seL4_Word)&(TEST_SOFTWARE_BREAK_EXPECTED_FAULT_LABEL));
    return sel4test_get_result();
}
DEFINE_TEST(BREAK_REQUEST_001, "Use an INT3/BKPT instruction to trigger a "
            "breakpoint, and ensure the correct message is delivered to the "
            "listening handler.",
            test_debug_api_software_break_request, config_set(CONFIG_HARDWARE_DEBUG_API))

#endif /* CONFIG_HARDWARE_DEBUG_API */
