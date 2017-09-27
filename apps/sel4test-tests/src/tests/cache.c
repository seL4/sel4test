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
#include <stddef.h>
#include <stdio.h>
#include <sel4/sel4.h>
#include <vka/object.h>
#include <sel4utils/util.h>
#include <sel4utils/arch/cache.h>

#include "../helpers.h"
#include "frame_type.h"

#if defined(CONFIG_ARCH_ARM)
static int
test_page_flush(env_t env)
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

    ptr = (volatile uint32_t*)vstart;
    ptrc = (volatile uint32_t*)vstartc;

    /* Create a frame */
    frame = vka_alloc_frame_leaky(vka, PAGE_BITS_4K);
    test_assert(frame != seL4_CapNull);

    /* Duplicate the cap */
    framec = get_free_slot(env);
    test_assert(framec != seL4_CapNull);
    err = cnode_copy(env, frame, framec, seL4_AllRights);
    test_assert(!err);

    /* map in a cap with cacheability */
    err = vspace_map_pages_at_vaddr(&env->vspace, &framec, NULL, vaddrc, 1, seL4_PageBits, reservationc);
    test_assert(!err);
    /* map in a cap without cacheability */
    err = vspace_map_pages_at_vaddr(&env->vspace, &frame, NULL, vaddr, 1, seL4_PageBits, reservation);
    test_assert(!err);

    /* Clean makes data observable to non-cached page */
    *ptr = 0xC0FFEE;
    *ptrc = 0xDEADBEEF;
    test_assert(*ptr == 0xC0FFEE);
    test_assert(*ptrc == 0xDEADBEEF);
    err = seL4_ARM_Page_Clean_Data(framec, 0, PAGE_SIZE_4K);
    assert(!err);
    test_assert(*ptr == 0xDEADBEEF);
    test_assert(*ptrc == 0xDEADBEEF);
    /* Clean/Invalidate makes data observable to non-cached page */
    *ptr = 0xC0FFEE;
    *ptrc = 0xDEADBEEF;
    test_assert(*ptr == 0xC0FFEE);
    test_assert(*ptrc == 0xDEADBEEF);
    err = seL4_ARM_Page_CleanInvalidate_Data(framec, 0, PAGE_SIZE_4K);
    assert(!err);
    test_assert(*ptr == 0xDEADBEEF);
    test_assert(*ptrc == 0xDEADBEEF);
    /* Invalidate makes RAM data observable to cached page */
    *ptr = 0xC0FFEE;
    *ptrc = 0xDEADBEEF;
    test_assert(*ptr == 0xC0FFEE);
    test_assert(*ptrc == 0xDEADBEEF);
    err = seL4_ARM_Page_Invalidate_Data(framec, 0, PAGE_SIZE_4K);
    assert(!err);

    /* In case the invalidation performs an implicit clean, write a new
       value to RAM and make sure the cached read retrieves it
       Remember to drain any store buffer!
    */
    *ptr = 0xBEEFCAFE;
#if defined(CONFIG_ARCH_AARCH32)
#ifndef CONFIG_ARCH_ARM_V6
    asm volatile ("dmb" ::: "memory");
#endif /* CONFIG_ARCH_ARM_V6 */
#elif defined(CONFIG_ARCH_AARCH64)
    asm volatile ("dmb sy" ::: "memory");
#endif /* CONFIG_ARCH_AARCHxx */
    test_assert(*ptrc == 0xBEEFCAFE);
    test_assert(*ptr == 0xBEEFCAFE);

    return sel4test_get_result();
}

static int
test_large_page_flush_operation(env_t env)
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
        error = vspace_map_pages_at_vaddr(&env->vspace, &frames[i], &cookie, (void*)(vstart + frame_types[i].vaddr_offset), 1, frame_types[i].size_bits, reserve);
        test_assert(error == seL4_NoError);
    }

    /* See if we can invoke page flush on each of them */
    for (int i = 0; i < num_frame_types; i++) {
        error = seL4_ARM_Page_Invalidate_Data(frames[i], 0, BIT(frame_types[i].size_bits));
        test_assert(error == 0);
        error = seL4_ARM_Page_Clean_Data(frames[i], 0, BIT(frame_types[i].size_bits));
        test_assert(error == 0);
        error = seL4_ARM_Page_CleanInvalidate_Data(frames[i], 0, BIT(frame_types[i].size_bits));
        test_assert(error == 0);
        error = seL4_ARM_Page_Unify_Instruction(frames[i], 0, BIT(frame_types[i].size_bits));
        test_assert(error == 0);
        error = seL4_ARCH_PageDirectory_Invalidate_Data(env->page_directory, vstart + frame_types[i].vaddr_offset, vstart + frame_types[i].vaddr_offset + BIT(frame_types[i].size_bits));
        test_assert(error == 0);
        error = seL4_ARCH_PageDirectory_Clean_Data(env->page_directory, vstart + frame_types[i].vaddr_offset, vstart + frame_types[i].vaddr_offset + BIT(frame_types[i].size_bits));
        test_assert(error == 0);
        error = seL4_ARCH_PageDirectory_CleanInvalidate_Data(env->page_directory, vstart + frame_types[i].vaddr_offset, vstart + frame_types[i].vaddr_offset + BIT(frame_types[i].size_bits));
        test_assert(error == 0);
        error = seL4_ARCH_PageDirectory_Unify_Instruction(env->page_directory, vstart + frame_types[i].vaddr_offset, vstart + frame_types[i].vaddr_offset + BIT(frame_types[i].size_bits));
        test_assert(error == 0);
    }

    return sel4test_get_result();
}

static int
test_page_directory_flush(env_t env)
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

    ptr = (volatile uint32_t*)vstart;
    ptrc = (volatile uint32_t*)vstartc;

    /* Create a frame */
    frame = vka_alloc_frame_leaky(vka, PAGE_BITS_4K);
    test_assert(frame != seL4_CapNull);

    /* Duplicate the cap */
    framec = get_free_slot(env);
    test_assert(framec != seL4_CapNull);
    err = cnode_copy(env, frame, framec, seL4_AllRights);
    test_assert(!err);

    /* map in a cap with cacheability */
    err = vspace_map_pages_at_vaddr(&env->vspace, &framec, NULL, vaddrc, 1, seL4_PageBits, reservationc);
    test_assert(!err);
    /* map in a cap without cacheability */
    err = vspace_map_pages_at_vaddr(&env->vspace, &frame, NULL, vaddr, 1, seL4_PageBits, reservation);
    test_assert(!err);

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
DEFINE_TEST(CACHEFLUSH0002, "Test a cache maintenance on page directories", test_page_directory_flush, config_set(CONFIG_HAVE_CACHE))
DEFINE_TEST(CACHEFLUSH0003, "Test that cache maintenance can be done on large pages", test_large_page_flush_operation, config_set(CONFIG_HAVE_CACHE))

#endif
