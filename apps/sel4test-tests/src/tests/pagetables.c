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
#include <assert.h>

#include <sel4/sel4.h>
#include <vka/object.h>
#include <utils/util.h>

#include "../helpers.h"
#include "frame_type.h"

#if defined(CONFIG_ARCH_ARM)
static int
fill_memory(seL4_Word addr, seL4_Word size_bytes)
{
    test_assert(IS_ALIGNED(addr, sizeof(seL4_Word)));
    test_assert(IS_ALIGNED(size_bytes, sizeof(seL4_Word)));
    seL4_Word *p = (void*)addr;
    seL4_Word size_words = size_bytes / sizeof(seL4_Word);
    while (size_words--) {
        *p++ = size_words ^ 0xbeefcafe;
    }
    return SUCCESS;
}

static int
__attribute__((warn_unused_result))
check_memory(seL4_Word addr, seL4_Word size_bytes)
{
    test_assert(IS_ALIGNED(addr, sizeof(seL4_Word)));
    test_assert(IS_ALIGNED(size_bytes, sizeof(seL4_Word)));
    seL4_Word *p = (void*)addr;
    seL4_Word size_words = size_bytes / sizeof(seL4_Word);
    while (size_words--) {
        if (*p++ != (size_words ^ 0xbeefcafe)) {
            return 0;
        }
    }
    return 1;
}

#if defined(CONFIG_ARCH_AARCH32)
#define LPAGE_SIZE   (1 << (vka_get_object_size(seL4_ARM_LargePageObject, 0)))
#define SECT_SIZE    (1 << (vka_get_object_size(seL4_ARM_SectionObject, 0)))
#define SUPSECT_SIZE (1 << (vka_get_object_size(seL4_ARM_SuperSectionObject, 0)))

/* Assumes caps can be mapped in at vaddr (= [vaddr,vaddr + 3*size) */
static int
do_test_pagetable_tlbflush_on_vaddr_reuse(env_t env, seL4_CPtr cap1, seL4_CPtr cap2, seL4_Word vstart,
                                          seL4_Word size)
{
    int error;
    seL4_Word vaddr;
    volatile seL4_Word* vptr = NULL;

    /* map, touch page 1 */
    vaddr = vstart;
    vptr = (seL4_Word*)vaddr;
    error = seL4_ARM_Page_Map(cap1, env->page_directory,
                              vaddr, seL4_AllRights,
                              seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);
    vptr[0] = 1;
    error = seL4_ARM_Page_Unmap(cap1);
    test_assert(error == 0);

    /* map, touch page 2 */
    vaddr = vstart + size;
    vptr = (seL4_Word*)vaddr;
    error = seL4_ARM_Page_Map(cap2, env->page_directory,
                              vaddr, seL4_AllRights,
                              seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);
    vptr[0] = 2;
    error = seL4_ARM_Page_Unmap(cap2);
    test_assert(error == 0);

    /* Test TLB */
    vaddr = vstart + 2 * size;
    vptr = (seL4_Word*)vaddr;
    error = seL4_ARM_Page_Map(cap1, env->page_directory,
                              vaddr, seL4_AllRights,
                              seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);
    test_check(vptr[0] == 1);

    error = seL4_ARM_Page_Map(cap2, env->page_directory,
                              vaddr, seL4_AllRights,
                              seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);
    test_check(vptr[0] == 2 || !"TLB contains stale entry");

    /* clean up */
    error = seL4_ARM_Page_Unmap(cap1);
    test_assert(error == 0);
    error = seL4_ARM_Page_Unmap(cap2);
    test_assert(error == 0);
    return sel4test_get_result();
}

