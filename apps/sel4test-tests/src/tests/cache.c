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
    /*
     * If invalidate works, both reads should get the uncached value.
     *
     * If invalidate does an implicit clean, both reads should get the cached value.
     * The latter is also true if a random cache eviction happens before the call to
     * seL4_ARM_Page_Invalidate_Data finishes.
     *
     * Whatever happens, both values should always be the same now.
     */
    test_assert(*ptr == *ptrc);

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
    /*
     * If invalidate works, both reads should get the uncached value.
     *
     * If invalidate does an implicit clean, both reads should get the cached value.
     * The latter is also true if a random cache eviction happens before the call to
     * seL4_ARM_Page_Invalidate_Data finishes.
     *
     * Whatever happens, both values should always be the same now.
     */
    test_assert(*ptr == *ptrc);

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
