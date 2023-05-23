/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <sel4test-driver/gen_config.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <vka/object.h>
#include <vka/capops.h>
#include <sel4utils/util.h>
#include <sel4utils/arch/cache.h>

#include "../helpers.h"
#include "frame_type.h"

#if defined(CONFIG_ARCH_ARM)
static int test_page_flush(env_t env)
{
    seL4_CPtr frame, framec;
    uintptr_t vstart, vstartc;
    volatile uint32_t *ptr, *ptrc;
    vka_t *vka;
    int error;

    vka = &env->vka;

    void *vaddr;
    void *vaddrc;
    reservation_t reservation, reservationc;

    reservation = vspace_reserve_range(&env->vspace,
                                       PAGE_SIZE_4K, seL4_AllRights, 0, &vaddr);
    assert(reservation.res);
    reservationc = vspace_reserve_range(&env->vspace,
                                        PAGE_SIZE_4K, seL4_AllRights, 1, &vaddrc);
    assert(reservationc.res);

    vstart = (uintptr_t)vaddr;
    assert(IS_ALIGNED(vstart, seL4_PageBits));
    vstartc = (uintptr_t)vaddrc;
    assert(IS_ALIGNED(vstartc, seL4_PageBits));

    ptr = (volatile uint32_t *)vstart;
    ptrc = (volatile uint32_t *)vstartc;

    /* Create a frame */
    frame = vka_alloc_frame_leaky(vka, PAGE_BITS_4K);
    test_assert(frame != seL4_CapNull);

    /* Duplicate the cap */
    framec = get_free_slot(env);
    test_assert(framec != seL4_CapNull);
    error = cnode_copy(env, frame, framec, seL4_AllRights);
    test_error_eq(error, seL4_NoError);

    /* map in a cap with cacheability */
    error = vspace_map_pages_at_vaddr(&env->vspace, &framec, NULL, vaddrc, 1, seL4_PageBits, reservationc);
    test_error_eq(error, seL4_NoError);
    /* map in a cap without cacheability */
    error = vspace_map_pages_at_vaddr(&env->vspace, &frame, NULL, vaddr, 1, seL4_PageBits, reservation);
    test_error_eq(error, seL4_NoError);

    /* Clean makes data observable to non-cached page */
    *ptr = 0xC0FFEE;
    *ptrc = 0xDEADBEEF;
    test_assert(*ptr == 0xC0FFEE);
    test_assert(*ptrc == 0xDEADBEEF);
    error = seL4_ARM_Page_Clean_Data(framec, 0, PAGE_SIZE_4K);
    assert(!error);
    test_assert(*ptr == 0xDEADBEEF);
    test_assert(*ptrc == 0xDEADBEEF);
    /* Clean/Invalidate makes data observable to non-cached page */
    *ptr = 0xC0FFEE;
    *ptrc = 0xDEADBEEF;
    test_assert(*ptr == 0xC0FFEE);
    test_assert(*ptrc == 0xDEADBEEF);
    error = seL4_ARM_Page_CleanInvalidate_Data(framec, 0, PAGE_SIZE_4K);
    assert(!error);
    test_assert(*ptr == 0xDEADBEEF);
    test_assert(*ptrc == 0xDEADBEEF);
    /* Invalidate makes RAM data observable to cached page */
    *ptr = 0xC0FFEE;
    *ptrc = 0xDEADBEEF;
    test_assert(*ptr == 0xC0FFEE);
    test_assert(*ptrc == 0xDEADBEEF);
    error = seL4_ARM_Page_Invalidate_Data(framec, 0, PAGE_SIZE_4K);
    assert(!error);

    /* In case the invalidation performs an implicit clean, write a new
       value to RAM and make sure the cached read retrieves it
       Remember to drain any store buffer!
    */
    *ptr = 0xBEEFCAFE;
#if defined(CONFIG_ARCH_AARCH32)
    asm volatile("dmb" ::: "memory");
#elif defined(CONFIG_ARCH_AARCH64)
    asm volatile("dmb sy" ::: "memory");
#endif /* CONFIG_ARCH_AARCHxx */
    test_assert(*ptrc == 0xBEEFCAFE);
    test_assert(*ptr == 0xBEEFCAFE);

    return sel4test_get_result();
}

