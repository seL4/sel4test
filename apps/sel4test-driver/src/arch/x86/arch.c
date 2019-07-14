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
#include "../../test.h"
#include <simple/simple.h>
#include <platsupport/serial.h>
#include <sel4platsupport/device.h>
#include <platsupport/plat/hpet.h>
#include <vka/capops.h>

int serial_utspace_alloc_at_fn(void *data, const cspacepath_t *dest, seL4_Word type, seL4_Word size_bits,
                               uintptr_t paddr, seL4_Word *cookie)
{
    ZF_LOGF("Serial on x86 doesn't use utspace");
    return -1;
}

vka_utspace_alloc_at_fn arch_get_serial_utspace_alloc_at(driver_env_t _env)
{
    static bool call_once = false;
    if (call_once) {
        ZF_LOGF("This function can only be called once.");
    }
    call_once = true;
    return serial_utspace_alloc_at_fn;
}
