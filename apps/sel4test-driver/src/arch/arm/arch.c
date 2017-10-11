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

#include "../../test.h"

#include <sel4platsupport/device.h>
#include <sel4platsupport/plat/serial.h>
#include <vka/capops.h>
#include <sel4utils/process.h>

void
arch_copy_serial_caps(test_init_data_t *init, driver_env_t env, sel4utils_process_t *test_process)
{
    init->serial_paddr = env->serial_objects.arch_serial_objects.serial_frame_paddr;
    init->serial_frame_cap = sel4utils_copy_cap_to_process(test_process, &env->vka, env->serial_objects.arch_serial_objects.serial_frame_obj.cptr);
    ZF_LOGF_IF(init->serial_frame_cap == 0,
               "Failed to copy PS default serial Frame cap to sel4test-test process");
}

/* global used for storing the environment as we are going to be overriding one function
 * of a vka interface, but we do not bother to create an entire interface wrapper with
 * a new 'data' cookie */
static driver_env_t alloc_at_env;
int serial_utspace_alloc_at_fn(void *data, const cspacepath_t *dest, seL4_Word type, seL4_Word size_bits,
                               uintptr_t paddr, seL4_Word *cookie)
{
    ZF_LOGF_IF(!alloc_at_env, "%s called before arch_get_serial_utspace_alloc_at", __FUNCTION__);
    if (paddr == alloc_at_env->serial_objects.arch_serial_objects.serial_frame_paddr) {
        cspacepath_t tmp_frame_path;

        vka_cspace_make_path(&alloc_at_env->vka,
                             alloc_at_env->serial_objects.arch_serial_objects.serial_frame_obj.cptr,
                             &tmp_frame_path);
        return vka_cnode_copy(dest, &tmp_frame_path, seL4_AllRights);
    }

    assert(alloc_at_env->vka.utspace_alloc_at);
    return alloc_at_env->vka.utspace_alloc_at(data, dest, type, size_bits, paddr, cookie);
}

vka_utspace_alloc_at_fn arch_get_serial_utspace_alloc_at(driver_env_t _env)
{
    static bool call_once = false;
    if (call_once) {
        ZF_LOGF("This function can only be called once.");
    }
    call_once = true;
    alloc_at_env = _env;
    return serial_utspace_alloc_at_fn;
}

#ifdef CONFIG_ARM_SMMU
seL4_SlotRegion
arch_copy_iospace_caps_to_process(sel4utils_process_t *process, driver_env_t env)
{
    seL4_SlotRegion ret = {0, 0};
    int num_iospace_caps = 0;
    seL4_Error UNUSED error = simple_get_iospace_cap_count(&env->simple, &num_iospace_caps);
    assert(error == seL4_NoError);
    for (int i = 0; i < num_iospace_caps; i++) {
        seL4_CPtr iospace = simple_get_nth_iospace_cap(&env->simple, i);
        assert(iospace != seL4_CapNull);
        seL4_CPtr slot = sel4utils_copy_cap_to_process(process, &env->vka, iospace);
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
