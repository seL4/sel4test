/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <sel4test-driver/gen_config.h>

/* This file contains tests related to multicore. */

#include <stdio.h>
#include <string.h>
#include <sel4/sel4.h>
#include <vka/object.h>
#include <sel4utils/mapping.h>

#include "../helpers.h"

static int counter_func(_Atomic seL4_Word *counter)
{
    while (1) {
        (*counter)++;
    }
    return 0;
}

int smp_test_tcb_resume(env_t env)
{
    helper_thread_t t1;
    _Atomic seL4_Word counter;
    ZF_LOGD("smp_test_tcb_resume\n");
    create_helper_thread(env, &t1);

    set_helper_priority(env, &t1, 100);
    start_helper(env, &t1, (helper_fn_t) counter_func, (seL4_Word) &counter, 0, 0, 0);

    seL4_Word old_counter;

    /* Let the counter thread run. */
    sel4test_sleep(env, 10 * NS_IN_MS);

    old_counter = counter;

    /* Let it run again on the current core. */
    sel4test_sleep(env, 10 * NS_IN_MS);

    /* Now, counter should have moved. */
    test_check(counter != old_counter);

    /* Suspend the thread, and move it to new core. */
    seL4_TCB_Suspend(get_helper_tcb(&t1));
    set_helper_affinity(env, &t1, 1);

    old_counter = counter;

    /* Check if the thread is running. */
    sel4test_sleep(env, 10 * NS_IN_MS);

    /* Counter should not have moved. */
    test_check(counter == old_counter);
    old_counter = counter;

    /* Resume the thread and check it does move. */
    seL4_TCB_Resume(get_helper_tcb(&t1));
    sel4test_sleep(env, 10 * NS_IN_MS);
    test_check(counter != old_counter);

    /* Suspend the thread. */
    seL4_TCB_Suspend(get_helper_tcb(&t1));

    old_counter = counter;

    /* Check if the thread is running. */
    sel4test_sleep(env, 10 * NS_IN_MS);

    /* Counter should not have moved. */
    test_check(counter == old_counter);

    /* Done. */
    cleanup_helper(env, &t1);

    return sel4test_get_result();
}
DEFINE_TEST(MULTICORE0001, "Test suspending and resuming a thread on different core", smp_test_tcb_resume,
            config_set(CONFIG_HAVE_TIMER) &&CONFIG_MAX_NUM_NODES > 1)

int smp_test_tcb_move(env_t env)
{
    helper_thread_t t1;
    _Atomic seL4_Word counter;
    ZF_LOGD("smp_test_tcb_move\n");
    create_helper_thread(env, &t1);

    set_helper_priority(env, &t1, 100);
    start_helper(env, &t1, (helper_fn_t) counter_func, (seL4_Word) &counter, 0, 0, 0);

    seL4_Word old_counter;

    old_counter = counter;

    /* Let it run on the current core. */
    sleep_busy(env, 10 * NS_IN_MS);

    /* Now, counter should not have moved. */
    test_check(counter == old_counter);

    for (int i = 1; i < env->cores; i++) {
        set_helper_affinity(env, &t1, i);

        old_counter = counter;

        /* Check if the thread is running. */
        sleep_busy(env, 10 * NS_IN_MS);

        /* Counter should have moved. */
        test_check(counter != old_counter);
    }

    /* Done. */
    cleanup_helper(env, &t1);

    return sel4test_get_result();
}
DEFINE_TEST(MULTICORE0002, "Test thread is runnable on all available cores (0 + other)", smp_test_tcb_move,
            config_set(CONFIG_HAVE_TIMER) &&CONFIG_MAX_NUM_NODES > 1)

int smp_test_tcb_delete(env_t env)
{
    helper_thread_t t1;
    _Atomic seL4_Word counter;
    ZF_LOGD("smp_test_tcb_delete\n");
    create_helper_thread(env, &t1);

    set_helper_priority(env, &t1, 100);
    start_helper(env, &t1, (helper_fn_t) counter_func, (seL4_Word) &counter, 0, 0, 0);

    seL4_Word old_counter;

    old_counter = counter;

    /* Let it run on the current core. */
    sleep_busy(env, 10 * NS_IN_MS);

    /* Now, counter should not have moved. */
    test_check(counter == old_counter);

    set_helper_affinity(env, &t1, 1);

    old_counter = counter;

    /* Check if the thread is running. */
    sleep_busy(env, 10 * NS_IN_MS);

    /* Counter should have moved. */
    test_check(counter != old_counter);

    /* Now delete the helper thread running on another core */
    cleanup_helper(env, &t1);

    old_counter = counter;

    /* Check if the thread is running. */
    sleep_busy(env, 10 * NS_IN_MS);

    /* Now, counter should not have moved. */
    test_check(counter == old_counter);

    /* Done. */
    return sel4test_get_result();
}