static int test_large_page_flush_operation(env_t env)
{
    int num_frame_types = ARRAY_SIZE(frame_types);
    seL4_CPtr frames[num_frame_types];
    int error;
    vka_t *vka = &env->vka;

    /* Grab some free vspace big enough to hold all the tests. */
    seL4_Word vstart;
    reservation_t reserve = vspace_reserve_range_aligned(&env->vspace, VSPACE_RV_SIZE, VSPACE_RV_ALIGN_BITS,
                                                         seL4_AllRights, 1, (void **) &vstart);
    test_assert(reserve.res != 0);

    /* Create us some frames to play with. */
    for (int i = 0; i < num_frame_types; i++) {
        frames[i] = vka_alloc_frame_leaky(vka, frame_types[i].size_bits);
        assert(frames[i]);
    }

    /* Map the pages in. */
    for (int i = 0; i < num_frame_types; i++) {
        uintptr_t cookie = 0;
        error = vspace_map_pages_at_vaddr(&env->vspace, &frames[i], &cookie, (void *)(vstart + frame_types[i].vaddr_offset), 1,
                                          frame_types[i].size_bits, reserve);
        test_error_eq(error, seL4_NoError);
    }

    /* See if we can invoke page flush on each of them */
    for (int i = 0; i < num_frame_types; i++) {
        error = seL4_ARM_Page_Invalidate_Data(frames[i], 0, BIT(frame_types[i].size_bits));
        test_error_eq(error, 0);
        error = seL4_ARM_Page_Clean_Data(frames[i], 0, BIT(frame_types[i].size_bits));
        test_error_eq(error, 0);
        error = seL4_ARM_Page_CleanInvalidate_Data(frames[i], 0, BIT(frame_types[i].size_bits));
        test_error_eq(error, 0);
        error = seL4_ARM_Page_Unify_Instruction(frames[i], 0, BIT(frame_types[i].size_bits));
        test_error_eq(error, 0);
        error = seL4_ARCH_PageDirectory_Invalidate_Data(env->page_directory, vstart + frame_types[i].vaddr_offset,
                                                        vstart + frame_types[i].vaddr_offset + BIT(frame_types[i].size_bits));
        test_error_eq(error, 0);
        error = seL4_ARCH_PageDirectory_Clean_Data(env->page_directory, vstart + frame_types[i].vaddr_offset,
                                                   vstart + frame_types[i].vaddr_offset + BIT(frame_types[i].size_bits));
        test_error_eq(error, 0);
        error = seL4_ARCH_PageDirectory_CleanInvalidate_Data(env->page_directory, vstart + frame_types[i].vaddr_offset,
                                                             vstart + frame_types[i].vaddr_offset + BIT(frame_types[i].size_bits));
        test_error_eq(error, 0);
        error = seL4_ARCH_PageDirectory_Unify_Instruction(env->page_directory, vstart + frame_types[i].vaddr_offset,
                                                          vstart + frame_types[i].vaddr_offset + BIT(frame_types[i].size_bits));
        test_error_eq(error, 0);
    }

    return sel4test_get_result();
}

