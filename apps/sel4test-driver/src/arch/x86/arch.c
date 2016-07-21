/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#include "../../test.h"
#include <simple/simple.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4platsupport/plat/serial.h>

void
arch_init_timer_caps(env_t env)
{
    env->io_port_cap = simple_get_IOPort_cap(&env->simple, PIT_IO_PORT_MIN, PIT_IO_PORT_MAX);
    if (env->io_port_cap == 0) {
        ZF_LOGF("Failed to get IO port cap for range %x to %x.", PIT_IO_PORT_MIN, PIT_IO_PORT_MAX);
    }
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
    error = vka_cspace_alloc(&env->vka, &env->serial_io_port_cap1);
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
        error = vka_cspace_alloc_path(&env->vka, &env->serial_irq_path);
        if (error != 0) {
            ZF_LOGE("Failed to alloc slot for default COM IRQ.");
            return error;
        }
        error = simple_get_IRQ_control(&env->simple,
                                       DEFAULT_SERIAL_INTERRUPT,
                                       env->serial_irq_path);
        if (error != 0) {
            ZF_LOGE("Failed to get IRQ cap for default COM device.");
            return error;
        }
    } else {
        cspacepath_t tmp = { 0 };
        env->serial_irq_path = tmp;
    }


    env->serial_io_port_cap1 = simple_get_IOPort_cap(&env->simple,
                                                     SERIAL_CONSOLE_COM1_PORT,
                                                     SERIAL_CONSOLE_COM1_PORT_END);
    if (env->serial_io_port_cap1 == 0) {
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
    ZF_LOGV("Copying timer irq\n");
    init->timer_irq = copy_cap_to_process(test_process, env->irq_path.capPtr);

    ZF_LOGV("Copying timer frame cap\n");
    init->timer_frame = copy_cap_to_process(test_process, env->frame_path.capPtr);
}

void
arch_copy_serial_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    init->serial_irq = copy_cap_to_process(test_process, env->serial_irq_path.capPtr);
    init->serial_io_port1 = copy_cap_to_process(test_process, env->serial_io_port_cap1);
}
