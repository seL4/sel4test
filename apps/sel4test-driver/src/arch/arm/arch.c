/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "../../test.h"

#include <sel4platsupport/plat/timer.h>

void
arch_init_timer_caps(env_t env)
{
    /* get the timer frame cap */
    seL4_CPtr cap;
    int error = vka_cspace_alloc(&env->vka, &cap);
    if (error) {
        ZF_LOGF("Failed to allocate cslot for timer frame cap path");
    }

    vka_cspace_make_path(&env->vka, cap, &env->frame_path);
    error = simple_get_frame_cap(&env->simple, (void *) DEFAULT_TIMER_PADDR, PAGE_BITS_4K, &env->frame_path);
    if (error) {
        ZF_LOGF("Failed to get frame cap for %p\n", (void *) DEFAULT_TIMER_PADDR);
    }
}

void
arch_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{

    /* Timer frame cap (only for arm). Here we assume the sel4platsupport
     * default timer only requires one frame. */
    init->timer_frame = copy_cap_to_process(test_process, env->frame_path.capPtr);
    if (init->timer_frame == 0) {
        ZF_LOGF("Failed to copy timer frame cap to process");
    }
}

#ifdef CONFIG_ARM_SMMU
seL4_SlotRegion
arch_copy_iospace_caps_to_process(sel4utils_process_t *process, env_t env)
{
    seL4_SlotRegion ret = {0, 0};
    int num_iospace_caps = 0;
    seL4_Error UNUSED error = simple_get_iospace_cap_count(&env->simple, &num_iospace_caps);
    assert(error == seL4_NoError);
    for (int i = 0; i < num_iospace_caps; i++) {
        seL4_CPtr iospace = simple_get_nth_iospace_cap(&env->simple, i);
        assert(iospace != seL4_CapNull);
        seL4_CPtr slot = copy_cap_to_process(process, iospace);
        assert(slot != seL4_CapNull);
        if (i == 0) {
            ret.start = slot;
        }
        ret.end = slot;
    }
    assert((ret.end - ret.start) + 1 == num_iospace_caps);
    /* the return region is now inclusive */
    return ret;
}
#endif
