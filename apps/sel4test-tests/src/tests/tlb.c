/*
 * Copyright 2026, Proofcraft Pty Ltd
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Regression test for TLB invalidation of a VSpace that is not the current
 * VSpace in ARM hyp mode. In hyp mode, TLB flushes only affect the VMID that is
 * currently loaded in VTTBR, so the kernel has to switch to the VMID of the
 * target VSpace before flushing.
 *
 * Only detects failure on hardware, because QEMU flushes all VMIDs.
 *
 * Also enabled for all other Arm configs to catch regressions where the wrong
 * ASID is invalidated.
 */

#include <sel4/sel4.h>
#include <vka/object.h>
#include <sel4utils/mapping.h>
#include <sel4utils/process.h>

#include "../helpers.h"

#ifdef CONFIG_ARCH_ARM

#define TEST_VADDR 0x10000000
#define MARKER     0xcafe

/* Runs in its own VSpace: write to vaddr so the translation is cached in the
 * TLB under the ASID/VMID of this VSpace, wait for the test to replace the
 * mapping, then read vaddr back. */
static int touch_wait_read(seL4_Word vaddr, seL4_Word ep, seL4_Word arg2, seL4_Word arg3)
{
    volatile seL4_Word *p = (volatile seL4_Word *)vaddr;
    *p = MARKER;
    seL4_Call(ep, seL4_MessageInfo_new(0, 0, 0, 0));
    return *p;
}

static int test_tlb_flush_other_vspace(env_t env)
{
    helper_thread_t t;
    cspacepath_t path;
    seL4_Word badge;
    int error;

    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr reply = vka_alloc_reply_leaky(&env->vka);
    seL4_CPtr frame1 = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    seL4_CPtr frame2 = vka_alloc_frame_leaky(&env->vka, seL4_PageBits);
    test_assert(ep != 0 && frame1 != 0 && frame2 != 0);

    create_helper_process(env, &t);
    vka_cspace_make_path(&env->vka, ep, &path);
    seL4_CPtr remote_ep = sel4utils_copy_path_to_process(&t.process, path);
    test_assert(remote_ep != -1);

    error = sel4utils_map_page_leaky(&env->vka, t.process.pd.cptr, frame1, (void *)TEST_VADDR,
                                     seL4_AllRights, 1);
    test_eq(error, 0);

    /* The helper writes to frame1 and then blocks on ep. */
    start_helper(env, &t, (helper_fn_t)touch_wait_read, TEST_VADDR, remote_ep, 0, 0);
    api_recv(ep, &badge, reply);

    /* Replace the mapping while the VSpace of the helper is not the current VSpace.
     * The kernel must flush the TLB entry under the ASID/VMID of the helper. */
    error = seL4_ARCH_Page_Map(frame2, t.process.pd.cptr, TEST_VADDR, seL4_AllRights,
                               seL4_ARCH_Default_VMAttributes);
    test_eq(error, seL4_NoError);

    /* frame2 is freshly retyped and therefore zero. A stale TLB entry will
     * still translate to frame1 and return MARKER. */
    api_reply(reply, seL4_MessageInfo_new(0, 0, 0, 0));
    test_eq(wait_for_helper(&t), 0);

    cleanup_helper(env, &t);
    return sel4test_get_result();
}
DEFINE_TEST(TLB0001, "Test TLB flush of a non-current VSpace",
            test_tlb_flush_other_vspace, config_set(CONFIG_ARCH_ARM))

#endif /* CONFIG_ARCH_ARM */
