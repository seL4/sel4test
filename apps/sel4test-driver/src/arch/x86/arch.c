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

void
arch_copy_serial_caps(test_init_data_t *init, driver_env_t env, sel4utils_process_t *test_process)
{
    init->serial_irq_cap = sel4utils_copy_cap_to_process(test_process, &env->vka, env->serial_objects.serial_irq_path.capPtr);
    init->serial_io_port_cap = sel4utils_copy_cap_to_process(test_process, &env->vka, env->serial_objects.arch_serial_objects.serial_io_port_cap);
}

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

/* global used for storing the environment as we are going to be overriding one function
 * of a simpleinterface, but we do not bother to create an entire interface wrapper with
 * a new 'data' cookie */
static driver_env_t alloc_at_env;
static seL4_Error serial_ioport_cap_fn(void *data, uint16_t start_port, uint16_t end_port, seL4_Word root, seL4_Word dest, seL4_Word depth)
{
    ZF_LOGF_IF(!alloc_at_env, "%s called before arch_get_serial_utspace_alloc_at", __FUNCTION__);
    if (start_port >= SERIAL_CONSOLE_COM1_PORT &&
           start_port <= SERIAL_CONSOLE_COM1_PORT_END) {
        return seL4_CNode_Copy(root, dest, depth, simple_get_cnode(&alloc_at_env->simple),
            alloc_at_env->serial_objects.arch_serial_objects.serial_io_port_cap, CONFIG_WORD_SIZE, seL4_AllRights);
    }

    assert(alloc_at_env->simple.arch_simple.IOPort_cap);
    return alloc_at_env->simple.arch_simple.IOPort_cap(data, start_port, end_port, root, dest, depth);
}

arch_simple_get_IOPort_cap_fn arch_get_serial_ioport_cap(driver_env_t _env)
{
    static bool call_once = false;
    if (call_once) {
        ZF_LOGF("This function can only be called once.");
    }
    call_once = true;
    alloc_at_env = _env;
    return serial_ioport_cap_fn;
}
