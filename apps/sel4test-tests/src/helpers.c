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

#include <sel4/sel4.h>
#include <sel4utils/arch/util.h>
#include <sel4utils/helpers.h>

#include <sel4test/test.h>
#include <stdarg.h>
#include <stdlib.h>

#include <utils/util.h>
#include <vka/capops.h>

#include <sel4platsupport/timer.h>

#include "helpers.h"
#include "test.h"

int
check_zeroes(seL4_Word addr, seL4_Word size_bytes)
{
    test_assert_fatal(IS_ALIGNED(addr, sizeof(seL4_Word)));
    test_assert_fatal(IS_ALIGNED(size_bytes, sizeof(seL4_Word)));
    seL4_Word *p = (void*)addr;
    seL4_Word size_words = size_bytes / sizeof(seL4_Word);
    while (size_words--) {
        if (*p++ != 0) {
            ZF_LOGE("Found non-zero at position %ld: %lu\n", ((long)p) - (addr), (unsigned long)p[-1]);
            return 0;
        }
    }
    return 1;
}

/* Determine whether a given slot in the init thread's CSpace is empty by
 * examining the error when moving a slot onto itself.
 *
 * Serves as == 0 comparator for caps.
 */
int
is_slot_empty(env_t env, seL4_Word slot)
{
    int error;

    error = cnode_move(env, slot, slot);

    assert(error == seL4_DeleteFirst || error == seL4_FailedLookup);
    return (error == seL4_FailedLookup);
}

seL4_Word
get_free_slot(env_t env)
{
    seL4_CPtr slot;
    UNUSED int error = vka_cspace_alloc(&env->vka, &slot);
    assert(!error);
    return slot;
}

int
cnode_copy(env_t env, seL4_CPtr src, seL4_CPtr dest, seL4_CapRights_t rights)
{
    cspacepath_t src_path, dest_path;
    vka_cspace_make_path(&env->vka, src, &src_path);
    vka_cspace_make_path(&env->vka, dest, &dest_path);
    return vka_cnode_copy(&dest_path, &src_path, rights);
}

int
cnode_delete(env_t env, seL4_CPtr slot)
{
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, slot, &path);
    return vka_cnode_delete(&path);
}

int
cnode_mint(env_t env, seL4_CPtr src, seL4_CPtr dest, seL4_CapRights_t rights, seL4_Word badge)
{
    cspacepath_t src_path, dest_path;

    vka_cspace_make_path(&env->vka, src, &src_path);
    vka_cspace_make_path(&env->vka, dest, &dest_path);
    return vka_cnode_mint(&dest_path, &src_path, rights, badge);
}

int
cnode_move(env_t env, seL4_CPtr src, seL4_CPtr dest)
{
    cspacepath_t src_path, dest_path;

    vka_cspace_make_path(&env->vka, src, &src_path);
    vka_cspace_make_path(&env->vka, dest, &dest_path);
    return vka_cnode_move(&dest_path, &src_path);
}

int
cnode_mutate(env_t env, seL4_CPtr src, seL4_CPtr dest)
{
    cspacepath_t src_path, dest_path;

    vka_cspace_make_path(&env->vka, src, &src_path);
    vka_cspace_make_path(&env->vka, dest, &dest_path);
    return vka_cnode_mutate(&dest_path, &src_path, seL4_NilData);
}

int
cnode_cancelBadgedSends(env_t env, seL4_CPtr cap)
{
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, cap, &path);
    return vka_cnode_cancelBadgedSends(&path);
}

int
cnode_revoke(env_t env, seL4_CPtr cap)
{
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, cap, &path);
    return vka_cnode_revoke(&path);
}

int
cnode_rotate(env_t env, seL4_CPtr src, seL4_CPtr pivot, seL4_CPtr dest)
{
    cspacepath_t src_path, dest_path, pivot_path;

    vka_cspace_make_path(&env->vka, src, &src_path);
    vka_cspace_make_path(&env->vka, dest, &dest_path);
    vka_cspace_make_path(&env->vka, pivot, &pivot_path);
    return vka_cnode_rotate(&dest_path, seL4_NilData, &pivot_path, seL4_NilData, &src_path);
}

