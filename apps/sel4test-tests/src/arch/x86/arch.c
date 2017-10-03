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
    } else {
        return init->timer_io_port_cap;
    }
}

static seL4_Error
get_msi(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
        UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
        UNUSED seL4_Word handle, seL4_Word vector)
{
    test_init_data_t *init = (test_init_data_t *) data;
    int error = seL4_CNode_Move(root, index, depth, init->root_cnode,
                                sel4platsupport_timer_objs_get_irq_cap(&init->to, vector, PS_MSI), seL4_WordBits);
    assert(error == seL4_NoError);
    return error;
}

static seL4_Error
get_ioapic(void *data, seL4_CNode root, seL4_Word index, uint8_t depth, seL4_Word ioapic,
           seL4_Word pin, seL4_Word level, seL4_Word polarity, seL4_Word vector)
{
    test_init_data_t *init = (test_init_data_t *) data;
    int error = seL4_CNode_Move(root, index, depth, init->root_cnode,
            sel4platsupport_timer_objs_get_irq_cap(&init->to, pin, PS_IOAPIC), seL4_WordBits);
    assert(error == seL4_NoError);
    return error;
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

void
arch_init_timer(env_t env, test_init_data_t *data)
{
    ps_io_ops_t ops = {};
    int error = sel4platsupport_new_io_ops(env->vspace, env->vka, &ops);
    ZF_LOGF_IF(error, "Failed to get io ops");

    error = sel4platsupport_get_io_port_ops(&ops.io_port_ops, &env->simple);
    ZF_LOGF_IF(error, "Failed to get io port ops");

    /* set up the irqs */
    error = sel4platsupport_init_timer_irqs(&env->vka, &env->simple,
            env->timer_notification.cptr, &env->timer, &data->to);
    ZF_LOGF_IF(error, "Failed to init timer");

    if (!error) {
        /* if this succeeds, sel4test-driver has set up the hpet for us */
        ps_irq_t irq;
        error = ltimer_hpet_describe_with_region(&env->timer.ltimer, ops, data->to.objs[0].region, &irq);
        if (!error) {
            ZF_LOGD("Trying HPET");
            error = ltimer_hpet_init(&env->timer.ltimer, ops, irq, data->to.objs[0].region);
        }
    }

    if (error) {
        /* Get the PIT instead */
        ZF_LOGD("Using PIT timer");
        error = ltimer_pit_init_freq(&env->timer.ltimer, ops, data->tsc_freq);
        ZF_LOGF_IF(error, "Failed to init pit");
    }
}