DEFINE_TEST(MULTICORE0005, "Test remote delete thread running on other cores", smp_test_tcb_delete,
            config_set(CONFIG_HAVE_TIMER) &&CONFIG_MAX_NUM_NODES > 1)

static int
faulter_func(volatile seL4_Word shared_mem)
{
    volatile seL4_Word *page;

    page = (volatile seL4_Word *)shared_mem;

    /* Accessing to the new page... */
    while (1) {
        *page = 1;
    }

    return 0;
}

static int handler_func(seL4_CPtr fault_ep, _Atomic seL4_Word *pf)
{
    seL4_MessageInfo_t tag;
    seL4_Word sender_badge = 0;

    /* Waiting for fault from faulter */
    tag = api_wait(fault_ep, &sender_badge);
    *pf = seL4_MessageInfo_get_label(tag);
    return 0;
}

static int smp_test_tlb_instance(env_t env, bool inter_as)
{
    int error;
    _Atomic seL4_Word tag;
    volatile seL4_Word shared_mem = 0;
    ZF_LOGD("smp_test_tlb\n");

    helper_thread_t handler_thread;
    helper_thread_t faulter_thread;
    vspace_t *vspace;
    seL4_CPtr faulter_vspace, faulter_cspace;
    seL4_CPtr fault_ep = vka_alloc_endpoint_leaky(&env->vka);
    create_helper_thread(env, &handler_thread);
    set_helper_priority(env, &handler_thread, 100);

    seL4_CPtr fault_ep_faulter = fault_ep;
    if (inter_as) {
        create_helper_process(env, &faulter_thread);

        /* copy the fault endpoint to the faulter */
        cspacepath_t path;
        vka_cspace_make_path(&env->vka,  fault_ep, &path);
        seL4_CPtr remote_fault_ep = sel4utils_copy_path_to_process(&faulter_thread.process, path);
        assert(remote_fault_ep != -1);

        if (!config_set(CONFIG_KERNEL_MCS)) {
            fault_ep_faulter = remote_fault_ep;
        }

        faulter_cspace = faulter_thread.process.cspace.cptr;
        faulter_vspace = faulter_thread.process.pd.cptr;
        vspace = &faulter_thread.process.vspace;
    } else {
        create_helper_thread(env, &faulter_thread);
        faulter_cspace = env->cspace_root;
        faulter_vspace = env->page_directory;
        vspace = &env->vspace;
    }

    error = api_tcb_set_space(get_helper_tcb(&faulter_thread),
                              fault_ep_faulter,
                              faulter_cspace,
                              api_make_guard_skip_word(seL4_WordBits - env->cspace_size_bits),
                              faulter_vspace, seL4_NilData);
    test_error_eq(error, seL4_NoError);

    /* Move handler to core 1 and faulter to the last available core */
    set_helper_affinity(env, &handler_thread, 1);
    set_helper_affinity(env, &faulter_thread, env->cores - 1);

    /* Map new page to shared address space */
    shared_mem = (seL4_Word) vspace_new_pages(vspace, seL4_AllRights, 1, seL4_PageBits);

    start_helper(env, &handler_thread, (helper_fn_t) handler_func, fault_ep, (seL4_Word) &tag, 0, 0);
    start_helper(env, &faulter_thread, (helper_fn_t) faulter_func, (seL4_Word) shared_mem, 0, 0, 0);

    /* Wait for some access... */
    sel4test_sleep(env, 10 * NS_IN_MS);

    /* Unmap the page */
    vspace_unmap_pages(vspace, (void *) shared_mem, 1, seL4_PageBits, &env->vka);

    /* Wait for some access... */
    sel4test_sleep(env, CONFIG_TIMER_TICK_MS * NS_IN_MS / 10);

    /* We should see page fault */
    test_check(tag == seL4_Fault_VMFault);

    /* Done. */
    cleanup_helper(env, &faulter_thread);
    cleanup_helper(env, &handler_thread);
    return sel4test_get_result();
}

