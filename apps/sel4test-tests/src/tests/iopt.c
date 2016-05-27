/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <assert.h>

#include <sel4/sel4.h>
#include <vka/object.h>
#include <vka/capops.h>

#include "../helpers.h"

/* Arbitrarily start mapping 256mb into the virtual address range */
#define IOPT_MAP_BASE 0x10000000

/* None of these tests actually check that mappings succeed sensibly. This would
 * require having a device that does DMA and making it perform operations.
 * I consider this too much work, and am largely checking that none of these
 * operations will cause the kernel to explode */

#define MAX_IOPT_DEPTH 8
typedef struct iopt_cptrs {
    int depth;
    seL4_CPtr pts[MAX_IOPT_DEPTH];
} iopt_cptrs_t;

#define FAKE_PCI_DEVICE 0x216u
#define DOMAIN_ID       0xf

#ifdef CONFIG_IOMMU

static int
map_iopt_from_iospace(env_t env, seL4_CPtr iospace, iopt_cptrs_t *pts, seL4_CPtr *frame)
{
    int error;

    pts->depth = 0;
    /* Allocate and map page tables until we can map a frame */
    *frame = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    test_assert_fatal(*frame);

    while (seL4_X86_Page_MapIO(*frame, iospace, seL4_AllRights, IOPT_MAP_BASE) == seL4_FailedLookup) {
        test_assert_fatal(pts->depth < MAX_IOPT_DEPTH);
        pts->pts[pts->depth] = vka_alloc_io_page_table_leaky(&env->vka);
        test_assert_fatal(pts->pts[pts->depth]);
        error = seL4_X86_IOPageTable_Map(pts->pts[pts->depth], iospace, IOPT_MAP_BASE);
        test_assert(error == seL4_NoError);
        pts->depth++;
    }
    test_assert(error == seL4_NoError);

    return sel4test_get_result();
}

static int
map_iopt_set(env_t env, seL4_CPtr *iospace, iopt_cptrs_t *pts, seL4_CPtr *frame)
{
    int error;
    cspacepath_t master_path, iospace_path;

    /* Allocate a random device ID that hopefully doesn't exist have any
     * RMRR regions */
    error = vka_cspace_alloc(&env->vka, iospace);
    test_assert(!error);
    vka_cspace_make_path(&env->vka, *iospace, &iospace_path);
    vka_cspace_make_path(&env->vka, env->io_space, &master_path);
    error = vka_cnode_mint(&iospace_path, &master_path, seL4_AllRights, (seL4_CapData_t) {
        .words = { (DOMAIN_ID << 16) | FAKE_PCI_DEVICE }
    });
    test_assert(error == seL4_NoError);

    error = map_iopt_from_iospace(env, *iospace, pts, frame);

    return error;
}

static void
delete_iospace(env_t env, seL4_CPtr iospace)
{
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, iospace, &path);
    vka_cnode_delete(&path);
}

static int
test_iopt_basic_iopt(env_t env)
{
    int error;
    seL4_CPtr iospace, frame;
    iopt_cptrs_t pts;
    error = map_iopt_set(env, &iospace, &pts, &frame);
    test_assert(error == seL4_NoError);

    delete_iospace(env, iospace);
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0001, "Testing basic IOPT mapping", test_iopt_basic_iopt)

static int
test_iopt_basic_map_unmap(env_t env)
{
    int error;
    int i;
    iopt_cptrs_t pts;
    seL4_CPtr iospace, frame;
    error = map_iopt_set(env, &iospace, &pts, &frame);
    test_assert(error == seL4_NoError);

    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);
    for (i = pts.depth - 1; i >= 0; i--) {
        error = seL4_X86_IOPageTable_Unmap(pts.pts[i]);
        test_assert(error == seL4_NoError);
    }

    error = map_iopt_from_iospace(env, iospace, &pts, &frame);
    test_assert(error == seL4_NoError);

    for (i = 0; i < pts.depth; i++) {
        error = seL4_X86_IOPageTable_Unmap(pts.pts[i]);
        test_assert(error == seL4_NoError);
    }

    error = seL4_X86_Page_Unmap(frame);
    test_assert(error == seL4_NoError);

    delete_iospace(env, iospace);
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0002, "Test basic IOPT mapping then unmapping", test_iopt_basic_map_unmap)