static int test_page_directory_flush(env_t env)
{
    seL4_CPtr frame, framec;
    uintptr_t vstart, vstartc;
    volatile uint32_t *ptr, *ptrc;
    vka_t *vka;
    int err;

    vka = &env->vka;

    void *vaddr;
    void *vaddrc;
    reservation_t reservation, reservationc;

    reservation = vspace_reserve_range(&env->vspace,
                                       PAGE_SIZE_4K, seL4_AllRights, 0, &vaddr);
    assert(reservation.res);
    reservationc = vspace_reserve_range(&env->vspace,
                                        PAGE_SIZE_4K, seL4_AllRights, 1, &vaddrc);
    assert(reservationc.res);

    vstart = (uintptr_t)vaddr;
    assert(IS_ALIGNED(vstart, seL4_PageBits));
    vstartc = (uintptr_t)vaddrc;
    assert(IS_ALIGNED(vstartc, seL4_PageBits));

    ptr = (volatile uint32_t *)vstart;
    ptrc = (volatile uint32_t *)vstartc;

    /* Create a frame */
    frame = vka_alloc_frame_leaky(vka, PAGE_BITS_4K);
    test_assert(frame != seL4_CapNull);

    /* Duplicate the cap */
    framec = get_free_slot(env);
    test_assert(framec != seL4_CapNull);
    err = cnode_copy(env, frame, framec, seL4_AllRights);
    test_error_eq(err, seL4_NoError);

    /* map in a cap with cacheability */
    err = vspace_map_pages_at_vaddr(&env->vspace, &framec, NULL, vaddrc, 1, seL4_PageBits, reservationc);
    test_error_eq(err, seL4_NoError);
    /* map in a cap without cacheability */
    err = vspace_map_pages_at_vaddr(&env->vspace, &frame, NULL, vaddr, 1, seL4_PageBits, reservation);
    test_error_eq(err, seL4_NoError);

    /* Clean makes data observable to non-cached page */
    *ptr = 0xC0FFEE;
    *ptrc = 0xDEADBEEF;
    test_assert(*ptr == 0xC0FFEE);
    test_assert(*ptrc == 0xDEADBEEF);
    err = seL4_ARCH_PageDirectory_Clean_Data(env->page_directory, vstartc, vstartc + PAGE_SIZE_4K);
    assert(!err);
    test_assert(*ptr == 0xDEADBEEF);
    test_assert(*ptrc == 0xDEADBEEF);
    /* Clean/Invalidate makes data observable to non-cached page */
    *ptr = 0xC0FFEE;
    *ptrc = 0xDEADBEEF;
    test_assert(*ptr == 0xC0FFEE);
    test_assert(*ptrc == 0xDEADBEEF);
    err = seL4_ARCH_PageDirectory_CleanInvalidate_Data(env->page_directory, vstartc, vstartc + PAGE_SIZE_4K);
    assert(!err);
    test_assert(*ptr == 0xDEADBEEF);
    test_assert(*ptrc == 0xDEADBEEF);
    /* Invalidate makes RAM data observable to cached page */
    *ptr = 0xC0FFEE;
    *ptrc = 0xDEADBEEF;
    test_assert(*ptr == 0xC0FFEE);
    test_assert(*ptrc == 0xDEADBEEF);
    err = seL4_ARCH_PageDirectory_Invalidate_Data(env->page_directory, vstartc, vstartc + PAGE_SIZE_4K);
    assert(!err);
    /* In case the invalidation performs an implicit clean, write a new
       value to RAM and make sure the cached read retrieves it.
       Need to do an invalidate before retrieving though to guard
       against speculative loads */
    *ptr = 0xBEEFCAFE;
    err = seL4_ARCH_PageDirectory_Invalidate_Data(env->page_directory, vstartc, vstartc + PAGE_SIZE_4K);
    assert(!err);
    test_assert(*ptrc == 0xBEEFCAFE);
    test_assert(*ptr == 0xBEEFCAFE);

    return sel4test_get_result();
}

DEFINE_TEST(CACHEFLUSH0001, "Test a cache maintenance on pages", test_page_flush, config_set(CONFIG_HAVE_CACHE))
DEFINE_TEST(CACHEFLUSH0002, "Test a cache maintenance on page directories", test_page_directory_flush,
            config_set(CONFIG_HAVE_CACHE))
