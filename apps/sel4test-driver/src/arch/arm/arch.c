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
arch_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{

    /* Timer frame cap (only for arm). Here we assume the sel4platsupport
     * default timer only requires one frame. */
    ZF_LOGV("Copying timer frame cap\n");
    init->timer_frame = copy_cap_to_process(test_process, env->frame_path.capPtr);
    if (init->timer_frame == 0) {
        ZF_LOGF("Failed to copy timer frame cap to process");
    }

    ZF_LOGV("Copying timer irq\n");
    init->timer_irq = copy_cap_to_process(test_process, env->irq_path.capPtr);
    assert(init->timer_irq != 0);

    plat_copy_timer_caps(init, env, test_process);
}

