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

#include <assert.h>

#include <sel4/sel4.h>
#include <vka/object.h>
#include <vka/capops.h>

#include "../test.h"
#include "../helpers.h"

/* Arbitrarily start mapping 128mb into the virtual address range */
#define EPT_MAP_BASE 0x8000000

#define OFFSET_4MB (BIT(22))
#define OFFSET_2MB (OFFSET_4MB >> 1)

/* None of these tests actually check that mappings succeeded. This would require
 * constructing a vcpu, creating a thread and having some code compiled to run
 * inside a vt-x instance. I consider this too much work, and am largely checking
 * that none of these operations will cause the kernel to explode */

#ifdef CONFIG_VTX

static int
map_ept_from_pdpt(env_t env, seL4_CPtr pml4, seL4_CPtr pdpt, seL4_CPtr *pd, seL4_CPtr *pt, seL4_CPtr *frame)
{
    int error;

    *pd = vka_alloc_ept_page_directory_leaky(&env->vka);
    test_assert(*pd);
    *pt = vka_alloc_ept_page_table_leaky(&env->vka);
    test_assert(*pt);
    *frame = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    test_assert(*frame);

    error = seL4_X86_EPTPD_Map(*pd, pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);
    error = seL4_X86_EPTPT_Map(*pt, pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);
    error = seL4_X86_Page_MapEPT(*frame, pml4, EPT_MAP_BASE, seL4_AllRights, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    return error;
}

static int
map_ept_set(env_t env, seL4_CPtr *pml4, seL4_CPtr *pdpt, seL4_CPtr *pd, seL4_CPtr *pt, seL4_CPtr *frame)
{
    int error;

    *pml4 = vka_alloc_ept_pml4_leaky(&env->vka);
    test_assert(*pml4);
    *pdpt = vka_alloc_ept_page_directory_pointer_table_leaky(&env->vka);
    test_assert(*pdpt);

    error = seL4_X86_ASIDPool_Assign(env->asid_pool, *pml4);
    test_assert(error == seL4_NoError);

    error = seL4_X86_EPTPDPT_Map(*pdpt, *pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    error = map_ept_from_pdpt(env, *pml4, *pdpt, pd, pt, frame);

    return error;
}

static int
map_ept_set_large_from_pdpt(env_t env, seL4_CPtr pml4, seL4_CPtr pdpt, seL4_CPtr *pd, seL4_CPtr *frame)
{
    int error;

    *pd = vka_alloc_ept_page_directory_leaky(&env->vka);
    test_assert(*pd);
    *frame = vka_alloc_frame_leaky(&env->vka, seL4_LargePageBits);
    test_assert(*frame);

    error = seL4_X86_EPTPD_Map(*pd, pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    if (error != seL4_NoError) {
        return error;
    }
    error = seL4_X86_Page_MapEPT(*frame, pml4, EPT_MAP_BASE, seL4_AllRights, seL4_X86_Default_VMAttributes);
    if (error != seL4_NoError) {
        return error;
    }

    return error;
}

static int
map_ept_set_large(env_t env, seL4_CPtr *pml4, seL4_CPtr *pdpt, seL4_CPtr *pd, seL4_CPtr *frame)
{
    int error;

    *pml4 = vka_alloc_ept_pml4_leaky(&env->vka);
    test_assert(*pml4);
    *pdpt = vka_alloc_ept_page_directory_pointer_table_leaky(&env->vka);
    test_assert(*pdpt);

    error = seL4_X86_ASIDPool_Assign(env->asid_pool, *pml4);
    test_assert(error == seL4_NoError);

    error = seL4_X86_EPTPDPT_Map(*pdpt, *pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    if (error != seL4_NoError) {
        return error;
    }

    error = map_ept_set_large_from_pdpt(env, *pml4, *pdpt, pd, frame);

    return error;
}

static int
test_ept_basic_ept(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, pt, frame;
    error = map_ept_set(env, &pml4, &pdpt, &pd, &pt, &frame);
    test_assert(error == seL4_NoError);
    return sel4test_get_result();
}
DEFINE_TEST(EPT0001, "Testing basic EPT mapping", test_ept_basic_ept, true)

static int
test_ept_basic_map_unmap(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, pt, frame;
    error = map_ept_set(env, &pml4, &pdpt, &pd, &pt, &frame);
    test_assert(error == seL4_NoError);

    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);
    error = seL4_X86_EPTPT_Unmap(pt);
    test_assert(error == seL4_NoError);
    error = seL4_X86_EPTPD_Unmap(pd);
    test_assert(error == seL4_NoError);

    error = map_ept_from_pdpt(env, pml4, pdpt, &pd, &pt, &frame);
    test_assert(error == seL4_NoError);

    error = seL4_X86_EPTPD_Unmap(pd);
    test_assert(error == seL4_NoError);
    error = seL4_X86_EPTPT_Unmap(pt);
    test_assert(error == seL4_NoError);
    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(EPT0002, "Test basic EPT mapping then unmapping", test_ept_basic_map_unmap, true)

static int
test_ept_basic_map_unmap_large(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, frame;
    error = map_ept_set_large(env, &pml4, &pdpt, &pd, &frame);
    test_assert(error == seL4_NoError);

    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);
    error = seL4_X86_EPTPD_Unmap(pd);
    test_assert(error == seL4_NoError);

    error = map_ept_set_large_from_pdpt(env, pml4, pdpt, &pd, &frame);
    test_assert(error == seL4_NoError);

    error = seL4_X86_EPTPD_Unmap(pd);
    test_assert(error == seL4_NoError);
    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(EPT0003, "Test basic EPT mapping then unmapping of large frame", test_ept_basic_map_unmap_large, true)

static int
test_ept_regression_1(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, frame;
    error = map_ept_set_large(env, &pml4, &pdpt, &pd, &frame);
    test_assert(error == seL4_NoError);

    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);
    error = seL4_X86_EPTPD_Unmap(frame);
    test_assert(error != seL4_NoError);

    error = map_ept_set_large_from_pdpt(env, pml4, pdpt, &pd, &frame);
    test_assert(error != seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(EPT1001, "EPT Regression: Unmap large frame then invoke EPT PD unmap on frame", test_ept_regression_1, true)

static int
test_ept_regression_2(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, frame;
    error = map_ept_set_large(env, &pml4, &pdpt, &pd, &frame);
    test_assert(error == seL4_NoError);

    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);
    error = seL4_X86_EPTPD_Unmap(pd);
    test_assert(error == seL4_NoError);

    error = map_ept_set_large_from_pdpt(env, pml4, pdpt, &pd, &frame);
    test_assert(error == seL4_NoError);

    error = seL4_X86_EPTPD_Unmap(frame);
    test_assert(error != seL4_NoError);
    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(EPT1002, "EPT Regression: Invoke EPT PD Unmap on large frame", test_ept_regression_2, true)

static int
test_ept_no_overlapping_4k(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, pt, frame;
    error = map_ept_set(env, &pml4, &pdpt, &pd, &pt, &frame);
    test_assert(error == seL4_NoError);

    frame = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    test_assert(frame);
    error = seL4_X86_Page_MapEPT(frame, pml4, EPT_MAP_BASE, seL4_AllRights, seL4_X86_Default_VMAttributes);
    test_assert(error != seL4_NoError);
    return sel4test_get_result();
}
DEFINE_TEST(EPT0004, "Test EPT cannot map overlapping 4k pages", test_ept_no_overlapping_4k, true)

static int
test_ept_no_overlapping_large(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, frame;
    error = map_ept_set_large(env, &pml4, &pdpt, &pd, &frame);
    test_assert(error == seL4_NoError);

    frame = vka_alloc_frame_leaky(&env->vka, seL4_LargePageBits);
    test_assert(frame);
    error = seL4_X86_Page_MapEPT(frame, pml4, EPT_MAP_BASE, seL4_AllRights, seL4_X86_Default_VMAttributes);
    test_assert(error != seL4_NoError);
    return sel4test_get_result();
}
DEFINE_TEST(EPT0005, "Test EPT cannot map overlapping large pages", test_ept_no_overlapping_large, true)

#ifdef CONFIG_ARCH_IA32
static int
test_ept_aligned_4m(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, frame, frame2;
    error = map_ept_set_large(env, &pml4, &pdpt, &pd, &frame);
    test_assert(error == seL4_NoError);

    frame2 = vka_alloc_frame_leaky(&env->vka, PAGE_BITS_4M);
    test_assert(frame2);
    /* Try and map a page at +2m */
    error = seL4_X86_Page_MapEPT(frame2, pml4, EPT_MAP_BASE + OFFSET_2MB, seL4_AllRights, seL4_X86_Default_VMAttributes);
    test_assert(error != seL4_NoError);
    /* But we should be able to map it at +4m */
    error = seL4_X86_Page_MapEPT(frame2, pml4, EPT_MAP_BASE + OFFSET_4MB, seL4_AllRights, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    /* Unmap them both */
    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);
    error = seL4_X86_Page_Unmap(frame2);
    test_assert(error == seL4_NoError);

    /* Now map the first one at +2m, which should just flat out fail */
    error = seL4_X86_Page_MapEPT(frame, pml4, EPT_MAP_BASE + OFFSET_2MB, seL4_AllRights, seL4_X86_Default_VMAttributes);
    test_assert(error != seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(EPT0006, "Test EPT 4M mappings must be 4M aligned and cannot overlap", test_ept_aligned_4m, true)

static int
test_ept_no_overlapping_pt_4m(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, pt, frame;
    error = map_ept_set_large(env, &pml4, &pdpt, &pd, &frame);
    test_assert(error == seL4_NoError);

    pt = vka_alloc_ept_page_table_leaky(&env->vka);
    test_assert(pt);
    /* now try and map a PT at both 2m entries */
    error = seL4_X86_EPTPT_Map(pt, pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    test_assert(error != seL4_NoError);
    error = seL4_X86_EPTPT_Map(pt, pml4, EPT_MAP_BASE + OFFSET_2MB, seL4_X86_Default_VMAttributes);
    test_assert(error != seL4_NoError);

    /* unmap PT and frame */
    error = seL4_X86_EPTPT_Unmap(pt);
    test_assert(error == seL4_NoError);
    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);

    /* put the page table in this time */
    error = seL4_X86_EPTPT_Map(pt, pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);
    /* now try the frame */
    error = seL4_X86_Page_MapEPT(frame, pml4, EPT_MAP_BASE, seL4_AllRights, seL4_X86_Default_VMAttributes);
    test_assert(error != seL4_NoError);

    /* unmap the PT */
    error = seL4_X86_EPTPT_Unmap(pt);
    test_assert(error == seL4_NoError);

    /* put the PT and the +2m location, then try the frame */
    error = seL4_X86_EPTPT_Map(pt, pml4, EPT_MAP_BASE + OFFSET_2MB, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);
    error = seL4_X86_Page_MapEPT(frame, pml4, EPT_MAP_BASE, seL4_AllRights, seL4_X86_Default_VMAttributes);
    test_assert(error != seL4_NoError);

    return sel4test_get_result();
}
DEFINE_TEST(EPT0007, "Test EPT 4m frame and PT cannot overlap", test_ept_no_overlapping_pt_4m, true)
#endif /* CONFIG_ARCH_IA32 */

static int
test_ept_map_remap_pd(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, pt, frame;
    error = map_ept_set(env, &pml4, &pdpt, &pd, &pt, &frame);
    test_assert(error == seL4_NoError);

    /* unmap the pd */
    error = seL4_X86_EPTPD_Unmap(pd);
    test_assert(error == seL4_NoError);

    /* now map it back in */
    error = seL4_X86_EPTPD_Map(pd, pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    /* should be able to map in a new PT */
    pt = vka_alloc_ept_page_table_leaky(&env->vka);
    test_assert(pt);
    error = seL4_X86_EPTPT_Map(pt, pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);
    return sel4test_get_result();
}
DEFINE_TEST(EPT0008, "Test EPT map and remap PD", test_ept_map_remap_pd, true)

static int
test_ept_no_overlapping_pt(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, pt, frame;
    error = map_ept_set(env, &pml4, &pdpt, &pd, &pt, &frame);
    test_assert(error == seL4_NoError);

    /* Mapping in a new PT should fail */
    pt = vka_alloc_ept_page_table_leaky(&env->vka);
    test_assert(pt);
    error = seL4_X86_EPTPT_Map(pt, pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    test_assert(error != seL4_NoError);
    return sel4test_get_result();

}
DEFINE_TEST(EPT0009, "Test EPT no overlapping PT", test_ept_no_overlapping_pt, true)

static int
test_ept_no_overlapping_pd(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, pt, frame;
    error = map_ept_set(env, &pml4, &pdpt, &pd, &pt, &frame);
    test_assert(error == seL4_NoError);

    /* Mapping in a new PT should fail */
    pd = vka_alloc_ept_page_directory_leaky(&env->vka);
    test_assert(pd);
    error = seL4_X86_EPTPD_Map(pd, pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    test_assert(error != seL4_NoError);
    return sel4test_get_result();

}
DEFINE_TEST(EPT0010, "Test EPT no overlapping PD", test_ept_no_overlapping_pd, true)

static int
test_ept_map_remap_pt(env_t env)
{
    int error;
    seL4_CPtr pml4, pdpt, pd, pt, frame;
    error = map_ept_set(env, &pml4, &pdpt, &pd, &pt, &frame);
    test_assert(error == seL4_NoError);

    /* unmap the pt */
    error = seL4_X86_EPTPT_Unmap(pt);
    test_assert(error == seL4_NoError);

    /* now map it back in */
    error = seL4_X86_EPTPT_Map(pt, pml4, EPT_MAP_BASE, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);

    /* should be able to map in a frame now */
    frame = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    test_assert(frame);
    error = seL4_X86_Page_MapEPT(frame, pml4, EPT_MAP_BASE, seL4_AllRights, seL4_X86_Default_VMAttributes);
    test_assert(error == seL4_NoError);
    return sel4test_get_result();
}
DEFINE_TEST(EPT0011, "Test EPT map and remap PT", test_ept_map_remap_pt, true)

#endif /* CONFIG_VTX */