DEFINE_TEST(CACHEFLUSH0003, "Test that cache maintenance can be done on large pages", test_large_page_flush_operation,
            config_set(CONFIG_HAVE_CACHE))

#endif


static int test_page_uncached_after_retype(env_t env)
{
    seL4_CPtr frame, frame2, untyped;
    uintptr_t vstart, vstart2;
    volatile uint32_t *ptr, *ptr2;
    vka_t *vka;
    int error;

    vka = &env->vka;

    // Create 2 vspace reservations because when we revoke the untyped the state gets out of sync.
    void *vaddr;
    void *vaddr2;
    reservation_t reservation, reservation2;

    reservation = vspace_reserve_range(&env->vspace,
                                       PAGE_SIZE_4K, seL4_AllRights, 0, &vaddr);
    assert(reservation.res);
    reservation2 = vspace_reserve_range(&env->vspace,
                                        PAGE_SIZE_4K, seL4_AllRights, 0, &vaddr2);
    assert(reservation2.res);

    vstart = (uintptr_t)vaddr;
    assert(IS_ALIGNED(vstart, seL4_PageBits));
    vstart2 = (uintptr_t)vaddr2;
    assert(IS_ALIGNED(vstart2, seL4_PageBits));

    ptr = (volatile uint32_t *)vstart;
    ptr2 = (volatile uint32_t *)vstart2;

    /* Get an untyped */
    untyped = vka_alloc_untyped_leaky(vka, PAGE_BITS_4K);
    test_assert(untyped != seL4_CapNull);
    cspacepath_t src_path;
    vka_cspace_make_path(vka, untyped, &src_path);

    frame = get_free_slot(env);
    test_assert(frame != seL4_CapNull);
    cspacepath_t dest_path;
    vka_cspace_make_path(vka, frame, &dest_path);
    /* Create a frame from the untyped */
    error = seL4_Untyped_Retype(untyped, seL4_ARCH_4KPage, PAGE_BITS_4K, dest_path.root, dest_path.dest,
                                dest_path.destDepth, dest_path.offset, 1);
    test_error_eq(error, seL4_NoError);

    /* map in the without cacheability */
    error = vspace_map_pages_at_vaddr(&env->vspace, &frame, NULL, vaddr, 1, seL4_PageBits, reservation);
    test_error_eq(error, seL4_NoError);

    /* Write some data directly to RAM as it's uncached */
    *ptr = 0xC0FFEE;

    /* Revoke the untyped. This deletes the frame. */
    vka_cnode_revoke(&src_path);

    /* Create the frame with the same untyped. The kernel guarantees that the contents have been zeroed. */
    error = seL4_Untyped_Retype(untyped, seL4_ARCH_4KPage, PAGE_BITS_4K, dest_path.root, dest_path.dest,
                                dest_path.destDepth, dest_path.offset, 1);
    test_error_eq(error, seL4_NoError);


    /* map in the frame without cacheability again. */
    error = vspace_map_pages_at_vaddr(&env->vspace, &frame, NULL, vaddr2, 1, seL4_PageBits, reservation2);
    test_error_eq(error, seL4_NoError);
    /* Confirm that the contents are zeroed */
    test_assert(*ptr2 == 0x0);


    return sel4test_get_result();
}
DEFINE_TEST(CACHEFLUSH0004, "Test that mapping a frame uncached doesn't see stale data after retype",
            test_page_uncached_after_retype,
            config_set(CONFIG_HAVE_CACHE))

#if defined(CONFIG_ARCH_ARM)

