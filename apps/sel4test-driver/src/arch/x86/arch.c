/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>
#include "../../test.h"
#include <simple/simple.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4platsupport/plat/serial.h>
#include <sel4platsupport/device.h>
#include <platsupport/plat/hpet.h>
#include <vka/capops.h>

void
arch_init_timer_caps(env_t env)
{
    int error;

    /* map in the timer paddr so that we can query the HPET properties. Although
     * we only support the possiblity of non FSB delivery if we are using the IOAPIC */
    cspacepath_t frame;
    error = vka_cspace_alloc_path(&env->vka, &frame);
    ZF_LOGF_IF(error, "Failed to allocate cslot");
    error = vka_untyped_retype(&env->timer_dev_ut_obj, seL4_X86_4K, seL4_PageBits, 1, &frame);
    ZF_LOGF_IF(error, "Failed to retype timer frame");
    void *vaddr;
    vaddr = vspace_map_pages(&env->vspace, &frame.capPtr, NULL, seL4_AllRights, 1, seL4_PageBits, 0);
    ZF_LOGF_IF(vaddr == NULL, "Failed to map HPET paddr");
    if (!hpet_supports_fsb_delivery(vaddr)) {
        if (!config_set(CONFIG_IRQ_IOAPIC)) {
            ZF_LOGF("HPET does not support FSB delivery and we are not using the IOAPIC");
        }
        uint32_t irq_mask = hpet_ioapic_irq_delivery_mask(vaddr);
        /* grab the first irq */
        int irq = FFS(irq_mask) - 1;
        error = arch_simple_get_ioapic(&env->simple.arch_simple, env->timer_irq_path, 0, irq, 0, 1, DEFAULT_TIMER_INTERRUPT);
    } else {
        error = arch_simple_get_msi(&env->simple.arch_simple, env->timer_irq_path, 0, 0, 0, 0, DEFAULT_TIMER_INTERRUPT);
    }
    ZF_LOGF_IF(error, "Failed to obtain IRQ cap for PS default timer.");
    vspace_unmap_pages(&env->vspace, vaddr, 1, seL4_PageBits, VSPACE_PRESERVE);
    vka_cnode_delete(&frame);
    vka_cspace_free(&env->vka, frame.capPtr);
}

int
arch_init_serial_caps(env_t env)
{
    int error;

    /* Fill in the IO port caps for each of the serial devices.
     *
     * There's no need to allocate cspace slots because all of these just
     * actually boil down into calls to simple_default_get_IOPort_cap(), which
     * simply returns seL4_CapIOPort.
     *
     * But if in the future, somebody refines libsel4simple-default to actually
     * retype and sub-partition caps, we want to be doing the correct thing.
     */
    error = vka_cspace_alloc(&env->vka, &env->serial_io_port_cap);
    if (error != 0) {
        ZF_LOGE("Failed to alloc slot for COM1 port cap.");
        return error;
    }

    /* Also allocate the serial IRQ. This is placed in the arch-specific code
     * because x86 also allows the EGA device to be defined as a
     * PS_SERIAL_DEFAULT device, so since it does not have an IRQ, we have to
     * do arch-specific processing for that case.
     */
    if (PS_SERIAL_DEFAULT != PC99_TEXT_EGA) {
        /* The slot was allocated earlier, outside in init_serial_caps. */
        error = simple_get_IRQ_handler(&env->simple,
                                       DEFAULT_SERIAL_INTERRUPT,
                                       env->serial_irq_path);
        if (error != 0) {
            ZF_LOGE("Failed to get IRQ cap for default COM device. IRQ is %d.",
                    DEFAULT_SERIAL_INTERRUPT);
            return error;
        }
    } else {
        cspacepath_t tmp = { 0 };
        env->serial_irq_path = tmp;
    }


    env->serial_io_port_cap = simple_get_IOPort_cap(&env->simple,
                                                     SERIAL_CONSOLE_COM1_PORT,
                                                     SERIAL_CONSOLE_COM1_PORT_END);
    if (env->serial_io_port_cap == 0) {
        ZF_LOGE("Failed to get COM1 port cap.");
        ZF_LOGW_IF(PS_SERIAL_DEFAULT == PS_SERIAL0, "COM1 is the default serial.");
        return -1;
    }

    return 0;
}

/* copy the caps required to set up the sel4platsupport default timer */
void
arch_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
}

void
arch_copy_serial_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    init->serial_irq_cap = copy_cap_to_process(test_process, env->serial_irq_path.capPtr);
    init->serial_io_port_cap = copy_cap_to_process(test_process, env->serial_io_port_cap);
}

int serial_utspace_alloc_at_fn(void *data, const cspacepath_t *dest, seL4_Word type, seL4_Word size_bits,
        uintptr_t paddr, seL4_Word *cookie)
{
    ZF_LOGF("Serial on x86 doesn't use utspace");
    return -1;
}

vka_utspace_alloc_at_fn arch_get_serial_utspace_alloc_at(env_t _env) {
    static bool call_once = false;
    if (call_once) {
        ZF_LOGF("This function can only be called once.");
    }
    call_once = true;
    return serial_utspace_alloc_at_fn;
}
