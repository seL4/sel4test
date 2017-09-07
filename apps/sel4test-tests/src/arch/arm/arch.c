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

#include "../../init.h"
#include <sel4platsupport/plat/serial.h>
#include <sel4platsupport/io.h>
#include <sel4platsupport/timer.h>
#include <vka/capops.h>

static cspacepath_t serial_frame_path = {0};
static vka_t old_vka;

static int
serial_utspace_alloc_at_fn(void *data, const cspacepath_t *dest, seL4_Word type, seL4_Word size_bits,
        uintptr_t paddr, seL4_Word *cookie)
{
    /* We already asked the allocator for the serial and timer device-untypeds
     * in the test-driver process, so since we can't ask for them again, we
     * just return the addresses that the test-driver passed to us, if we
     * are asked for those specific physical addresses.
     *
     * This function is then used to wrap around all requests to
     * vka_utspace_alloc_at(), from the tests as they run.
     */
    if (paddr == DEFAULT_SERIAL_PADDR) {
        return vka_cnode_copy(dest, &serial_frame_path, seL4_AllRights);
    }

    return old_vka.utspace_alloc_at(data, dest, type, size_bits, paddr, cookie);
}

void
arch_init_allocator(env_t env, test_init_data_t *data)
{
    /* Create cspacepaths to the device-untypeds we got from the test-driver
     * process, and then set up the proxy vka_utspace_alloc_at()
     * wrapper function.
     */
    vka_cspace_make_path(&env->vka, data->serial_frame_cap, &serial_frame_path);

    old_vka = env->vka;
    env->vka.utspace_alloc_at = serial_utspace_alloc_at_fn;
}

void
arch_init_simple(simple_t *simple) {
    /* nothing to do */
}