int
are_tcbs_distinct(seL4_CPtr tcb1, seL4_CPtr tcb2)
{
    seL4_UserContext regs;

    /* Initialise regs to prevent compiler warning. */
    int error = seL4_TCB_ReadRegisters(tcb1, 0, 0, 1, &regs);
    if (error) {
        return -1;
    }

    for (int i = 0; i < 2; ++i) {
        sel4utils_set_instruction_pointer(&regs, i);
        error = seL4_TCB_WriteRegisters(tcb1, 0, 0, 1, &regs);

        /* Check that we had permission to do that and the cap was a TCB. */
        if (error) {
            return -1;
        }

        error = seL4_TCB_ReadRegisters(tcb2, 0, 0, 1, &regs);

        /* Check that we had permission to do that and the cap was a TCB. */
        if (error) {
            return -1;
        } else if (sel4utils_get_instruction_pointer(regs) != i) {
            return 1;
        }

    }

    return 0;
}

void
create_helper_process(env_t env, helper_thread_t *thread)
{
    UNUSED int error;

    error = vka_alloc_endpoint(&env->vka, &thread->local_endpoint);
    assert(error == 0);

    thread->is_process = true;

    sel4utils_process_config_t config = process_config_default_simple(&env->simple, "", OUR_PRIO - 1);
    config = process_config_asid_pool(config, env->asid_pool);
    config = process_config_noelf(config, NULL, 0);
    config = process_config_create_vspace(config, env->regions, env->num_regions);
    vka_object_t fault_endpoint = { .cptr = env->endpoint };
    config = process_config_fault_endpoint(config, fault_endpoint);
    error = sel4utils_configure_process_custom(&thread->process, &env->vka, &env->vspace,
                                               config);
    assert(error == 0);

    /* copy the elf reservations we need into the current process */
    memcpy(thread->regions, env->regions, sizeof(sel4utils_elf_region_t) * env->num_regions);
    thread->num_regions = env->num_regions;

    /* clone data/code into vspace */
    for (int i = 0; i < env->num_regions; i++) {
        error = sel4utils_bootstrap_clone_into_vspace(&env->vspace, &thread->process.vspace, thread->regions[i].reservation);
        assert(error == 0);
    }

    thread->thread = thread->process.thread;
    assert(error == 0);
}

NORETURN static void
signal_helper_finished(seL4_CPtr local_endpoint, int val)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 1);
    seL4_SetMR(0, val);
    while (true) {
        seL4_Call(local_endpoint, info);
    }
}

NORETURN static void
helper_thread(int argc, char **argv)
{

    helper_fn_t entry_point = (void *) atol(argv[0]);
    seL4_CPtr local_endpoint = (seL4_CPtr) atol(argv[1]);

    seL4_Word args[HELPER_THREAD_MAX_ARGS] = {0};
    for (int i = 2; i < argc && i - 2 < HELPER_THREAD_MAX_ARGS; i++) {
        assert(argv[i] != NULL);
        args[i - 2] = atol(argv[i]);
    }

    /* run the thread */
    int result = entry_point(args[0], args[1], args[2], args[3]);
    signal_helper_finished(local_endpoint, result);
    /* does not return */
}

extern uintptr_t sel4_vsyscall[];

