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

#include <sel4/sel4.h>
#include <utils/attribute.h>

#include <arch/debug.h>
#include "../../../helpers.h"
#include "../../../test.h"
#include "../../../tests/breakpoints.h"

/* Single-step test is currently only for x86. ARM requires some extra
 * components before it can be usable.
 */
static volatile int counter;

NO_INLINE static void
stop_point(void)
{
    /* Counter is volatile so the compiler cannot optimize this away */
    counter = counter;
}

NO_INLINE static void
single_step_guinea_pig(void)
{
    for (; counter < SINGLESTEP_TEST_MAX_LOOP_ITERATIONS; counter++) {
        /* This syscall is inserted here to ensure that syscalls are being
         * stepped over successfully on x86. On ARM, we can just disable single-
         * stepping in PL1 and PL2 altogether, so the problem doesn't arise.
         */
        seL4_Yield();
    }
    stop_point();
}

int debugger_main(seL4_Word a0, seL4_Word reply, seL4_Word a2, seL4_Word a3)
{
    seL4_CPtr debuggee_tcb_cap = a0;
    seL4_Word badge;
    seL4_MessageInfo_t tag;
    seL4_TCB_ConfigureSingleStepping_t result;

    /* The main thread sets a breakpoint in the debuggee thread, and this thread
     * will get a fault on the fault EP. At that point we can enable
     * single-stepping on the debuggee thread.
     */
    tag = api_wait(fault_ep_cspath.capPtr, &badge);

    if (seL4_MessageInfo_get_label(tag) != seL4_Fault_DebugException) {
        ZF_LOGE("debugger: Got unexpected fault %zd.\n",
               seL4_MessageInfo_get_label(tag));
        return -1;
    }

    fault_data.vaddr = seL4_GetMR(seL4_DebugException_FaultIP);
    fault_data.reason = seL4_GetMR(seL4_DebugException_ExceptionReason);
    fault_data.bp_num = seL4_GetMR(seL4_DebugException_BreakpointNumber);
    if (fault_data.reason != seL4_InstructionBreakpoint
            || fault_data.vaddr != (seL4_Word)&single_step_guinea_pig) {
        ZF_LOGE("debugger: debug exception not triggered by expected conditions.\n");
        return -1;
    }

    /* So now we can set the single stepping going. */
    ZF_LOGV("debugger: Instruction BP triggered: beginning single-stepping.\n");
    seL4_TCB_UnsetBreakpoint(debuggee_tcb_cap, TEST_FIRST_INSTR_BP);
    result = seL4_TCB_ConfigureSingleStepping(debuggee_tcb_cap, TEST_FIRST_INSTR_BP + 1, 1);

    test_eq((int)result.bp_was_consumed, SINGLESTEP_EXPECTED_BP_CONSUMPTION_VALUE);
    test_eq(result.error, seL4_NoError);

    /* Resume() the debuggee thread, and keep stepping until it reaches the
     * stop point (when it calls stop_point()).
     *
     * Set the counter to 0. The debuggee thread will be incrementing this.
     * We will check its value afterward to verify that the debuggee ran as
     * expected.
     */
    counter = 0;
    seL4_TCB_Resume(debuggee_tcb_cap);

    for (;;) {
        tag = api_recv(fault_ep_cspath.capPtr, &badge, reply);

        if (seL4_MessageInfo_get_label(tag) != seL4_Fault_DebugException) {
            ZF_LOGE("Debugger: while single stepping, got unexpected fault.\n");
            return -1;
        }

        fault_data.vaddr = seL4_GetMR(seL4_DebugException_FaultIP);
        fault_data.reason = seL4_GetMR(seL4_DebugException_ExceptionReason);
        fault_data.bp_num = seL4_GetMR(seL4_DebugException_TriggerAddress);
        if (fault_data.reason != seL4_SingleStep) {
            ZF_LOGE("Debugger: while single stepping, got debug exception, but "
                   "for the wrong reason (reason %zd).\n",
                   fault_data.reason);
            return -1;
        }

        if (fault_data.vaddr == (seL4_Word)&stop_point) {
            /* We're done: disable single-stepping */
            ZF_LOGV("About to disable stepping and resume debuggee.\n");
            tag = seL4_MessageInfo_set_label(tag, 0);
            seL4_SetMR(0, 0);
            api_reply(reply, tag);
            break;
        }

        tag = seL4_MessageInfo_set_label(tag, 0);
        seL4_SetMR(0, 1);
        api_reply(reply, tag);
    }

    if (fault_data.vaddr != (seL4_Word)&stop_point) {
        ZF_LOGE("Exited loop, but the debuggee thread did not get where we "
               "expected it to.\n");
        return -1;
    }

    /* Test the value of "counter", which the debuggee thread was incrementing. */
    if (counter <= 0 || counter != SINGLESTEP_TEST_MAX_LOOP_ITERATIONS) {
        return -1;
    }

    return 0;
}

int debuggee_main(seL4_Word a0, seL4_Word a1, seL4_Word a2, seL4_Word a3)
{
    ZF_LOGV("Debuggee: about to execute guinea_pig.\n");
    single_step_guinea_pig();
    return 0;
}

static int
test_debug_api_single_step(struct env *env)
{
    helper_thread_t debugger, debuggee;
    int error;

    test_eq(setup_caps_for_test(env), 0);

    create_helper_thread(env, &debugger);
    set_helper_priority(env, &debugger, BREAKPOINT_TEST_HANDLER_PRIO);

    error = setup_faulter_thread_for_test(env, &debuggee);
    test_eq(error, seL4_NoError);
    error = seL4_TCB_SetBreakpoint(get_helper_tcb(&debuggee),
                                   TEST_FIRST_INSTR_BP, (seL4_Word)&single_step_guinea_pig,
                                   seL4_InstructionBreakpoint, 0, seL4_BreakOnRead);
    test_eq(error, seL4_NoError);

    start_helper(env, &debugger, &debugger_main,
                 get_helper_tcb(&debuggee),
                 get_helper_reply(&debuggee), 0, 0);
    start_helper(env, &debuggee, &debuggee_main, 0, 0, 0, 0);

    wait_for_helper(&debugger);
    wait_for_helper(&debuggee);

    cleanup_helper(env, &debuggee);
    cleanup_helper(env, &debugger);
    return sel4test_get_result();
}
DEFINE_TEST(SINGLESTEP_001, "Attempt to step through a function",
            test_debug_api_single_step, config_set(CONFIG_HARDWARE_DEBUG_API));

#endif /* #ifdef CONFIG_HARDWARE_DEBUG_API */
