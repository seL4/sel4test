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

#include <platsupport/plat/serial.h>
#include <sel4platsupport/plat/hpet.h>
#include <sel4platsupport/device.h>
#include <platsupport/plat/hpet.h>

static seL4_CPtr
get_IOPort_cap(void *data, uint16_t start_port, uint16_t end_port)
{
    test_init_data_t *init = (test_init_data_t *) data;

    assert(start_port >= SERIAL_CONSOLE_COM1_PORT &&
           start_port <= SERIAL_CONSOLE_COM1_PORT_END);

    return init->serial_io_port_cap;
}

static seL4_Error
get_msi(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
        UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
        UNUSED seL4_Word handle, seL4_Word vector)
{
    test_init_data_t *init = (test_init_data_t *) data;
    int error = seL4_CNode_Move(root, index, depth, init->root_cnode,
            init->timer_irq_cap, seL4_WordBits);
    assert(error == seL4_NoError);
    return error;
}

static seL4_Error
get_ioapic(void *data, seL4_CNode root, seL4_Word index, uint8_t depth, seL4_Word ioapic,
           seL4_Word pin, seL4_Word level, seL4_Word polarity, seL4_Word vector)
{
    test_init_data_t *init = (test_init_data_t *) data;
    int error = seL4_CNode_Move(root, index, depth, init->root_cnode,
            init->timer_irq_cap, seL4_WordBits);
    assert(error == seL4_NoError);
    return error;
}

void
arch_init_simple(simple_t *simple) {
    simple->arch_simple.IOPort_cap = get_IOPort_cap;
    simple->arch_simple.msi = get_msi;
    simple->arch_simple.ioapic = get_ioapic;
}

seL4_timer_t *
arch_init_timer(env_t env, test_init_data_t *data)
{
    /* Map the HPET so we can query its proprties */
    vka_object_t frame;
    void *vaddr;
    vaddr = sel4platsupport_map_frame_at(&env->vka, &env->vspace, data->timer_paddr, seL4_PageBits, &frame);
    int irq;
    int vector;
    ZF_LOGF_IF(vaddr == NULL, "Failed to map HPET paddr");
    if (!hpet_supports_fsb_delivery(vaddr)) {
        if (!config_set(CONFIG_IRQ_IOAPIC)) {
            ZF_LOGF("HPET does not support FSB delivery and we are not using the IOAPIC");
        }
        uint32_t irq_mask = hpet_ioapic_irq_delivery_mask(vaddr);
        /* grab the first irq */
        irq = FFS(irq_mask) - 1;
    } else {
        irq = -1;
    }
    vector = DEFAULT_TIMER_INTERRUPT;
    vspace_unmap_pages(&env->vspace, vaddr, 1, seL4_PageBits, VSPACE_PRESERVE);
    vka_free_object(&env->vka, &frame);
    return sel4platsupport_get_hpet_paddr(&env->vspace, &env->simple, &env->vka,
                                         data->timer_paddr, env->timer_notification.cptr,
                                         irq, vector);
}

void
arch_init_allocator(env_t env, test_init_data_t *data)
{
    /* nothing to do */
}