int smp_test_tlb(env_t env)
{
    test_result_t result;
    /* Test unmapping a frame from the same VSpace and different VSpace. */
    for (int i = 0; i < 20; i++) {
        bool inter_as = (i % 2 == 0) ? true : false;
        result = smp_test_tlb_instance(env, inter_as);
        if (result != SUCCESS) {
            return result;
        }
    }
    return result;

}
DEFINE_TEST(MULTICORE0003, "Test TLB invalidated cross cores", smp_test_tlb,
            config_set(CONFIG_HAVE_TIMER) &&CONFIG_MAX_NUM_NODES > 1)

static int
kernel_entry_func(seL4_Word *unused)
{
    while (1) {
        seL4_Yield();
    }
    return 0;
}

int smp_test_tcb_clh(env_t env)
{
    helper_thread_t t[env->cores];
    ZF_LOGD("smp_test_tcb_move\n");

    for (int i = 1; i < env->cores; i++) {
        create_helper_thread(env, &t[i]);

        set_helper_affinity(env, &t[i], i);
        start_helper(env, &t[i], (helper_fn_t) kernel_entry_func, (seL4_Word) NULL, 0, 0, 0);
    }

    /* All threads start calling 'seL4_Yield', which results in a queue to be generated in CLH lock.
     * By the time we are trying to clean up threads, they should be already in CLH queue which
     * result the delay release of the lock and stalling of the core.
     *
     * There is no failing here.
     * If something is worng with IPI or lock handling we would stuck or possibly crash! */

    /* We should be able to cleanup all threads */
    for (int i = 1; i < env->cores; i++) {
        cleanup_helper(env, &t[i]);
    }

    /* Do this again... */
    for (int i = 1; i < env->cores; i++) {
        create_helper_thread(env, &t[i]);

        set_helper_affinity(env, &t[i], i);
        start_helper(env, &t[i], (helper_fn_t) kernel_entry_func, (seL4_Word) NULL, 0, 0, 0);
    }

    /* We should be able to cleanup all threads */
    for (int i = 1; i < env->cores; i++) {
        cleanup_helper(env, &t[i]);
    }

    return sel4test_get_result();
}
DEFINE_TEST(MULTICORE0004, "Test core stalling is behaving properly (flaky)", smp_test_tcb_clh,
            CONFIG_MAX_NUM_NODES > 1)

#ifdef CONFIG_ARCH_X86_64

#define STALE_CALL_ITERATIONS 200000
/* Counter steps between kernel entries: enough to still be counting when
 * the test core reads the registers after a stall that did not happen. */
#define STALE_CALL_STEPS      256
/* Range of the busy-wait before each register read, in loop iterations. */
#define STALE_CALL_DELAYS     32768

typedef struct stale_call_state {
    seL4_Word stop;
    seL4_Word seen;
    seL4_Word rewound;
    seL4_CPtr ep;
} stale_call_state_t;

/* Count in r14 and check that the count never goes back, as it would if
 * the test core wrote back a register image that it read while this thread
 * was still running. Enter the kernel with NBSend to an endpoint without
 * receivers every STALE_CALL_STEPS steps, to wait for the kernel lock
 * while the test core makes remote calls. Nothing here uses the stack: the
 * stack pointer is restored from rbx after each system call. */
static int stale_call_counter(seL4_Word state_arg, UNUSED seL4_Word arg2, UNUSED seL4_Word arg3,
                              UNUSED seL4_Word arg4)
{
    stale_call_state_t *state = (stale_call_state_t *)state_arg;

    asm volatile(
        "    movq    %[state], %%r13       \n"
        "    movq    8(%%r13), %%r14       \n"
        "1:  movq    %[steps], %%r12       \n"
        "4:  incq    %%r14                 \n"
        "    cmpq    8(%%r13), %%r14       \n"
        "    jbe     2f                    \n"
        "    movq    %%r14, 8(%%r13)       \n"
        "    decq    %%r12                 \n"
        "    jnz     4b                    \n"
        "    cmpq    $0, 0(%%r13)          \n"
        "    jne     3f                    \n"
        "    movq    24(%%r13), %%rdi      \n"
        "    xorl    %%esi, %%esi          \n"
        "    movq    %[sys], %%rdx         \n"
        "    movq    %%rsp, %%rbx          \n"
        "    syscall                       \n"
        "    movq    %%rbx, %%rsp          \n"
        "    jmp     1b                    \n"
        "2:  movq    %%r14, 16(%%r13)      \n"
        "3:                                \n"
        :
        : [state] "r"(state), [sys] "i"(seL4_SysNBSend), [steps] "i"(STALE_CALL_STEPS)
        : "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
        "memory", "cc"
    );
    return SUCCESS;
}

