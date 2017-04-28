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

#include <sel4platsupport/device.h>
#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4platsupport/plat/serial.h>

void
arch_init_timer_caps(env_t env)
{
    int error;

    /* Obtain the IRQ cap for the PS default timer IRQ
     * The slot was allocated earlier, outside in init_timer_caps.
     *
     * The IRQ cap setup is arch specific because x86 uses MSI, and that's
     * a different function.
     */
    error = sel4platsupport_copy_irq_cap(&env->vka, &env->simple, DEFAULT_TIMER_INTERRUPT,
                                             &env->timer_irq_path);
    ZF_LOGF_IF(error, "Failed to obtain PS default timer IRQ cap.");

    plat_init_timer_caps(env);
}

void
arch_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    plat_copy_timer_caps(init, env, test_process);
}

int
arch_init_serial_caps(env_t env)
{
    int error;

    /* Obtain IRQ cap for PS default serial. */
    error = sel4platsupport_copy_irq_cap(&env->vka, &env->simple, DEFAULT_SERIAL_INTERRUPT,
                                           &env->serial_irq_path);
    ZF_LOGF_IF(error, "Failed to obtain PS default serial IRQ cap.");

    /* Obtain frame cap for PS default serial.
     * We pass the serial's MMIO frame as a frame object, and not an untyped,
     * like the way we pass the timer MMIO paddr. The reason for this is that
     * the child tests use the serial device themselves, and we can't retype
     * an untyped twice. But we can make copies of a Frame cap.
     */
    env->serial_frame_paddr = DEFAULT_SERIAL_PADDR;
    error = vka_alloc_frame_at(&env->vka, seL4_PageBits, DEFAULT_SERIAL_PADDR,
                               &env->serial_frame_obj);
    ZF_LOGF_IF(error, "Failed to obtain frame cap for default serial.");

    return plat_init_serial_caps(env);
}

void
arch_copy_serial_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    init->serial_paddr = env->serial_frame_paddr;
    init->serial_frame_cap = copy_cap_to_process(test_process, env->serial_frame_obj.cptr);
    ZF_LOGF_IF(init->serial_frame_cap == 0,
               "Failed to copy PS default serial Frame cap to sel4test-test process");

    plat_copy_serial_caps(init, env, test_process);
}

env_t env;
int serial_utspace_alloc_at_fn(void *data, const cspacepath_t *dest, seL4_Word type, seL4_Word size_bits,
        uintptr_t paddr, seL4_Word *cookie)
{
    if (paddr == env->serial_objects.arch_serial_objects.serial_frame_paddr) {
        cspacepath_t tmp_frame_path;

        vka_cspace_make_path(&env->vka,
                             env->serial_objects.arch_serial_objects.serial_frame_obj.cptr,
                             &tmp_frame_path);
        return vka_cnode_copy(dest, &tmp_frame_path, seL4_AllRights);
    }

    assert(env->vka.utspace_alloc_at);
    return env->vka.utspace_alloc_at(data, dest, type, size_bits, paddr, cookie);
}

vka_utspace_alloc_at_fn arch_get_serial_utspace_alloc_at(env_t _env) {
    static bool call_once = false;
    if (call_once) {
        ZF_LOGF("This function can only be called once.");
    }
    call_once = true;
    env = _env;
    return serial_utspace_alloc_at_fn;
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
