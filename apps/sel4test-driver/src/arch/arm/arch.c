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

/* FIXME: This file is symlinked to RISC-V from ARM because they use the same
 * implementation. This was done because there is a plan to remove this functionality
 * in favour of a vka RPC client that allows the test process to query for hardware
 * resources instead of preallocating them.
 */

#include "../../test.h"

#include <sel4platsupport/device.h>
#include <platsupport/plat/serial.h>
#include <vka/capops.h>
#include <sel4utils/process.h>

#ifdef CONFIG_TK1_SMMU
seL4_SlotRegion arch_copy_iospace_caps_to_process(sel4utils_process_t *process, driver_env_t env)
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
