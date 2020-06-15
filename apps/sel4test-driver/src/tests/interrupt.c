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

#include <autoconf.h>
#include <sel4test-driver/gen_config.h>
#include <sel4/sel4.h>
#include <sel4/benchmark_track_types.h>
#include <sel4utils/benchmark_track.h>
#include <vka/object.h>
#include <vka/capops.h>
#include <platsupport/local_time_manager.h>

#include "../timer.h"

#include <utils/util.h>

#if CONFIG_MAX_NUM_NODES > 1 && CONFIG_ARCH_ARM && CONFIG_TRACK_KERNEL_ENTRIES

static int is_timer_interrupt(driver_env_t env, int irq)
{
    int i;

    for (i = 0; i < env->timer.to.nirqs; i++) {
        if (irq == env->timer.to.irqs[i].irq.irq.number) {
            return 1;
        }
    }
    return 0;
}

static int override_timer_irqs(driver_env_t env, int targetCore)
{
    sel4ps_irq_t irq;
    cspacepath_t path;
    seL4_Error err;
    int i;

    for (i = 0; i < env->timer.to.nirqs; i++) {
        irq = env->timer.to.irqs[i];
        path = irq.handler_path;
        /* Clear the current IRQ handler */
        err = vka_cnode_delete(&path);
        test_assert(err == seL4_NoError);

        /* And replace it by the new, core specific handler */
        err = seL4_IRQControl_GetTriggerCore(seL4_CapIRQControl, irq.irq.irq.number,
           1, path.root, path.capPtr, path.capDepth, 1 << targetCore);
        test_assert(err == seL4_NoError);

        /* Rebind it with its notification object */
        err = seL4_IRQHandler_SetNotification(irq.handler_path.capPtr, irq.badged_ntfn_path.capPtr);
        test_assert(err == seL4_NoError);
    }
}

static int test_core(driver_env_t env, int core)
{
    int i;
    seL4_Error err;

    override_timer_irqs(env, core);
    err = ltimer_set_timeout(&env->timer.ltimer, 1 * NS_IN_MS, TIMEOUT_PERIODIC);
    test_assert_fatal(!err);

    for (i = 0; i < 10; i++) {
        wait_for_timer_interrupt(env);
    }

    err = ltimer_reset(&env->timer.ltimer);
    test_assert_fatal(!err);
    return 1;
}

static int test_core_affinity_interrupt(driver_env_t env)
{
    cspacepath_t path;
    seL4_Error err;
    seL4_CPtr logBufferCap;
    benchmark_track_kernel_entry_t *logBuffer;
    unsigned nbLogEntries, i;
    unsigned irqPerNode[CONFIG_MAX_NUM_NODES] = {0};

    /* Set up logs */
    logBuffer = vspace_new_pages(&env->vspace, seL4_AllRights, 1, seL4_SectionBits);
    logBufferCap = vspace_get_cap(&env->vspace, logBuffer);
    seL4_BenchmarkSetLogBuffer(logBufferCap);

    /* Start logging */
    seL4_BenchmarkResetLog();

    for (i = 0; i < CONFIG_MAX_NUM_NODES; i++) {
        test_core(env, i); /* Generate 10 interrupts routed to core i */
    }

    /* Stop logging */
    nbLogEntries = seL4_BenchmarkFinalizeLog();

    /* Iterate through the log to count all timer irqs delivered to each core */
    for (i = 0; i < nbLogEntries; i++) {
        if (logBuffer[i].entry.path == Entry_Interrupt) {
            uint32_t irqVal = logBuffer[i].entry.word;
            uint8_t irqCore = logBuffer[i].entry.core;

            if (is_timer_interrupt(env, irqVal)) {
                irqPerNode[irqCore]++;
            }
        }
    }

    /* Assert that exactly 10 timer irqs were delivered to each core */
    for (i = 0; i < CONFIG_MAX_NUM_NODES; i++) {
        test_assert(irqPerNode[i] == 10);
    }
    return sel4test_get_result();
}
DEFINE_TEST_BOOTSTRAP(SMPIRQ0001, "Test multicore irqs", test_core_affinity_interrupt, true);

#endif