static int
test_iopt_no_overlapping_4k(env_t env)
{
    int error;
    iopt_cptrs_t pts;
    seL4_CPtr iospace, frame;
    error = map_iopt_set(env, &iospace, &pts, &frame);
    test_assert(error == seL4_NoError);

    frame = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    test_assert_fatal(frame);
    error = seL4_X86_Page_MapIO(frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
    test_assert(error != seL4_NoError);

    delete_iospace(env, iospace);
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0004, "Test IOPT cannot map overlapping 4k pages", test_iopt_no_overlapping_4k)

static int
test_iopt_map_remap_top_pt(env_t env)
{
    int error;
    iopt_cptrs_t pts;
    seL4_CPtr iospace, frame, pt;
    error = map_iopt_set(env, &iospace, &pts, &frame);
    test_assert(error == seL4_NoError);

    /* unmap the top PT */
    error = seL4_X86_IOPageTable_Unmap(pts.pts[0]);
    test_assert(error == seL4_NoError);

    /* now map it back in */
    error = seL4_X86_IOPageTable_Map(pts.pts[0], iospace, IOPT_MAP_BASE);
    test_assert(error == seL4_NoError);

    /* it should retain its old mappings, and mapping in a new PT should fail */
    pt = vka_alloc_io_page_table_leaky(&env->vka);
    test_assert_fatal(pt);
    error = seL4_X86_IOPageTable_Map(pt, iospace, IOPT_MAP_BASE);
    test_assert(error != seL4_NoError);

    delete_iospace(env, iospace);
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0008, "Test IOPT map and remap top PT", test_iopt_map_remap_top_pt)

static int
test_iopt_no_overlapping_pt(env_t env)
{
    int error;
    iopt_cptrs_t pts;
    seL4_CPtr iospace, frame, pt;
    error = map_iopt_set(env, &iospace, &pts, &frame);
    test_assert(error == seL4_NoError);

    /* Mapping in a new PT should fail */
    pt = vka_alloc_io_page_table_leaky(&env->vka);
    test_assert_fatal(pt);
    error = seL4_X86_IOPageTable_Map(pt, iospace, IOPT_MAP_BASE);
    test_assert(error != seL4_NoError);

    delete_iospace(env, iospace);
    return sel4test_get_result();
}
DEFINE_TEST(iopt0009, "Test iopt no overlapping PT", test_iopt_no_overlapping_pt)

static int
test_iopt_map_remap_pt(env_t env)
{
    int error;
    iopt_cptrs_t pts;
    seL4_CPtr iospace, frame;
    error = map_iopt_set(env, &iospace, &pts, &frame);
    test_assert(error == seL4_NoError);

    /* unmap the pt */
    error = seL4_X86_IOPageTable_Unmap(pts.pts[pts.depth - 1]);
    test_assert(error == seL4_NoError);

    /* now map it back in */
    error = seL4_X86_IOPageTable_Map(pts.pts[pts.depth - 1], iospace, IOPT_MAP_BASE);
    test_assert(error == seL4_NoError);

    /* it should retain its old mappings, and mapping in a new frame should fail */
    frame = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    test_assert_fatal(frame);
    error = seL4_X86_Page_MapIO(frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
    test_assert(error != seL4_NoError);

    delete_iospace(env, iospace);
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0011, "Test IOPT map and remap PT", test_iopt_map_remap_pt)

static int
test_iopt_recycle_bottom_pt(env_t env)
{
    int error;
    iopt_cptrs_t pts;
    seL4_CPtr iospace, frame;
    cspacepath_t path;
    error = map_iopt_set(env, &iospace, &pts, &frame);
    test_assert(error == seL4_NoError);

    /* recycle the pt */
    vka_cspace_make_path(&env->vka, pts.pts[pts.depth - 1], &path);
    error = vka_cnode_recycle(&path);
    test_assert(error == seL4_NoError);

    /* now map a new PT and the same frame */
    pts.pts[pts.depth - 1] = vka_alloc_io_page_table_leaky(&env->vka);
    test_assert_fatal(pts.pts[pts.depth - 1]);
    error = seL4_X86_IOPageTable_Map(pts.pts[pts.depth - 1], iospace, IOPT_MAP_BASE);
    test_assert(error == seL4_NoError);
    error = seL4_X86_Page_MapIO(frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
    test_assert(error == seL4_NoError);

    delete_iospace(env, iospace);
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0012, "Test IOPT recycle bottom PT", test_iopt_recycle_bottom_pt)

static int
test_iopt_recycle_top_pt(env_t env)
{
    int error;
    iopt_cptrs_t pts;
    seL4_CPtr iospace, frame;
    cspacepath_t path;
    error = map_iopt_set(env, &iospace, &pts, &frame);
    test_assert(error == seL4_NoError);

    /* recycle the top level PT */
    vka_cspace_make_path(&env->vka, pts.pts[0], &path);
    error = vka_cnode_recycle(&path);
    test_assert(error == seL4_NoError);

    /* now map a new top level PT */
    pts.pts[0] = vka_alloc_io_page_table_leaky(&env->vka);
    test_assert_fatal(pts.pts[0]);
    error = seL4_X86_IOPageTable_Map(pts.pts[0], iospace, IOPT_MAP_BASE);
    test_assert(error == seL4_NoError);

    /* map the old next level pt back in */
    error = seL4_X86_IOPageTable_Map(pts.pts[1], iospace, IOPT_MAP_BASE);
    test_assert(error == seL4_NoError);

    /* the old frame should still be mapped into the pt, so mapping a new one should fail */
    frame = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    test_assert_fatal(frame);
    error = seL4_X86_Page_MapIO(frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
    test_assert(error != seL4_NoError);

    delete_iospace(env, iospace);
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0013, "Test iopt recycle top PT", test_iopt_recycle_top_pt)

static int
test_iopt_recycle_iospace(env_t env)
{
    int error;
    iopt_cptrs_t pts;
    seL4_CPtr iospace, frame, pt;
    cspacepath_t path;
    error = map_iopt_set(env, &iospace, &pts, &frame);
    test_assert(error == seL4_NoError);

    /* recycle the iospace */
    vka_cspace_make_path(&env->vka, iospace, &path);
    error = vka_cnode_recycle(&path);
    test_assert(error == seL4_NoError);

    /* now map the old first level PT back in */
    error = seL4_X86_IOPageTable_Map(pts.pts[0], iospace, IOPT_MAP_BASE);
    test_assert(error == seL4_NoError);

    /* the rest of the old pts should still be mapped in */
    pt = vka_alloc_io_page_table_leaky(&env->vka);
    test_assert_fatal(pt);
    error = seL4_X86_IOPageTable_Map(pt, iospace, IOPT_MAP_BASE);
    test_assert(error != seL4_NoError);

    delete_iospace(env, iospace);
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0014, "Test IOPT recycle iospace", test_iopt_recycle_iospace)

#endif /* CONFIG_IOMMU */


#ifdef CONFIG_ARM_SMMU
/* tests for ARM SystemMMU */

#define IOPT_MAP_BASE   0x10000000

static int
map_iopt_from_iospace(env_t env, seL4_CPtr iospace, seL4_CPtr *iopt, seL4_CPtr *frame)
{
    int error;
    *frame = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    test_assert_fatal(*frame);
    error = seL4_ARM_Page_MapIO(*frame, iospace, seL4_AllRights, IOPT_MAP_BASE);

    if (error == seL4_FailedLookup) {
        *iopt = vka_alloc_io_page_table_leaky(&env->vka);
        test_assert_fatal(*iopt);
        error = seL4_ARM_IOPageTable_Map(*iopt, iospace, IOPT_MAP_BASE);
        test_assert(error == seL4_NoError);
        error = seL4_ARM_Page_MapIO(*frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
        test_assert(error == seL4_NoError);
    }
    test_assert(error == seL4_NoError);

    return error;

}

static int
map_iopt_set(env_t env, seL4_CPtr iospace, seL4_CPtr *iopt_cptr, seL4_CPtr *frame)
{
    int error = map_iopt_from_iospace(env, iospace, iopt_cptr, frame);
    return error;

}

static void
delete_iospace(env_t env, seL4_CPtr iospace)
{
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, iospace, &path);
    vka_cnode_delete(&path);
}

static int
test_iopt_basic_iopt(env_t env)
{
    int error;
    seL4_CPtr pt = 0;
    seL4_CPtr frame = 0;
    seL4_SlotRegion caps = env->io_space_caps;
    int cap_count = caps.end - caps.start + 1;
    seL4_CPtr cap = caps.start;

    for (int i = 0; i < cap_count; i++) {
        error = map_iopt_set(env, cap + i, &pt, &frame);
        test_assert(error == seL4_NoError);
        delete_iospace(env, cap + i);
    }
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0001, "Testing basic ARM IOPT mapping", test_iopt_basic_iopt);


static int
test_iopt_basic_map_unmap(env_t env)
{
    int error;
    int i;
    seL4_CPtr iospace, pt, frame;
    seL4_SlotRegion caps = env->io_space_caps;
    int cap_count = caps.end - caps.start + 1;

    for (i = 0; i < cap_count; i++) {
        iospace = caps.start + i;
        error = map_iopt_set(env, iospace, &pt, &frame);
        test_assert(error == seL4_NoError);

        error = seL4_ARM_Page_Unmap(frame);
        test_assert(error == seL4_NoError);

        error = seL4_ARM_IOPageTable_Unmap(pt);
        test_assert(error == seL4_NoError);

        error = map_iopt_from_iospace(env, iospace, &pt, &frame);
        test_assert(error == seL4_NoError);

        error = seL4_ARM_IOPageTable_Unmap(pt);
        test_assert(error == seL4_NoError);
        error = seL4_ARM_Page_Unmap(frame);
        test_assert(error == seL4_NoError);
        delete_iospace(env, iospace);
    }
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0002, "Test basic ARM IOPT mapping then unmapping", test_iopt_basic_map_unmap)

static int
test_iopt_no_overlapping_4k(env_t env)
{
    int error;
    int i;
    seL4_CPtr iospace, pt, frame;
    seL4_SlotRegion caps = env->io_space_caps;
    int cap_count = caps.end - caps.start + 1;
    for (i = 0; i < cap_count; i++) {
        iospace = caps.start + i;
        error = map_iopt_set(env, iospace, &pt, &frame);
        test_assert(error == seL4_NoError);

        frame = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
        test_assert_fatal(frame);
        /* mapping in a new frame should fail */
        error = seL4_ARM_Page_MapIO(frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
        test_assert(error != seL4_NoError);
        delete_iospace(env, iospace);
    }

    return sel4test_get_result();
}
DEFINE_TEST(IOPT0004, "Test ARM IOPT cannot map overlapping 4k pages", test_iopt_no_overlapping_4k)


static int
test_iopt_map_remap_pt(env_t env)
{
    int error;
    int i;
    seL4_CPtr iospace, pt, frame;
    seL4_SlotRegion caps = env->io_space_caps;
    int cap_count = caps.end - caps.start + 1;
    for (i = 0; i < cap_count; i++) {
        iospace = caps.start + i;
        error = map_iopt_set(env, iospace, &pt, &frame);
        test_assert(error == seL4_NoError);

        /* unmap the PT */
        error = seL4_ARM_IOPageTable_Unmap(pt);
        test_assert(error == seL4_NoError);

        /* now map it back in */
        error = seL4_ARM_IOPageTable_Map(pt, iospace, IOPT_MAP_BASE);
        test_assert(error == seL4_NoError);

        /* it should retain its old mappings, and mapping in a new PT should fail */
        pt = vka_alloc_io_page_table_leaky(&env->vka);
        test_assert_fatal(pt);
        error = seL4_ARM_IOPageTable_Map(pt, iospace, IOPT_MAP_BASE);
        test_assert(error != seL4_NoError);
        delete_iospace(env, iospace);
    }

    return sel4test_get_result();
}
DEFINE_TEST(IOPT0008, "Test ARM IOPT map and remap PT", test_iopt_map_remap_pt)

static int
test_iopt_no_overlapping_pt(env_t env)
{
    int error;
    int i;
    seL4_CPtr iospace, pt, frame;
    seL4_SlotRegion caps = env->io_space_caps;
    int cap_count = caps.end - caps.start + 1;
    for (i = 0; i < cap_count; i++) {
        iospace = caps.start + i;
        error = map_iopt_set(env, iospace, &pt, &frame);
        test_assert(error == seL4_NoError);

        /* Mapping in a new PT should fail */
        pt = vka_alloc_io_page_table_leaky(&env->vka);
        test_assert_fatal(pt);
        error = seL4_ARM_IOPageTable_Map(pt, iospace, IOPT_MAP_BASE);
        test_assert(error != seL4_NoError);
        delete_iospace(env, iospace);
    }
    return sel4test_get_result();
}
DEFINE_TEST(IOPT0009, "Test ARM iopt no overlapping PT", test_iopt_no_overlapping_pt)


static int
test_iopt_recycle_pt(env_t env)
{
    int error;
    int i;
    seL4_CPtr iospace, pt, frame;
    cspacepath_t path;
    seL4_SlotRegion caps = env->io_space_caps;
    int cap_count = caps.end - caps.start + 1;
    for (i = 0; i < cap_count; i++) {
        iospace = caps.start + i;

        error = map_iopt_set(env, iospace, &pt, &frame);
        test_assert(error == seL4_NoError);
        assert(pt != 0);

        /* recycle PT */
        vka_cspace_make_path(&env->vka, pt, &path);
        error = vka_cnode_recycle(&path);
        test_assert(error == seL4_NoError);

        pt = vka_alloc_io_page_table_leaky(&env->vka);
        test_assert_fatal(pt);

        /* mapping a new PT should be fine */
        error = seL4_ARM_IOPageTable_Map(pt, iospace, IOPT_MAP_BASE);
        test_assert(error == seL4_NoError);

        /* mapping the old frame is not OK */
        error = seL4_ARM_Page_MapIO(frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
        test_assert(error != seL4_NoError);

        /* recycle the frame */
        vka_cspace_make_path(&env->vka, frame, &path);
        error = vka_cnode_recycle(&path);
        test_assert(error == seL4_NoError);

        /* now should be fine */
        error = seL4_ARM_Page_MapIO(frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
        test_assert(error == seL4_NoError);

        /* unmap and remap it, should pass */
        error = seL4_ARM_Page_Unmap(frame);
        test_assert(error == seL4_NoError);

        error = seL4_ARM_Page_MapIO(frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
        test_assert(error == seL4_NoError);

        delete_iospace(env, iospace);
    }
    return sel4test_get_result();
}
DEFINE_TEST(IOPT00012, "Test ARM IOPT recycle PT", test_iopt_recycle_pt)

static int
test_iopt_recycle_iospaces(env_t env)
{
    int error;
    int i;
    seL4_CPtr iospace, pt, frame;
    cspacepath_t path;
    seL4_SlotRegion caps = env->io_space_caps;
    int cap_count = caps.end - caps.start + 1;

    for (i = 0; i < cap_count; i++) {
        iospace = caps.start + i;
        error = map_iopt_set(env, iospace, &pt, &frame);
        test_assert(error == seL4_NoError);
        assert(pt != 0);

        /* recycle IO space, page table are not reset */
        vka_cspace_make_path(&env->vka, iospace, &path);
        error = vka_cnode_recycle(&path);
        test_assert(error == seL4_NoError);

        /* mapping old pt should fail without recycling it first */
        error = seL4_ARM_IOPageTable_Map(pt, iospace, IOPT_MAP_BASE);
        test_assert(error != seL4_NoError);

        /* recycle the old pt */
        vka_cspace_make_path(&env->vka, pt, &path);
        error = vka_cnode_recycle(&path);
        test_assert(error == seL4_NoError);

        /* now map in should be fine */
        error = seL4_ARM_IOPageTable_Map(pt, iospace, IOPT_MAP_BASE);
        test_assert(error == seL4_NoError);

        /* mapping old frame should fail since it contains a valid ASID */
        error = seL4_ARM_Page_MapIO(frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
        test_assert(error != seL4_NoError);

        /* recyle the frame */
        vka_cspace_make_path(&env->vka, frame, &path);
        error = vka_cnode_recycle(&path);
        test_assert(error == seL4_NoError);

        /* now mapping frame should be ok */
        error = seL4_ARM_Page_MapIO(frame, iospace, seL4_AllRights, IOPT_MAP_BASE);
        test_assert(error == seL4_NoError);

        delete_iospace(env, iospace);
    }
    return sel4test_get_result();
}
DEFINE_TEST(IOPT00013, "Test ARM IOPT recycle iospaces", test_iopt_recycle_iospaces)

#endif
