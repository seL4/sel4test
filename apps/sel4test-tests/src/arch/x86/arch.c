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

#include <sel4platsupport/io.h>
#include <sel4platsupport/timer.h>
#include <platsupport/plat/timer.h>
#include <platsupport/plat/serial.h>
#include <sel4platsupport/arch/io.h>
#include <sel4utils/sel4_zf_logif.h>

static seL4_CPtr
get_IOPort_cap(void *data, uint16_t start_port, uint16_t end_port)
{
    test_init_data_t *init = (test_init_data_t *) data;

    if (start_port >= SERIAL_CONSOLE_COM1_PORT &&
           start_port <= SERIAL_CONSOLE_COM1_PORT_END) {
        return init->serial_io_port_cap;
    }

    return seL4_CapNull;
}

static seL4_Error
get_msi(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
        UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
        UNUSED seL4_Word handle, seL4_Word vector)
{
    return 0;
}

static seL4_Error
get_ioapic(void *data, seL4_CNode root, seL4_Word index, uint8_t depth, seL4_Word ioapic,
           seL4_Word pin, seL4_Word level, seL4_Word polarity, seL4_Word vector)
{
    return 0;
}

void arch_init_simple(simple_t *simple) {
    simple->arch_simple.IOPort_cap = get_IOPort_cap;
    simple->arch_simple.msi = get_msi;
    simple->arch_simple.ioapic = get_ioapic;
}

void
arch_init_allocator(env_t env, test_init_data_t *data)
{
}
