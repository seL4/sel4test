/*
 * Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
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

#ifdef CONFIG_ARM_SMMU

sel4utils_process_t smmu_process;
static void create_smmu_vspace(driver_env_t env)
{
    int error;

    sel4utils_process_config_t config = process_config_default_simple(&env->simple, TESTS_APP, env->init->priority);
    config = process_config_mcp(config, seL4_MaxPrio);
    config = process_config_auth(config, simple_get_tcb(&env->simple));
    config = process_config_create_cnode(config, TEST_PROCESS_CSPACE_SIZE_BITS);
    error = sel4utils_configure_process_custom(&smmu_process, &env->vka, &env->vspace, config);
    ZF_LOGF_IF(error, "Failed to creat a fake process for SMMU tests");

}

static int test_smmu_control_caps(driver_env_t env)
{

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
    error = seL4_ARM_SIDControl_GetSID(simple_get_sid_ctrl(&env->simple), 0, slot_path.root, slot_path.capPtr,
                                       slot_path.capDepth);
    ZF_LOGF_IF(error, "Failed to allocate SID cap");
    sid_cap = slot_path.capPtr;

    /*testing the creating a SID with a magic number*/
    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot for SID in a magic number");
    error = seL4_ARM_SIDControl_GetSID(simple_get_sid_ctrl(&env->simple), 1780, slot_path.root, slot_path.capPtr,
                                       slot_path.capDepth);
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
    error = seL4_ARM_CBControl_GetCB(simple_get_cb_ctrl(&env->simple), 0, slot_path.root, slot_path.capPtr,
                                     slot_path.capDepth);
    ZF_LOGF_IF(error, "Failed to allocate CB cap 0");
    cb_cap = slot_path.capPtr;

    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot for CB63");
    error = seL4_ARM_CBControl_GetCB(simple_get_cb_ctrl(&env->simple), 63, slot_path.root, slot_path.capPtr,
                                     slot_path.capDepth);
    ZF_LOGF_IF(error, "Failed to allocate CB cap 63");
    cb_cap_2 = slot_path.capPtr;

    /*testing allocating a context bank cap with a magic number*/
    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot for CB 6309");
    error = seL4_ARM_CBControl_GetCB(simple_get_cb_ctrl(&env->simple), 6309, slot_path.root, slot_path.capPtr,
                                     slot_path.capDepth);
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
    printf("cb fault return %d status 0x%lx address 0x%lx for context bank 0\n", smmu_cb_fault.error,
           smmu_cb_fault.status, smmu_cb_fault.address);
    error = seL4_ARM_CB_CBClearFault(cb_cap);
    ZF_LOGF_IF(error, "Failed to clear fault status of the context bank 0");

    smmu_cb_fault = seL4_ARM_CB_CBGetFault(cb_cap_2);
    ZF_LOGF_IF(smmu_cb_fault.error, "Failed to read the fault status of the context bank 63");
    printf("cb fault return %d status 0x%lx address 0x%lx for context bank 63\n", smmu_cb_fault.error,
           smmu_cb_fault.status, smmu_cb_fault.address);
    error = seL4_ARM_CB_CBClearFault(cb_cap);
    ZF_LOGF_IF(error, "Failed to clear fault status of the context bank 63");

    /*testing if the unmapping can trigger TLB invalidation in the context banks*/
    page_ptr = vspace_new_pages(&smmu_process.vspace, seL4_AllRights, 1, PAGE_BITS_4K);
    printf("mapped page_ptr 0x%p\n", page_ptr);
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
    printf("global fault return %d status 0x%lx syndrome_0 0x%lx syndrome_1 0x%lx\n", smmu_global_fault.error,
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

    error = seL4_ARM_CBControl_GetCB(simple_get_cb_ctrl(&env->simple), 0, slot_path.root, slot_path.capPtr,
                                     slot_path.capDepth);
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

    error = seL4_ARM_SIDControl_GetSID(simple_get_sid_ctrl(&env->simple), 0, slot_path.root, slot_path.capPtr,
                                       slot_path.capDepth);
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
    error = seL4_ARM_SIDControl_GetSID(simple_get_sid_ctrl(&env->simple), 1, slot_path.root, slot_path.capPtr,
                                       slot_path.capDepth);
    ZF_LOGF_IF(!error, "the deleted SID control cap is still vaild for creating new SID caps");

    error = vka_cspace_alloc_path(&env->vka, &slot_path);
    ZF_LOGF_IF(error, "Failed to allocate cnode slot");
    error = seL4_ARM_CBControl_GetCB(simple_get_cb_ctrl(&env->simple), 1, slot_path.root, slot_path.capPtr,
                                     slot_path.capDepth);
    ZF_LOGF_IF(!error, "the deleted CB control cap is still vaild for creating new CB caps");

    return sel4test_get_result();
}


DEFINE_TEST_BOOTSTRAP(SMMU0001, "Test SMMU control caps", test_smmu_control_caps, true);

#endif /* CONFIG_ARM_SMMU */