void
start_helper(env_t env, helper_thread_t *thread, helper_fn_t entry_point,
             seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3)
{

    UNUSED int error;

    seL4_CPtr local_endpoint;

    if (thread->is_process) {
        /* copy the local endpoint */
        cspacepath_t path;
        vka_cspace_make_path(&env->vka, thread->local_endpoint.cptr, &path);
        local_endpoint = sel4utils_copy_path_to_process(&thread->process, path);
    } else {
        local_endpoint = thread->local_endpoint.cptr;
    }

    sel4utils_create_word_args(thread->args_strings, thread->args, HELPER_THREAD_TOTAL_ARGS,
        (seL4_Word) entry_point, local_endpoint,
        arg0, arg1, arg2, arg3);

    if (thread->is_process) {
        thread->process.entry_point = (void*)helper_thread;
        error = sel4utils_spawn_process_v(&thread->process, &env->vka, &env->vspace,
                                        HELPER_THREAD_TOTAL_ARGS, thread->args, 0);
        assert(error == 0);
        /* sel4utils_spawn_process_v has created a stack frame that contains, amongst other
           things, our arguments. Since we are going to be running a clone of this vspace
           we would like to not call _start as this will result in initializing the C library
           a second time. As we know the argument count and where argv will start we can
           construct a register (or stack) layout that will allow us to pretend to be doing
           a function call to helper_thread. */
        seL4_UserContext context;
        uintptr_t argv_base = (uintptr_t)thread->process.thread.initial_stack_pointer + sizeof(long);
        uintptr_t aligned_stack_pointer = ALIGN_DOWN((uintptr_t)thread->process.thread.initial_stack_pointer, STACK_CALL_ALIGNMENT);
        error = sel4utils_arch_init_context_with_args((sel4utils_thread_entry_fn)helper_thread, (void*)HELPER_THREAD_TOTAL_ARGS,
                                              (void*)argv_base, NULL, false, (void*)aligned_stack_pointer,
                                              &context, &env->vka, &env->vspace, &thread->process.vspace);
        assert(error == 0);
        error = seL4_TCB_WriteRegisters(thread->process.thread.tcb.cptr, 1, 0, sizeof(seL4_UserContext) / sizeof(seL4_Word), &context);
        assert(error == 0);
    } else {
        error = sel4utils_start_thread(&thread->thread, (sel4utils_thread_entry_fn)helper_thread,
                                       (void *) HELPER_THREAD_TOTAL_ARGS, (void *) thread->args, 1);
        assert(error == 0);
    }
}

void
cleanup_helper(env_t env, helper_thread_t *thread)
{

    vka_free_object(&env->vka, &thread->local_endpoint);

    if (thread->is_process) {
        /* free the regions (no need to unmap, as the
        * entry address space / cspace is being destroyed */
        for (int i = 0; i < thread->num_regions; i++) {
            vspace_free_reservation(&thread->process.vspace, thread->regions[i].reservation);
        }

        thread->process.fault_endpoint.cptr = 0;
        sel4utils_destroy_process(&thread->process, &env->vka);
    } else {
        sel4utils_clean_up_thread(&env->vka, &env->vspace, &thread->thread);
    }
}

void
create_helper_thread(env_t env, helper_thread_t *thread)
{
    UNUSED int error;

    error = vka_alloc_endpoint(&env->vka, &thread->local_endpoint);
    assert(error == 0);

    thread->is_process = false;
    thread->fault_endpoint = env->endpoint;
    seL4_Word data = api_make_guard_skip_word(seL4_WordBits - env->cspace_size_bits);
    sel4utils_thread_config_t config = thread_config_default(&env->simple, env->cspace_root, data, env->endpoint, OUR_PRIO - 1);

    error = sel4utils_configure_thread_config(&env->vka, &env->vspace, &env->vspace,
                                              config, &thread->thread);
    assert(error == 0);
}

int
wait_for_helper(helper_thread_t *thread)
{
    seL4_Word badge;

    api_recv(thread->local_endpoint.cptr, &badge, thread->thread.reply.cptr);
    return seL4_GetMR(0);
}

void
set_helper_priority(env_t env, helper_thread_t *thread, seL4_Word prio)
{
    UNUSED int error;
    error = seL4_TCB_SetPriority(thread->thread.tcb.cptr, env->tcb, prio);
    assert(error == seL4_NoError);
}

