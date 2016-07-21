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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <sel4/sel4.h>
#include <vka/object.h>
#include <sel4utils/util.h>
#include <sel4utils/mapping.h>

#include "../helpers.h"
#include "frame_type.h"

static void
fill_memory(seL4_Word addr, seL4_Word size_bytes)
{
    test_assert_fatal(IS_ALIGNED(addr, sizeof(seL4_Word)));
    test_assert_fatal(IS_ALIGNED(size_bytes, sizeof(seL4_Word)));
    seL4_Word *p = (void*)addr;
    seL4_Word size_words = size_bytes / sizeof(seL4_Word);
    while (size_words--) {
        *p++ = size_words ^ 0xbeefcafe;
    }
}

static int
test_frame_recycle(env_t env)
{
    int num_frame_types = ARRAY_SIZE(frame_types);
    seL4_CPtr frames[num_frame_types];
    int error;
    vka_t *vka = &env->vka;

    /* Grab some free vspace big enough to hold all the tests. */
    uintptr_t vstart;
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

        /* Fill each of these with random junk and then recycle them. */
        fill_memory(
            vstart + frame_types[i].vaddr_offset,
            BIT(frame_types[i].size_bits));
    }

    /* Now recycle all of them. */
    for (int i = 0; i < num_frame_types; i++) {
        error = seL4_CNode_Recycle(env->cspace_root,
                                   frames[i], seL4_WordBits);
        test_assert(!error);
    }

    /* Map them back in again. */
    for (int i = 0; i < num_frame_types; i++) {
        error = seL4_ARCH_Page_Map(frames[i], env->page_directory,
                                   vstart + frame_types[i].vaddr_offset, seL4_AllRights,
                                   seL4_ARCH_Default_VMAttributes);
        test_assert(error == seL4_NoError);
    }

    /* Now check they are zero. */
    for (int i = 0; i < num_frame_types; i++) {
        test_assert(check_zeroes(vstart + frame_types[i].vaddr_offset,
                                 BIT(frame_types[i].size_bits)));
    }

    vspace_free_reservation(&env->vspace, reserve);

    return sel4test_get_result();
}
DEFINE_TEST(FRAMERECYCLE0001, "Test recycling of frame caps", test_frame_recycle)

static int
test_frame_exported(env_t env)
{
    /* Reserve a location that is aligned and large enough to map our
     * largest kind of frame */
    void *vaddr;
    reservation_t reserve = vspace_reserve_range_aligned(&env->vspace, VSPACE_RV_SIZE, VSPACE_RV_ALIGN_BITS,
                                                         seL4_AllRights, 1, &vaddr);
    test_assert(reserve.res);

    /* loop through frame sizes, allocate, map and touch them until we run out
     * of memory. */
    size_t mem_total = 0;
    int err;
    for (int i = 0; i < ARRAY_SIZE(frame_types); i++) {
        while (1) {
            /* Allocate the frame */
            seL4_CPtr frame = vka_alloc_frame_leaky(&env->vka, frame_types[i].size_bits);
            if (!frame) {
                break;
            }
            mem_total += BIT(frame_types[i].size_bits);

            uintptr_t cookie = 0;
            err = vspace_map_pages_at_vaddr(&env->vspace, &frame, &cookie, (void*)vaddr, 1, frame_types[i].size_bits, reserve);
            test_assert(err == seL4_NoError);

            /* Touch the memory */
            char *data = (char*)vaddr;

            *data = 'U';
            test_assert(*data == 'U');

            err = seL4_ARCH_Page_Remap(frame,
                                       env->page_directory,
                                       seL4_AllRights,
                                       seL4_ARCH_Default_VMAttributes);
            test_assert(!err);
            /* Touch the memory again */
            *data = 'V';
            test_assert(*data == 'V');

            vspace_unmap_pages(&env->vspace, (void*)vaddr, 1, frame_types[i].size_bits, VSPACE_PRESERVE);
            test_assert(err == seL4_NoError);
        }
    }
    return sel4test_get_result();
}
DEFINE_TEST(FRAMEEXPORTS0001, "Test that we can access all exported frames", test_frame_exported)

#if defined(CONFIG_ARCH_ARM)
/* XN support is only implemented for ARM currently. */

/* Function that generates a fault. If we're mapped XN we should instruction
 * fault at the start of the function. If not we should data fault on 0x42.
 */
static int fault(seL4_Word arg1, seL4_Word arg2, seL4_Word arg3, seL4_Word arg4)
{
    *(char*)0x42 = 'c';
    return 0;
}

/* Wait for a VM fault originating on the given EP the return the virtual
 * address it occurred at. Returns the sentinel 0xffffffff if the message
 * received was not a VM fault.
 */
static int handle(seL4_CPtr fault_ep, seL4_Word arg2, seL4_Word arg3, seL4_Word arg4)
{
    seL4_MessageInfo_t info = seL4_Recv(fault_ep, NULL);
    if (seL4_MessageInfo_get_label(info) == seL4_Fault_VMFault) {
        return (int)seL4_GetMR(1);
    } else {
        return (int)0xffffffff;
    }
}