#ifdef CONFIG_ARM_SMMU 
sel4utils_process_t smmu_process; 
static void create_smmu_vspace(driver_env_t env) {
    int error;

    sel4utils_process_config_t config = process_config_default_simple(&env->simple, TESTS_APP, env->init->priority);
    config = process_config_mcp(config, seL4_MaxPrio);
    config = process_config_auth(config, simple_get_tcb(&env->simple));
    config = process_config_create_cnode(config, TEST_PROCESS_CSPACE_SIZE_BITS);
    error = sel4utils_configure_process_custom(&smmu_process, &env->vka, &env->vspace, config);
    ZF_LOGF_IF(error, "Failed to creat a fake process for SMMU tests");

}

static int test_smmu_control_caps(driver_env_t env) {

    int error; 
    cspacepath_t slot_path, src_path, dest_path; 
    seL4_CPtr sid_cap, sid_copy_cap; 
    seL4_CPtr cb_cap, cb_copy_cap, cb_cap_2;
  //  vka_object_t cb_vspace; 
    seL4_ARM_SIDControl_GetFault_t smmu_global_fault; 
    seL4_ARM_CB_CBGetFault_t smmu_cb_fault; 
    void *page_ptr; 
    /*init a project structure for testing purpose*/
    create_smmu_vspace(env); 

    /*invalidating all TLB entries in context banks*/
    error = seL4_ARM_CBControl_TLBInvalidateAll(simple_get_cb_ctrl(&env->simple)); 
    ZF_LOGF_IF(error, "Failed to invalidate all TLB entries in SMMU context banks");

    /*get the SID cap managing the SID number 0*/
    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot for SID0");
    error = seL4_ARM_SIDControl_GetSID(simple_get_sid_ctrl(&env->simple), 0, slot_path.root, slot_path.capPtr, slot_path.capDepth); 
    ZF_LOGF_IF(error, "Failed to allocate SID cap");
    sid_cap = slot_path.capPtr; 

    /*testing the creating a SID with a magic number*/
    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot for SID in a magic number");
    error = seL4_ARM_SIDControl_GetSID(simple_get_sid_ctrl(&env->simple), 1780, slot_path.root, slot_path.capPtr, slot_path.capDepth); 
    ZF_LOGF_IF(!error, "the can allocate a SID with a magic number");

    /*testing copy SID caps*/
    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot for copying SID cap 0");
    sid_copy_cap = slot_path.capPtr; 
    vka_cspace_make_path(&env->vka, sid_cap, &src_path);
    vka_cspace_make_path(&env->vka, sid_copy_cap, &dest_path);
    error = vka_cnode_copy(&dest_path, &src_path, seL4_AllRights); 
    ZF_LOGF_IF(error, "Failed to copy SID cap 0");

    /*get the context bank cap that manages CB 0 and 63 (max CB number is 64)*/
    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot for CB0");
    error = seL4_ARM_CBControl_GetCB(simple_get_cb_ctrl(&env->simple), 0, slot_path.root, slot_path.capPtr, slot_path.capDepth); 
    ZF_LOGF_IF(error, "Failed to allocate CB cap 0");
    cb_cap = slot_path.capPtr; 

    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot for CB63");
    error = seL4_ARM_CBControl_GetCB(simple_get_cb_ctrl(&env->simple), 63, slot_path.root, slot_path.capPtr, slot_path.capDepth); 
    ZF_LOGF_IF(error, "Failed to allocate CB cap 63");
    cb_cap_2 = slot_path.capPtr; 

    /*testing allocating a context bank cap with a magic number*/
    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot for CB 6309");
    error = seL4_ARM_CBControl_GetCB(simple_get_cb_ctrl(&env->simple), 6309, slot_path.root, slot_path.capPtr, slot_path.capDepth); 
    ZF_LOGF_IF(!error, "can allocate a context bank with a magic number");
    
    /*allocate a vspace and its VSID for the context banks*/
    /*testing copy context bank caps*/
    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot for copying context bank cap 0");
    cb_copy_cap = slot_path.capPtr; 
    vka_cspace_make_path(&env->vka, cb_cap, &src_path);
    vka_cspace_make_path(&env->vka, cb_copy_cap, &dest_path);
    error = vka_cnode_copy(&dest_path, &src_path, seL4_AllRights); 
    ZF_LOGF_IF(error, "Failed to copy CB cap 0");

    /*testing assigning a Vspace to CB cap*/ 
    error = seL4_ARM_CB_AssignVspace(cb_cap, smmu_process.pd.cptr); 
    ZF_LOGF_IF(error, "Failed to assign vspace to CB 0");

    /*testing 2 context banks sharing one vspace*/
    error = seL4_ARM_CB_AssignVspace(cb_cap_2, smmu_process.pd.cptr); 
    ZF_LOGF_IF(error, "Failed to assign vspace to CB 63");

    /*testing the get fault API on context bank cap*/
    smmu_cb_fault = seL4_ARM_CB_CBGetFault(cb_cap); 
    ZF_LOGF_IF(smmu_cb_fault.error, "Failed to read the fault status of the context bank 0");
    printf("cb fault return %d status 0x%x address 0x%lx for context bank 0\n", smmu_cb_fault.error,
        smmu_cb_fault.status, smmu_cb_fault.address);
    error = seL4_ARM_CB_CBClearFault(cb_cap); 
    ZF_LOGF_IF(error, "Failed to clear fault status of the context bank 0");

    smmu_cb_fault = seL4_ARM_CB_CBGetFault(cb_cap_2); 
    ZF_LOGF_IF(smmu_cb_fault.error, "Failed to read the fault status of the context bank 63");
    printf("cb fault return %d status 0x%x address 0x%lx for context bank 63\n", smmu_cb_fault.error,
        smmu_cb_fault.status, smmu_cb_fault.address);
    error = seL4_ARM_CB_CBClearFault(cb_cap); 
    ZF_LOGF_IF(error, "Failed to clear fault status of the context bank 63");

    /*testing if the unmapping can trigger TLB invalidation in the context banks*/
    page_ptr = vspace_new_pages(&smmu_process.vspace, seL4_AllRights, 1, PAGE_BITS_4K); 
    printf("mapped page_ptr 0x%x\n", page_ptr);
    assert(page_ptr != NULL);
    vspace_unmap_pages(&smmu_process.vspace, page_ptr, 1, PAGE_BITS_4K, NULL); 
    printf("unmap page is done\n");
    /*testing if a context bank can be assigned with a vspace without unassign the previous one*/
    error = seL4_ARM_CB_AssignVspace(cb_copy_cap, smmu_process.pd.cptr); 
    ZF_LOGF_IF(!error, "context bank 0 can be bound with a second vspace without unassign");
    
    /*testing binding Context bank to SID*/
    error = seL4_ARM_SID_BindCB(sid_cap, cb_cap); 
    ZF_LOGF_IF(error, "Failed to bind CB 0 to SID 0");
    error = seL4_ARM_SID_BindCB(sid_cap, cb_copy_cap); 
    ZF_LOGF_IF(!error, "context bank 0 can be bound with SID 0 without unbind");
    error = seL4_ARM_SID_BindCB(sid_copy_cap, cb_copy_cap); 
    ZF_LOGF_IF(!error, "context bank 0 can be bound with SID 0 without unbind");
   
    /*testing the TLB invalidate API on context bank caps*/
    error = seL4_ARM_CB_TLBInvalidate(cb_cap); 
    ZF_LOGF_IF(error, "Failed to invalidate TLB entries in CB 0");
    error = seL4_ARM_CB_TLBInvalidate(cb_cap_2); 
    ZF_LOGF_IF(error, "Failed to invalidate TLB entries in CB 63");
   
    /*testing the global falut status on SMMU*/
    smmu_global_fault = seL4_ARM_SIDControl_GetFault(simple_get_sid_ctrl(&env->simple)); 
    ZF_LOGF_IF(smmu_global_fault.error, "Failed to read the global fault status of the SMMU ");
    printf("global fault return %d status 0x%x syndrome_0 0x%x syndrome_1 0x%x\n", smmu_global_fault.error,
        smmu_global_fault.status, smmu_global_fault.syndrome_0, smmu_global_fault.syndrome_1);
    error = seL4_ARM_SIDControl_ClearFault(simple_get_sid_ctrl(&env->simple)); 
    ZF_LOGF_IF(error, "Failed to clear the global fault status of the SMMU ");

    /*testing unassign vspace from context banks*/
    error = seL4_ARM_CB_UnassignVspace(cb_cap); 
    ZF_LOGF_IF(error, "Failed to unassigned VSpace used by CB 0");
    error = seL4_ARM_CB_UnassignVspace(cb_cap); 
    ZF_LOGF_IF(!error, "The context bank unassigns a NULL vspace");

    /*testing if the context bank vspace can be revoked by delete the process*/
    sel4utils_destroy_process(&smmu_process, &env->vka);
    printf("unassgin the vspace after the parent of that vspace is deleted\n");
    error = seL4_ARM_CB_UnassignVspace(cb_cap_2); 
    ZF_LOGF_IF(error, "Failed to unassigned VSpace used by CB 63");

    /*using the vspace used by root task from here*/
    /*testing if can be reassigned*/
    error = seL4_ARM_CB_AssignVspace(cb_copy_cap, simple_get_pd(&env->simple)); 
    ZF_LOGF_IF(error, "Failed to reassigned vspace to CB");

    error = seL4_ARM_SID_UnbindCB(sid_cap);
    ZF_LOGF_IF(error, "Failed to unbind CB from the SID");

    error = seL4_ARM_SID_BindCB(sid_cap, cb_copy_cap); 
    ZF_LOGF_IF(error, "Failed to rebind CB to SID");

    //testing revoking the cb cap 
    vka_cspace_make_path(&env->vka, cb_cap, &src_path);
    error = vka_cnode_delete(&src_path); 
    ZF_LOGF_IF(error, "Failed to delete CB cap");

    vka_cspace_make_path(&env->vka, cb_copy_cap, &src_path);
    error = vka_cnode_delete(&src_path); 
    ZF_LOGF_IF(error, "Failed to delete the second CB cap");

    error = seL4_ARM_SID_UnbindCB(sid_cap);
    ZF_LOGF_IF(error, "Failed to unbind CB from the SID, this should remove the copy of the vspace assigned to that cb");

    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot");

    error = seL4_ARM_CBControl_GetCB(simple_get_cb_ctrl(&env->simple), 0, slot_path.root, slot_path.capPtr, slot_path.capDepth); 
    ZF_LOGF_IF(error, "Failed to re-allocate CB cap 0");

    //testing revoking the sid cap 
    vka_cspace_make_path(&env->vka, sid_cap, &src_path);
    error = vka_cnode_delete(&src_path); 
    ZF_LOGF_IF(error, "Failed to delete SID cap");

    error = seL4_ARM_SID_UnbindCB(sid_cap);
    ZF_LOGF_IF(!error, "the deleted sid cap still accepts function calls");

    vka_cspace_make_path(&env->vka, sid_copy_cap, &src_path);
    error = vka_cnode_delete(&src_path); 
    ZF_LOGF_IF(error, "Failed to delete the second SID cap");

    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot");

    error = seL4_ARM_SIDControl_GetSID(simple_get_sid_ctrl(&env->simple), 0, slot_path.root, slot_path.capPtr, slot_path.capDepth); 
    ZF_LOGF_IF(error, "Failed to re-allocate SID cap");

    // testing deleting sid and cb control caps
    vka_cspace_make_path(&env->vka, simple_get_sid_ctrl(&env->simple), &src_path);
    error = vka_cnode_delete(&src_path); 
    ZF_LOGF_IF(error, "Failed to delete the SID control cap");

    vka_cspace_make_path(&env->vka, simple_get_cb_ctrl(&env->simple), &src_path);
    error = vka_cnode_delete(&src_path); 
    ZF_LOGF_IF(error, "Failed to delete the CB control cap");

    //testing cap allocation with invalide control cap
    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot");
    error = seL4_ARM_SIDControl_GetSID(simple_get_sid_ctrl(&env->simple), 1, slot_path.root, slot_path.capPtr, slot_path.capDepth); 
    ZF_LOGF_IF(!error, "the deleted SID control cap is still vaild for creating new SID caps");

    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot");
    error = seL4_ARM_CBControl_GetCB(simple_get_cb_ctrl(&env->simple), 1, slot_path.root, slot_path.capPtr, slot_path.capDepth); 
    ZF_LOGF_IF(!error, "the deleted CB control cap is still vaild for creating new CB caps");

    return sel4test_get_result(); 
}


DEFINE_TEST_BOOTSTRAP(SMMU0001, "Test SMMU control caps", test_smmu_control_caps, true);

#endif 