/*
 * The following tests aim to test the seL4_ARM_* cache operations on every
 * kind of frame mapping that can be created. The motivation behind this is
 * that some ARM hardware cache instructions generate faults depending on the
 * permissions of the mapping and we need to ensure that the kernel catches
 * these so that a fault does not occur in kernel space. In addition, these
 * tests ensure that the kernel is enforcing a frame's cap rights, depending
 * on the cache operation.
 *
 * For each kind of mapping we need to test the following operations:
 *
 * seL4_ARM_VSpace_Clean_Data
 * seL4_ARM_VSpace_Invalidate_Data
 * seL4_ARM_VSpace_CleanInvalidate_Data
 * seL4_ARM_VSpace_Unify_Instruction
 * seL4_ARM_Page_Clean_Data
 * seL4_ARM_Page_Invalidate_Data
 * seL4_ARM_Page_CleanInvalidate_Data
 * seL4_ARM_Page_Unify_Instruction
 *
 */
static int test_cache_invalid(env_t env)
{
    seL4_CPtr frame;
    uintptr_t vstart;
    int err;
    void *vaddr;
    reservation_t reservation;
    vka_t *vka = &env->vka;

    reservation = vspace_reserve_range(&env->vspace, PAGE_SIZE_4K, seL4_AllRights, 1, &vaddr);
    assert(reservation.res);

    vstart = (uintptr_t)vaddr;
    assert(IS_ALIGNED(vstart, seL4_PageBits));

    /* Create a frame, but deliberately do not create a mapping for it. */
    frame = vka_alloc_frame_leaky(vka, PAGE_BITS_4K);
    test_assert(frame != seL4_CapNull);

    /* Top-level page table operations for invalid mappings are silently ignored by
     * the kernel so we do not test them here */

    /* Page-level operations */
    err = seL4_ARM_Page_Clean_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_FailedLookup);
    err = seL4_ARM_Page_Invalidate_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_FailedLookup);
    err = seL4_ARM_Page_CleanInvalidate_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_FailedLookup);
    err = seL4_ARM_Page_Unify_Instruction(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_FailedLookup);

    return sel4test_get_result();
}

static int test_cache_kernel_only(env_t env)
{
    /*
     * This test makes a mapping to a frame with seL4_NoRights, this means
     * that the kernel will map it in with the kernel-only VM attribute.
     */
    seL4_CPtr frame;
    uintptr_t vstart;
    vka_t *vka;
    int err;

    vka = &env->vka;

    void *vaddr;
    reservation_t reservation;

    reservation = vspace_reserve_range(&env->vspace,
                                       PAGE_SIZE_4K, seL4_NoRights, 1, &vaddr);
    assert(reservation.res);

    vstart = (uintptr_t)vaddr;
    assert(IS_ALIGNED(vstart, seL4_PageBits));

    /* Create a frame */
    frame = vka_alloc_frame_leaky(vka, PAGE_BITS_4K);
    test_assert(frame != seL4_CapNull);

    /* map in a cap with cacheability */
    err = vspace_map_pages_at_vaddr(&env->vspace, &frame, NULL, vaddr, 1, seL4_PageBits, reservation);
    test_error_eq(err, seL4_NoError);

    /* Since the mapping is kernel-only, all of these invocations should fail.*/

    /* Top-level page table operations */
    err = seL4_ARCH_PageDirectory_Clean_Data(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_IllegalOperation);
    err = seL4_ARCH_PageDirectory_Invalidate_Data(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_IllegalOperation);
    err = seL4_ARCH_PageDirectory_CleanInvalidate_Data(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_IllegalOperation);
    err = seL4_ARCH_PageDirectory_Unify_Instruction(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_IllegalOperation);
    /* Page-level operations */
    err = seL4_ARM_Page_Clean_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_IllegalOperation);
    err = seL4_ARM_Page_Invalidate_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_IllegalOperation);
    err = seL4_ARM_Page_CleanInvalidate_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_IllegalOperation);
    err = seL4_ARM_Page_Unify_Instruction(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_IllegalOperation);

    return sel4test_get_result();
}