/* Regression test for remote calls taken from a late IPI. A core that runs
 * a remote call from the kernel lock's wait loop still receives the
 * hardware IPI of that call later, on its way back to user level. If that
 * IPI is handled while the next remote call is being set up, the kernel
 * must not run the previous call's operation in the new round. Here the
 * previous call is a TLB invalidation and the next one stalls the core of
 * a running thread to read its registers and suspend it: run in its place,
 * the invalidation leaves the thread running, so the registers read are
 * those of its last kernel entry, and writing them back rewinds it.
 *
 * The race is narrow: the late IPI must read the previous call between the
 * test core's write of the next call and its write of the ipi flag, and the
 * reading core checks the flag only a few instructions after reading the
 * call. The test can only make this likely, not certain: it fails when the
 * reading core is delayed inside that window, e.g. by a VM exit under
 * QEMU/KVM, and may pass on an affected kernel, in particular on hardware. */
static int smp_test_stale_remote_call(env_t env)
{
    test_assert(env->cores > 1);
    int counters = env->cores - 1;
    helper_thread_t threads[counters];
    stale_call_state_t states[counters];
    seL4_UserContext regs;
    const seL4_Word nregs = sizeof(regs) / sizeof(seL4_Word);
    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);
    bool mapped = true;

    /* Mapping and unmapping this page invalidates the TLBs of every core
     * that this VSpace has run on. */
    void *page = vspace_new_pages(&env->vspace, seL4_AllRights, 1, seL4_PageBits);
    test_assert(page != NULL);
    seL4_CPtr page_cap = vspace_get_cap(&env->vspace, page);

    memset(states, 0, sizeof(states));
    for (int i = 0; i < counters; i++) {
        states[i].ep = ep;
        create_helper_thread(env, &threads[i]);
        set_helper_affinity(env, &threads[i], i + 1);
        start_helper(env, &threads[i], stale_call_counter, (seL4_Word) &states[i], 0, 0, 0);
    }

    for (int it = 0; it < STALE_CALL_ITERATIONS; it++) {
        int i = it % counters;
        seL4_CPtr tcb = get_helper_tcb(&threads[i]);

        if (__atomic_load_n(&states[i].rewound, __ATOMIC_ACQUIRE)) {
            break;
        }
        if (mapped) {
            test_error_eq(seL4_ARCH_Page_Unmap(page_cap), seL4_NoError);
        } else {
            test_error_eq(seL4_ARCH_Page_Map(page_cap, env->page_directory, (seL4_Word) page,
                                             seL4_AllRights, seL4_ARCH_Default_VMAttributes), seL4_NoError);
        }
        mapped = !mapped;
        /* Vary when the next call is set up relative to the late IPI of the
         * invalidation, whose delivery time depends on the platform. */
        for (volatile int d = 0; d < (it / counters) % STALE_CALL_DELAYS; d++);
        /* The thread stays suspended from the read to the write, so writing
         * back what was read changes nothing. */
        test_error_eq(seL4_TCB_ReadRegisters(tcb, true, 0, nregs, &regs), seL4_NoError);
        test_error_eq(seL4_TCB_WriteRegisters(tcb, true, 0, nregs, &regs), seL4_NoError);
    }

    for (int i = 0; i < counters; i++) {
        __atomic_store_n(&states[i].stop, 1, __ATOMIC_RELEASE);
    }
    for (int i = 0; i < counters; i++) {
        test_eq(wait_for_helper(&threads[i]), SUCCESS);
        cleanup_helper(env, &threads[i]);
        printf("core %d: counted to %lu, rewound to %lu\n", i + 1,
               (unsigned long) states[i].seen, (unsigned long) states[i].rewound);
        test_check(states[i].seen > 0);
        test_eq(states[i].rewound, (seL4_Word) 0);
    }
    if (!mapped) {
        test_error_eq(seL4_ARCH_Page_Map(page_cap, env->page_directory, (seL4_Word) page,
                                         seL4_AllRights, seL4_ARCH_Default_VMAttributes), seL4_NoError);
    }
    return sel4test_get_result();
}
DEFINE_TEST(MULTICORE0006, "Test remote calls are not run from a late IPI", smp_test_stale_remote_call,
            CONFIG_MAX_NUM_NODES > 1)

#endif /* CONFIG_ARCH_X86_64 */
