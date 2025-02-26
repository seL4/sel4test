/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <sel4test-driver/gen_config.h>

/* This file contains tests related to multicore. */

#include <stdio.h>
#include <sel4/sel4.h>

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
