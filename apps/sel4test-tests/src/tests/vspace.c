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

#ifdef CONFIG_ARCH_X86
#include <platsupport/arch/tsc.h>
#endif

#include "../helpers.h"

static int
remote_function(void)
{
    return 42;
}

static int
test_interas_diffcspace(env_t env)
{
    helper_thread_t t;

    create_helper_process(env, &t);

    start_helper(env, &t, (helper_fn_t) remote_function, 0, 0, 0, 0);
    seL4_Word ret = wait_for_helper(&t);
    test_assert(ret == 42);
    cleanup_helper(env, &t);

    return sel4test_get_result();
}
DEFINE_TEST(VSPACE0000, "Test threads in different cspace/vspace", test_interas_diffcspace, true)

#if defined(CONFIG_ARCH_AARCH32)
static int
test_unmap_after_delete(env_t env)
{
    seL4_Word map_addr = 0x10000000;
    cspacepath_t path;
    int error;

    seL4_CPtr pd = vka_alloc_object_leaky(&env->vka, seL4_ARM_PageDirectoryObject, 0);
    seL4_CPtr pt = vka_alloc_object_leaky(&env->vka, seL4_ARM_PageTableObject, 0);
    seL4_CPtr frame = vka_alloc_object_leaky(&env->vka, seL4_ARM_SmallPageObject, 0);
    test_assert(pd != 0);
    test_assert(pt != 0);
    test_assert(frame != 0);

    seL4_ARM_ASIDPool_Assign(env->asid_pool, pd);

    /* map page table into page directory */
    error = seL4_ARM_PageTable_Map(pt, pd, map_addr, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    /* map frame into the page table */
    error = seL4_ARM_Page_Map(frame, pd, map_addr, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    /* delete the page directory */
    vka_cspace_make_path(&env->vka, pd, &path);
    seL4_CNode_Delete(path.root, path.capPtr, path.capDepth);

    /* unmap the frame */
    seL4_ARM_Page_Unmap(frame);

    return sel4test_get_result();
}
DEFINE_TEST(VSPACE0001, "Test unmapping a page after deleting the PD", test_unmap_after_delete, true)

#elif defined(CONFIG_ARCH_AARCH64)
static int
test_unmap_after_delete(env_t env)
{
    seL4_Word map_addr = 0x10000000;
    cspacepath_t path;
    int error;

    seL4_CPtr pgd = vka_alloc_object_leaky(&env->vka, seL4_ARM_PageGlobalDirectoryObject, 0);
    seL4_CPtr pud = vka_alloc_object_leaky(&env->vka, seL4_ARM_PageUpperDirectoryObject, 0);
    seL4_CPtr pd = vka_alloc_object_leaky(&env->vka, seL4_ARM_PageDirectoryObject, 0);
    seL4_CPtr pt = vka_alloc_object_leaky(&env->vka, seL4_ARM_PageTableObject, 0);
    seL4_CPtr frame = vka_alloc_object_leaky(&env->vka, seL4_ARM_SmallPageObject, 0);
    test_assert(pgd != 0);
    test_assert(pud != 0);
    test_assert(pd != 0);
    test_assert(pt != 0);
    test_assert(frame != 0);

    seL4_ARM_ASIDPool_Assign(env->asid_pool, pgd);

    /* map pud into page global directory */
    error = seL4_ARM_PageUpperDirectory_Map(pud, pgd, map_addr, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    /* map pd into page upper directory */
    error = seL4_ARM_PageDirectory_Map(pd, pgd, map_addr, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    /* map page table into page directory */
    error = seL4_ARM_PageTable_Map(pt, pgd, map_addr, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    /* map frame into the page table */
    error = seL4_ARM_Page_Map(frame, pgd, map_addr, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    /* delete the page directory */
    vka_cspace_make_path(&env->vka, pgd, &path);
    seL4_CNode_Delete(path.root, path.capPtr, path.capDepth);

    /* unmap the frame */
    seL4_ARM_Page_Unmap(frame);

    return sel4test_get_result();
}
DEFINE_TEST(VSPACE0001, "Test unmapping a page after deleting the PD", test_unmap_after_delete, true)
#endif /* CONFIG_ARCH_AARCHxx */

static int
test_asid_pool_make(env_t env)
{
    vka_t *vka = &env->vka;
    cspacepath_t path;
    seL4_CPtr pool = vka_alloc_untyped_leaky(vka, seL4_PageBits);
    test_assert(pool);
    int ret = vka_cspace_alloc_path(vka, &path);
    ret = seL4_ARCH_ASIDControl_MakePool(env->asid_ctrl, pool, env->cspace_root, path.capPtr, path.capDepth);
    test_eq(ret, seL4_NoError);
    ret = seL4_ARCH_ASIDPool_Assign(path.capPtr, env->page_directory);
    test_eq(ret, seL4_InvalidCapability);
    vka_object_t vspaceroot;
    ret = vka_alloc_vspace_root(vka, &vspaceroot);
    test_assert(!ret);
    ret = seL4_ARCH_ASIDPool_Assign(path.capPtr, vspaceroot.cptr);
    test_eq(ret, seL4_NoError);
    return sel4test_get_result();

}
DEFINE_TEST(VSPACE0002, "Test create ASID pool", test_asid_pool_make, true)

#ifdef CONFIG_ARCH_IA32
static int
test_dirty_accessed_bits(env_t env)
{
    seL4_CPtr frame;
    int err;
    seL4_X86_PageDirectory_GetStatusBits_t status;

    void *vaddr;
    reservation_t reservation;

    reservation = vspace_reserve_range(&env->vspace,
                                       PAGE_SIZE_4K, seL4_AllRights, 1, &vaddr);
    test_assert(reservation.res);

    /* Create a frame */
    frame = vka_alloc_frame_leaky(&env->vka, PAGE_BITS_4K);
    test_assert(frame != seL4_CapNull);

    /* Map it in */
    err = vspace_map_pages_at_vaddr(&env->vspace, &frame, NULL, vaddr, 1, seL4_PageBits, reservation);
    test_assert(!err);

    /* Check the status flags */
    status = seL4_X86_PageDirectory_GetStatusBits(vspace_get_root(&env->vspace), (seL4_Word)vaddr);
    test_assert(!status.error);
    test_assert(!status.accessed);
    test_assert(!status.dirty);
    /* try and prevent prefetcher */
    rdtsc_cpuid();

    /* perform a read and check status flags */
    asm volatile("" :: "r"(*(uint32_t*)vaddr) : "memory");
    status = seL4_X86_PageDirectory_GetStatusBits(vspace_get_root(&env->vspace), (seL4_Word)vaddr);
    test_assert(!status.error);
    test_assert(status.accessed);
    test_assert(!status.dirty);
    /* try and prevent prefetcher */
    rdtsc_cpuid();

    /* perform a write and check status flags */
    *(uint32_t*)vaddr = 42;
    asm volatile("" ::: "memory");
    status = seL4_X86_PageDirectory_GetStatusBits(vspace_get_root(&env->vspace), (seL4_Word)vaddr);
    test_assert(!status.error);
    test_assert(status.accessed);
    test_assert(status.dirty);

    return sel4test_get_result();
}
DEFINE_TEST(VSPACE0010, "Test dirty and accessed bits on mappings", test_dirty_accessed_bits, true)
#endif