static int
test_pagetable_arm(env_t env)
{
    int error;

    /* Grab some free vspace big enough to hold a couple of supersections. */
    seL4_Word vstart;
    reservation_t reserve = vspace_reserve_range(&env->vspace, SUPSECT_SIZE * 4,
                                                  seL4_AllRights, 1, (void **) &vstart);
    vstart = ALIGN_UP(vstart, SUPSECT_SIZE * 2);

    /* Create us some frames to play with. */
    seL4_CPtr small_page = vka_alloc_object_leaky(&env->vka, seL4_ARM_SmallPageObject, 0);
    seL4_CPtr small_page2 = vka_alloc_object_leaky(&env->vka, seL4_ARM_SmallPageObject, 0);
    seL4_CPtr large_page = vka_alloc_object_leaky(&env->vka, seL4_ARM_LargePageObject, 0);
    seL4_CPtr section = vka_alloc_object_leaky(&env->vka, seL4_ARM_SectionObject, 0);
    seL4_CPtr supersection = vka_alloc_object_leaky(&env->vka, seL4_ARM_SuperSectionObject, 0);

    test_assert(small_page != 0);
    test_assert(small_page2 != 0);
    test_assert(large_page != 0);
    test_assert(section != 0);
    test_assert(supersection != 0);

    /* Also create a pagetable to map the pages into. */
    seL4_CPtr pt = vka_alloc_page_table_leaky(&env->vka);

    /* Check we can't map the supersection in at an address it's not aligned
     * to. */
    test_assert(supersection != 0);
    error = seL4_ARM_Page_Map(supersection, env->page_directory,
                              vstart + SUPSECT_SIZE / 2, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_AlignmentError);

    /* Check we can map it in somewhere correctly aligned. */
    error = seL4_ARM_Page_Map(supersection, env->page_directory,
                              vstart + SUPSECT_SIZE, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Now fill it with stuff to check later. */
    /* TDDO fx these constants */
    error = fill_memory(vstart + SUPSECT_SIZE, SUPSECT_SIZE);
    test_assert(error == SUCCESS);

    /* Check we now can't map a section over the top. */
    error = seL4_ARM_Page_Map(section, env->page_directory,
                              vstart + SUPSECT_SIZE + SUPSECT_SIZE / 2, seL4_AllRights,
                              seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_DeleteFirst);

    /* Unmapping the section shouldn't do anything. */
    error = seL4_ARM_Page_Unmap(section);
    test_assert(error == 0);

    /* Unmap supersection and try again. */
    error = seL4_ARM_Page_Unmap(supersection);
    test_assert(error == 0);

    error = seL4_ARM_Page_Map(section, env->page_directory,
                              vstart + SUPSECT_SIZE + SUPSECT_SIZE / 2, seL4_AllRights,
                              seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    fill_memory(vstart + SUPSECT_SIZE + SUPSECT_SIZE / 2, SECT_SIZE);

    /* Now that a section is there, see if we can map a supersection over the
     * top. */
    error = seL4_ARM_Page_Map(supersection, env->page_directory,
                              vstart + SUPSECT_SIZE, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_DeleteFirst);

    if (!check_memory(vstart + SUPSECT_SIZE + SUPSECT_SIZE / 2, SECT_SIZE)) {
        return FAILURE;
    }

    /* Unmap the section, leaving nothing mapped. */
    error = seL4_ARM_Page_Unmap(section);
    test_assert(error == 0);

    /* Now, try mapping in the supersection into two places. */
    error = seL4_ARM_Page_Map(supersection, env->page_directory,
                              vstart, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);
    error = seL4_ARM_Page_Map(supersection, env->page_directory,
                              vstart + SUPSECT_SIZE, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_InvalidCapability);

    /* Now check what we'd written earlier is still there. */
    test_assert(check_memory(vstart, SUPSECT_SIZE));

    /* Try mapping the section into two places. */
    error = seL4_ARM_Page_Map(section, env->page_directory,
                              vstart + 2 * SUPSECT_SIZE, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);
    error = seL4_ARM_Page_Map(section, env->page_directory,
                              vstart + SUPSECT_SIZE, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_InvalidCapability);

    /* Unmap everything again. */
    error = seL4_ARM_Page_Unmap(section);
    test_assert(error == 0);
    error = seL4_ARM_Page_Unmap(supersection);
    test_assert(error == 0);

    /* Map a large page somewhere with no pagetable. */
    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_FailedLookup);

    /* Map a pagetable at an unaligned address. Oddly enough, this will succeed
     * as the kernel silently truncates the address to the nearest correct
     * boundary. */
    error = seL4_ARM_PageTable_Map(pt, env->page_directory,
                                   vstart + SECT_SIZE + 10, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Map the large page in at an unaligned address. */
    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart + SECT_SIZE + LPAGE_SIZE / 2,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_AlignmentError);

    /* Map the large page in at an aligned address. */
    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart + SECT_SIZE + LPAGE_SIZE,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Map it again, and it should fail. */
    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart + SECT_SIZE + 2 * LPAGE_SIZE,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_InvalidCapability);

    /* Fill it with more stuff to check. */
    fill_memory(vstart + SECT_SIZE + LPAGE_SIZE, LPAGE_SIZE);

    /* Try mapping a small page over the top of it. */
    error = seL4_ARM_Page_Map(small_page, env->page_directory,
                              vstart + SECT_SIZE + LPAGE_SIZE,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_DeleteFirst);

    /* Try mapping a small page elsewhere useful. */
    error = seL4_ARM_Page_Map(small_page, env->page_directory,
                              vstart + SECT_SIZE + 3 * LPAGE_SIZE,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Fill the small page with useful data too. */
    fill_memory(vstart + SECT_SIZE + 3 * LPAGE_SIZE, PAGE_SIZE_4K);

    /* Pull the plug on the page table. Apparently a recycle isn't good enough.
     * Get another pagetable! */
    error = seL4_CNode_Delete(env->cspace_root, pt, seL4_WordBits);
    test_assert(error == 0);
    error = seL4_ARM_Page_Unmap(small_page);
    test_assert(error == 0);
    error = seL4_ARM_Page_Unmap(large_page);
    test_assert(error == 0);
    pt = vka_alloc_page_table_leaky(&env->vka);

    /* Map the pagetable somewhere new. */
    error = seL4_ARM_PageTable_Map(pt, env->page_directory,
                                   vstart, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Map our small page and large page back in. */
    error = seL4_ARM_Page_Map(small_page, env->page_directory,
                              vstart + PAGE_SIZE_4K,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart + LPAGE_SIZE,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Check their contents. */
    test_assert(check_memory(vstart + PAGE_SIZE_4K, PAGE_SIZE_4K));
    test_assert(check_memory(vstart + LPAGE_SIZE, LPAGE_SIZE));

    /* Now unmap the small page */
    error = seL4_ARM_Page_Unmap(small_page);
    test_assert(error == 0);

    /* Unmap the large page. */
    error = seL4_ARM_Page_Unmap(large_page);
    test_assert(error == 0);

    /* Now map the large page where the small page was. */
    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Now check the contents of the large page. */
    test_assert(check_memory(vstart, LPAGE_SIZE));

    /* Map the small page elsewhere. */
    error = seL4_ARM_Page_Map(small_page, env->page_directory,
                              vstart + LPAGE_SIZE,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Now unmap the small page */
    error = seL4_ARM_Page_Unmap(small_page);
    test_assert(error == 0);

    /* Map a different small page in its place. */
    error = seL4_ARM_Page_Map(small_page2, env->page_directory,
                              vstart + LPAGE_SIZE,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Fill it in with stuff. */
    fill_memory(vstart + LPAGE_SIZE, PAGE_SIZE_4K);

    vspace_free_reservation(&env->vspace, reserve);

    return sel4test_get_result();
}
DEFINE_TEST(PT0001, "Fun with page tables on ARM", test_pagetable_arm, true)

static int
test_pagetable_tlbflush_on_vaddr_reuse(env_t env)
{
    int error;
    int result = SUCCESS;

    /* Grab some free vspace big enough to hold a couple of supersections. */
    void *vstart;
    reservation_t reserve = vspace_reserve_range_aligned(&env->vspace, VSPACE_RV_SIZE, VSPACE_RV_ALIGN_BITS,
            seL4_AllRights, 1, &vstart);
    test_assert(reserve.res);

    seL4_CPtr cap1, cap2;
    /* Create us some frames to play with. */

    /* Also create a pagetable to map the pages into. */
    seL4_CPtr pt = vka_alloc_page_table_leaky(&env->vka);

    /* supersection */
    cap1 = vka_alloc_object_leaky(&env->vka, seL4_ARM_SuperSectionObject, 0);
    cap2 = vka_alloc_object_leaky(&env->vka, seL4_ARM_SuperSectionObject, 0);
    if (do_test_pagetable_tlbflush_on_vaddr_reuse(env, cap1, cap2, (uintptr_t)vstart, SUPSECT_SIZE) == FAILURE) {
        result = FAILURE;
    }
    /* section */
    cap1 = vka_alloc_object_leaky(&env->vka, seL4_ARM_SectionObject, 0);
    cap2 = vka_alloc_object_leaky(&env->vka, seL4_ARM_SectionObject, 0);
    if (do_test_pagetable_tlbflush_on_vaddr_reuse(env, cap1, cap2, (uintptr_t)vstart, SUPSECT_SIZE) == FAILURE) {
        result = FAILURE;
    }

    /* map a PT for smaller page objects */
    error = seL4_ARM_PageTable_Map(pt, env->page_directory,
                                   (seL4_Word)vstart, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Large page */
    cap1 = vka_alloc_object_leaky(&env->vka, seL4_ARM_LargePageObject, 0);
    cap2 = vka_alloc_object_leaky(&env->vka, seL4_ARM_LargePageObject, 0);
    if (do_test_pagetable_tlbflush_on_vaddr_reuse(env, cap1, cap2, (uintptr_t)vstart, LPAGE_SIZE) == FAILURE) {
        result = FAILURE;
    }
    /* small page */
    cap1 = vka_alloc_object_leaky(&env->vka, seL4_ARM_SmallPageObject, 0);
    cap2 = vka_alloc_object_leaky(&env->vka, seL4_ARM_SmallPageObject, 0);
    if (do_test_pagetable_tlbflush_on_vaddr_reuse(env, cap1, cap2, (uintptr_t)vstart, PAGE_SIZE_4K) == FAILURE) {
        result = FAILURE;
    }

    return result;
}
DEFINE_TEST(PT0002, "Reusing virtual addresses flushes TLB", test_pagetable_tlbflush_on_vaddr_reuse, true)

#elif defined(CONFIG_ARCH_AARCH64)
#define LPAGE_SIZE   (1 << (vka_get_object_size(seL4_ARM_LargePageObject, 0)))

static int
test_pagetable_arm(env_t env)
{
    int error;

    /* Grab some free vspace big enough to hold a couple of large pages. */
    seL4_Word vstart;
    reservation_t reserve = vspace_reserve_range(&env->vspace, LPAGE_SIZE * 4,
                                                  seL4_AllRights, 1, (void **) &vstart);
    vstart = ALIGN_UP(vstart, LPAGE_SIZE * 2);

    /* Create us some frames to play with. */
    seL4_CPtr small_page = vka_alloc_object_leaky(&env->vka, seL4_ARM_SmallPageObject, 0);
    seL4_CPtr small_page2 = vka_alloc_object_leaky(&env->vka, seL4_ARM_SmallPageObject, 0);
    seL4_CPtr large_page = vka_alloc_object_leaky(&env->vka, seL4_ARM_LargePageObject, 0);

    test_assert(small_page != 0);
    test_assert(small_page2 != 0);
    test_assert(large_page != 0);

    /* Also create a pagetable to map the pages into. */
    seL4_CPtr pt = vka_alloc_page_table_leaky(&env->vka);

    /* Check we can't map the large page in at an address it's not aligned to. */
    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart + LPAGE_SIZE / 2, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_AlignmentError);

    /* Check we can map it in somewhere correctly aligned. */
    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart + LPAGE_SIZE, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

   /* Unmap large page and try again. */
    error = seL4_ARM_Page_Unmap(large_page);
    test_assert(error == 0);

    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart + LPAGE_SIZE, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    fill_memory(vstart + LPAGE_SIZE, LPAGE_SIZE);

    /* Unmap the large page, leaving nothing mapped. */
    error = seL4_ARM_Page_Unmap(large_page);
    test_assert(error == 0);

    /* Now, try mapping in the large page into two places. */
    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);
    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart + LPAGE_SIZE, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_InvalidCapability);

    /* Now check what we'd written earlier is still there. */
    test_assert(check_memory(vstart, LPAGE_SIZE));

    /* Unmap everything again. */
    error = seL4_ARM_Page_Unmap(large_page);
    test_assert(error == 0);

    /* Map a small page somewhere with no pagetable. */
    error = seL4_ARM_Page_Map(small_page, env->page_directory,
                              vstart, seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == seL4_FailedLookup);

    /* Map a pagetable at an unaligned address. Oddly enough, this will succeed
     * as the kernel silently truncates the address to the nearest correct
     * boundary. */
    error = seL4_ARM_PageTable_Map(pt, env->page_directory,
                                   vstart + LPAGE_SIZE + 10, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Try mapping a small page. */
    error = seL4_ARM_Page_Map(small_page, env->page_directory, vstart + LPAGE_SIZE + PAGE_SIZE_4K,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Fill the small page with useful data too. */
    fill_memory(vstart + LPAGE_SIZE + PAGE_SIZE_4K, PAGE_SIZE_4K);

    /* Pull the plug on the page table. Apparently a recycle isn't good enough.
     * Get another pagetable! */
    error = seL4_CNode_Delete(env->cspace_root, pt, seL4_WordBits);
    test_assert(error == 0);
    error = seL4_ARM_Page_Unmap(small_page);
    test_assert(error == 0);
    error = seL4_ARM_Page_Unmap(large_page);
    test_assert(error == 0);
    pt = vka_alloc_page_table_leaky(&env->vka);

    /* Map the pagetable somewhere new. */
    error = seL4_ARM_PageTable_Map(pt, env->page_directory,
                                   vstart, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Map our small page and large page back in. */
    error = seL4_ARM_Page_Map(small_page, env->page_directory,
                              vstart + PAGE_SIZE_4K,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    error = seL4_ARM_Page_Map(large_page, env->page_directory,
                              vstart + LPAGE_SIZE,
                              seL4_AllRights, seL4_ARM_Default_VMAttributes);
    test_assert(error == 0);

    /* Check their contents. */
    test_assert(check_memory(vstart + PAGE_SIZE_4K, PAGE_SIZE_4K));
    test_assert(check_memory(vstart + LPAGE_SIZE, LPAGE_SIZE));

    /* Now unmap the small page */
    error = seL4_ARM_Page_Unmap(small_page);
    test_assert(error == 0);

    /* Unmap the large page. */
    error = seL4_ARM_Page_Unmap(large_page);
    test_assert(error == 0);

    vspace_free_reservation(&env->vspace, reserve);

    return sel4test_get_result();
}
DEFINE_TEST(PT0001, "Fun with page tables on ARM", test_pagetable_arm, true)
#endif /* CONFIG_ARCH_AARCHxx */
#endif /* CONFIG_ARCH_ARM */