void
set_helper_mcp(env_t env, helper_thread_t *thread, seL4_Word mcp)
{
    UNUSED int error;
    error = seL4_TCB_SetMCPriority(thread->thread.tcb.cptr, env->tcb, mcp);
    assert(error == seL4_NoError);
}

void
set_helper_affinity(UNUSED env_t env, helper_thread_t *thread, seL4_Word affinity)
{
#ifdef CONFIG_KERNEL_RT
    seL4_Time timeslice = CONFIG_BOOT_THREAD_TIME_SLICE * US_IN_S;
    int error = seL4_SchedControl_Configure(simple_get_sched_ctrl(&env->simple, affinity),
                                       thread->thread.sched_context.cptr,
                                       timeslice, timeslice, 0, 0);
    ZF_LOGF_IF(error, "Failed to configure scheduling context");
#elif CONFIG_MAX_NUM_NODES > 1
    int error = seL4_TCB_SetAffinity(thread->thread.tcb.cptr, affinity);
    ZF_LOGF_IF(error, "Failed to set tcb affinity");
#endif
}

seL4_CPtr
get_helper_tcb(helper_thread_t *thread)
{
    return thread->thread.tcb.cptr;
}

seL4_CPtr
get_helper_reply(helper_thread_t *thread)
{
    return thread->thread.reply.cptr;
}

seL4_CPtr
get_helper_sched_context(helper_thread_t *thread)
{
    return thread->thread.sched_context.cptr;
}

uintptr_t
get_helper_ipc_buffer_addr(helper_thread_t *thread)
{
    return thread->thread.ipc_buffer_addr;
}

uintptr_t
get_helper_initial_stack_pointer(helper_thread_t *thread)
{
    return (uintptr_t)thread->thread.initial_stack_pointer;
}

static void
sel4test_send_time_request(seL4_CPtr ep, uint64_t ns, sel4test_output_t request_type, timeout_type_t timeout_type)
{
    seL4_MessageInfo_t tag;
    seL4_SetMR(0, request_type);

    switch(request_type) {
        case SEL4TEST_TIME_TIMEOUT:
            seL4_SetMR(1, timeout_type);
            sel4utils_64_set_mr(2, ns);
            tag = seL4_MessageInfo_new(0, 0, 0, (seL4_Uint32) SEL4UTILS_64_WORDS + 2);
            break;
        case SEL4TEST_TIME_TIMESTAMP:
        case SEL4TEST_TIME_RESET:
             tag = seL4_MessageInfo_new(0, 0, 0, 1);
            break;
        default:
            ZF_LOGE("Invalid time request\n");
            break;
    }

    seL4_Call(ep, tag);
}

void sleep_busy(env_t env, uint64_t ns) {
    uint64_t start = sel4test_timestamp(env);
    uint64_t now = sel4test_timestamp(env);
    int same = 0;
    while (now < start + ns) {
        if (now == start) {
            same++;
            if (same == 10000) {
                ZF_LOGE("Timer hasn't moved in 10000 iterations, are you handling interrupts?");
            }
        } else {
            same = 0;
        }
        now = sel4test_timestamp(env);
    }
}

inline void
sel4test_sleep(env_t env, uint64_t ns)
{
    /*
     * sleep is meant to block the calling thread for at least @ns. RPC costs and
     * delivering timer notifications are not accounted for. Due to the fact that
     * sleep requests are RPC calls on the same env->ep, only one test can request a sleep
     * on a time. It is possible, however, that 2 (or more) threads request a sleep,
     * being serialised, and wait on the same env->timer_notification at the same time,
     * in which case the first thread in the queue will only be notified and not the
     * other(s). This is a limitation, and the current interface won't handle it. Only
     * one thread can request/wait/sleep/wakeup on a time.
     */

    sel4test_send_time_request(env->endpoint, ns, SEL4TEST_TIME_TIMEOUT, TIMEOUT_RELATIVE);
    /* The tests have a timer_notification that they can wait on by default.
     * sel4-driver will notify us on timer_notification when it gets a timer interrupt
     */
    seL4_Wait(env->timer_notification.cptr, NULL);
}

