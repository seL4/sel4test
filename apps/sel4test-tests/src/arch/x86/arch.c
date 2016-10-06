/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#include "../../init.h"

#include <platsupport/plat/serial.h>
#include <sel4platsupport/plat/hpet.h>

static seL4_CPtr
get_IOPort_cap(void *data, uint16_t start_port, uint16_t end_port)
{
    test_init_data_t *init = (test_init_data_t *) data;

    assert(start_port >= SERIAL_CONSOLE_COM1_PORT &&
           start_port <= SERIAL_CONSOLE_COM1_PORT_END);

    return init->serial_io_port1;
}

static seL4_Error
get_msi(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
        UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
        UNUSED seL4_Word handle, seL4_Word vector)
{
    assert(vector == IRQ_OFFSET);
    test_init_data_t *init = (test_init_data_t *) data;
    int error = seL4_CNode_Move(root, index, depth, init->root_cnode,
            init->timer_irq, seL4_WordBits);
    assert(error == seL4_NoError);
    return seL4_NoError;
}

void
arch_init_simple(simple_t *simple) {
    simple->arch_simple.IOPort_cap = get_IOPort_cap;
    simple->arch_simple.msi = get_msi;
}

seL4_timer_t *
arch_init_timer(env_t env, test_init_data_t *data)
{
   return sel4platsupport_get_hpet_paddr(&env->vspace, &env->simple, &env->vka,
                                         data->timer_paddr, env->timer_notification.cptr,
                                         DEFAULT_TIMER_INTERRUPT);
}

void
arch_init_allocator(allocman_t *alloc, vka_t *vka, test_init_data_t *data)
{
    /* nothing to do */
}