static int test_cache_read_write(env_t env)
{
    seL4_CPtr frame;
    uintptr_t vstart;
    vka_t *vka;
    int err;

    vka = &env->vka;

    void *vaddr;
    reservation_t reservation;

    reservation = vspace_reserve_range(&env->vspace,
                                       PAGE_SIZE_4K, seL4_AllRights, 1, &vaddr);
    assert(reservation.res);

    vstart = (uintptr_t)vaddr;
    assert(IS_ALIGNED(vstart, seL4_PageBits));

    /* Create a frame */
    frame = vka_alloc_frame_leaky(vka, PAGE_BITS_4K);
    test_assert(frame != seL4_CapNull);

    /* map in a cap with cacheability */
    err = vspace_map_pages_at_vaddr(&env->vspace, &frame, NULL, vaddr, 1, seL4_PageBits, reservation);
    test_error_eq(err, seL4_NoError);

    /* Now that we have setup the read-write mapping, we can test all the caching operations. */

    /* Top-level page table operations */
    err = seL4_ARCH_PageDirectory_Clean_Data(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    err = seL4_ARCH_PageDirectory_Invalidate_Data(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    err = seL4_ARCH_PageDirectory_CleanInvalidate_Data(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    err = seL4_ARCH_PageDirectory_Unify_Instruction(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    /* Page-level operations */
    err = seL4_ARM_Page_Clean_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    err = seL4_ARM_Page_Invalidate_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    err = seL4_ARM_Page_CleanInvalidate_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    err = seL4_ARM_Page_Unify_Instruction(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);

    return sel4test_get_result();
}

static int test_cache_read_only(env_t env)
{
    seL4_CPtr frame;
    uintptr_t vstart;
    vka_t *vka;
    int err;

    vka = &env->vka;

    void *vaddr;
    reservation_t reservation;

    reservation = vspace_reserve_range(&env->vspace,
                                       PAGE_SIZE_4K, seL4_CanRead, 1, &vaddr);
    assert(reservation.res);

    vstart = (uintptr_t)vaddr;
    assert(IS_ALIGNED(vstart, seL4_PageBits));

    /* Create a frame */
    frame = vka_alloc_frame_leaky(vka, PAGE_BITS_4K);
    test_assert(frame != seL4_CapNull);

    /* map in a cap with cacheability */
    err = vspace_map_pages_at_vaddr(&env->vspace, &frame, NULL, vaddr, 1, seL4_PageBits, reservation);
    test_error_eq(err, seL4_NoError);

    /* Now that we have setup the read-only mapping, we can test all the caching operations. */

    /* Top-level page table operations */
    err = seL4_ARCH_PageDirectory_Clean_Data(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    err = seL4_ARCH_PageDirectory_Invalidate_Data(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_IllegalOperation);
    err = seL4_ARCH_PageDirectory_CleanInvalidate_Data(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    err = seL4_ARCH_PageDirectory_Unify_Instruction(env->page_directory, vstart, vstart + PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    /* Page-level operations */
    err = seL4_ARM_Page_Clean_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    err = seL4_ARM_Page_Invalidate_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_IllegalOperation);
    err = seL4_ARM_Page_CleanInvalidate_Data(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);
    err = seL4_ARM_Page_Unify_Instruction(vstart, 0, PAGE_SIZE_4K);
    test_error_eq(err, seL4_NoError);

    return sel4test_get_result();
}

DEFINE_TEST(CACHEFLUSH0005, "Test cache maintenance on invalid mappings", test_cache_invalid,
            config_set(CONFIG_HAVE_CACHE))
DEFINE_TEST(CACHEFLUSH0006, "Test cache maintenance on kernel-only mappings", test_cache_kernel_only,
            config_set(CONFIG_HAVE_CACHE))
DEFINE_TEST(CACHEFLUSH0007, "Test cache maintenance on read-write mappings", test_cache_read_write,
            config_set(CONFIG_HAVE_CACHE))
DEFINE_TEST(CACHEFLUSH0008, "Test cache maintenance on read-only mappings", test_cache_read_only,
            config_set(CONFIG_HAVE_CACHE))

#endif
