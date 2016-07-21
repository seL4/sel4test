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
#include <sel4platsupport/plat/serial.h>

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

int
arch_init_serial_caps(env_t env)
{
    /* Get the serial frame cap */
    int error;

    error = vka_cspace_alloc_path(&env->vka, &env->serial_frame_path);
    if (error != 0) {
        ZF_LOGE("Failed to alloc slot for serial Frame cap.");
        return error;
    }
    error = simple_get_frame_cap(&env->simple, (void *) DEFAULT_SERIAL_PADDR,
                                 PAGE_BITS_4K, &env->serial_frame_path);
    if (error != 0) {
        ZF_LOGE("Failed to get Frame cap for serial device.");
        return error;
    }
    return 0;
}

void
arch_copy_serial_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    init->serial_frame = copy_cap_to_process(test_process, env->serial_frame_path.capPtr);
    if (init->serial_frame == 0) {
        ZF_LOGF("Failed to copy serial Frame cap to sel4test-test process.");
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