inline void
sel4test_periodic_start(env_t env, uint64_t ns)
{
    sel4test_send_time_request(env->endpoint, ns, SEL4TEST_TIME_TIMEOUT, TIMEOUT_PERIODIC);
}

uint64_t
sel4test_timestamp(env_t env)
{
    /*
     * Request a timestamp from sel4test-driver. The request is sent over the fault ep,
     * and, being synchronous, sel4test-driver sends back the timestamp in the RPC reply.
     * RPC costs are not accounted for.
     */
    uint64_t time = 0;

    sel4test_send_time_request(env->endpoint, 0, SEL4TEST_TIME_TIMESTAMP, 0);
    time = sel4utils_64_get_mr(1);

    return time;
}

inline void
sel4test_timer_reset(env_t env)
{
    sel4test_send_time_request(env->endpoint, 0, SEL4TEST_TIME_RESET, 0);
}

inline void
sel4test_ntfn_timer_wait(env_t env)
{
    seL4_Wait(env->timer_notification.cptr, NULL);
}
int
set_helper_sched_params(UNUSED env_t env, UNUSED helper_thread_t *thread, UNUSED uint64_t budget,
        UNUSED uint64_t period, UNUSED seL4_Word badge)
{
    seL4_Word refills = 0;
    if (budget < period) {
#ifdef CONFIG_KERNEL_RT
        refills = seL4_MaxExtraRefills(seL4_MinSchedContextBits);
#endif
    }
    return api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0),
                                       thread->thread.sched_context.cptr,
                                       budget, period, refills, badge);
}

int create_passive_thread(env_t env, helper_thread_t *passive, helper_fn_t fn, seL4_CPtr ep,
                          seL4_Word arg1, seL4_Word arg2, seL4_Word arg3)
{
    create_helper_thread(env, passive);
    return start_passive_thread(env, passive, fn, ep, arg1, arg2, arg3);
}

int start_passive_thread(env_t env, helper_thread_t *passive, helper_fn_t fn, seL4_CPtr ep,
                          seL4_Word arg1, seL4_Word arg2, seL4_Word arg3)
{
    start_helper(env, passive, fn, ep, arg1, arg2, arg3);

    /* Wait for helper to signal it has initialised */
    ZF_LOGD("Wait for passive thread to init");
    seL4_Wait(ep, NULL);
    ZF_LOGD("Done");
    /* convert to passive */
    return api_sc_unbind(passive->thread.sched_context.cptr);
}

int
restart_after_syscall(env_t env, helper_thread_t *helper)
{
    /* save and resume helper->*/
    seL4_UserContext regs;

    int error = seL4_TCB_ReadRegisters(helper->thread.tcb.cptr, false, 0,
                                   sizeof(seL4_UserContext) / sizeof(seL4_Word), &regs);
    test_eq(error, seL4_NoError);

    /* skip the call */
    sel4utils_set_instruction_pointer(&regs, sel4utils_get_instruction_pointer(regs) + ARCH_SYSCALL_INSTRUCTION_SIZE);


    error = seL4_TCB_WriteRegisters(helper->thread.tcb.cptr, true, 0,
                                    sizeof(seL4_UserContext) / sizeof(seL4_Word), &regs);
    test_eq(error, seL4_NoError);

    return 0;
}

void
set_helper_tfep(env_t env, helper_thread_t *thread, seL4_CPtr tfep)
{
    ZF_LOGF_IF(!config_set(CONFIG_KERNEL_RT), "Unsupported on non MCS kernel");
#ifdef CONFIG_KERNEL_RT
    int error = seL4_TCB_SetTimeoutEndpoint(thread->thread.tcb.cptr, tfep);
    if (error != seL4_NoError) {
        ZF_LOGF("Failed to set tfep\n");
    }
#endif
}