static int test_xn(env_t env, seL4_ArchObjectType frame_type)
{
    int err;
    /* Find the size of the frame type we want to test. */
    int sz_bits = 0;
    for (unsigned int i = 0; i < ARRAY_SIZE(frame_types); i++) {
        if (frame_types[i].type == frame_type) {
            sz_bits = frame_types[i].size_bits;
            break;
        }
    }
    test_assert(sz_bits != 0);

    /* Get ourselves a frame. */
    seL4_CPtr frame_cap = vka_alloc_frame_leaky(&env->vka, sz_bits);
    test_assert(frame_cap != seL4_CapNull);

    /* Map it in */
    uintptr_t cookie = 0;
    void *dest = vspace_map_pages(&env->vspace, &frame_cap, &cookie, seL4_AllRights, 1, sz_bits, 1);
    test_assert(dest != NULL);

    /* Set up a function we're going to have another thread call. Assume that
     * the function is no more than 100 bytes long.
     */
    memcpy(dest, (void*)fault, 100);

    /* First setup a fault endpoint.
     */
    seL4_CPtr fault_ep = vka_alloc_endpoint_leaky(&env->vka);
    cspacepath_t path;
    vka_cspace_make_path(&env->vka, fault_ep, &path);
    test_assert(fault_ep != seL4_CapNull);

    /* Then setup the thread that will, itself, fault. */
    helper_thread_t faulter;
    create_helper_thread(env, &faulter);
    set_helper_priority(&faulter, 100);
    err = seL4_TCB_Configure(faulter.thread.tcb.cptr,
                             fault_ep,
                             fault_ep, 
                             seL4_Prio_new(100, 0, 0, 0),
                             faulter.thread.sched_context.cptr,
                             env->cspace_root,
                             seL4_CapData_Guard_new(0, seL4_WordBits - env->cspace_size_bits),
                             env->page_directory, seL4_NilData,
                             faulter.thread.ipc_buffer_addr,
                             faulter.thread.ipc_buffer);
    start_helper(env, &faulter, dest, 0, 0, 0 ,0);
    test_assert(err == 0);

    /* Now a fault handler that will catch and diagnose its fault. */
    helper_thread_t handler;
    create_helper_thread(env, &handler);
    set_helper_priority(&handler, 100);
    start_helper(env, &handler, handle, fault_ep, 0, 0, 0);

    /* Wait for the fault to happen */
    void *res = (void*)wait_for_helper(&handler);

    test_assert(res == (void*)0x42);

    cleanup_helper(env, &handler);
    cleanup_helper(env, &faulter);

    /* Now let's remap the page with XN set and confirm that we can't execute
     * in it any more.
     */
    err = seL4_ARM_Page_Remap(frame_cap, env->page_directory, seL4_AllRights,
                              seL4_ARM_Default_VMAttributes | seL4_ARM_ExecuteNever);
    test_assert(err == 0);

    /* The page should still contain our code from before. */
    test_assert(!memcmp(dest, (void*)fault, 100));

    /* We need to reallocate a fault EP because the thread cleanup above
     * inadvertently destroys the EP we were using.
     */
    fault_ep = vka_alloc_endpoint_leaky(&env->vka);
    test_assert(fault_ep != seL4_CapNull);

    /* Recreate our two threads. */
    create_helper_thread(env, &faulter);
    set_helper_priority(&faulter, 100);
    err = seL4_TCB_Configure(faulter.thread.tcb.cptr,
                             fault_ep,
                             fault_ep, 
                             seL4_Prio_new(100, 0, 0, 0),
                             faulter.thread.sched_context.cptr,
                             env->cspace_root,
                             seL4_CapData_Guard_new(0, seL4_WordBits - env->cspace_size_bits),
                             env->page_directory, seL4_NilData,
                             faulter.thread.ipc_buffer_addr,
                             faulter.thread.ipc_buffer);
    start_helper(env, &faulter, dest, 0, 0, 0 ,0);
    create_helper_thread(env, &handler);
    set_helper_priority(&handler, 100);
    start_helper(env, &handler, handle, fault_ep, 0, 0, 0);

    /* Wait for the fault to happen */
    res = (void*)wait_for_helper(&handler);

    /* Confirm that, this time, we faulted at the start of the XN-mapped page. */
    test_assert(res == (void*)dest);

    /* Resource tear down. */
    cleanup_helper(env, &handler);
    cleanup_helper(env, &faulter);

    return sel4test_get_result();
}

static int test_xn_small_frame(env_t env)
{
    return test_xn(env, seL4_ARM_SmallPageObject);
}
DEFINE_TEST(FRAMEXN0001, "Test that we can map a small frame XN", test_xn_small_frame)

static int test_xn_large_frame(env_t env)
{
    return test_xn(env, seL4_ARM_LargePageObject);
}
DEFINE_TEST(FRAMEXN0002, "Test that we can map a large frame XN", test_xn_large_frame)

#endif
