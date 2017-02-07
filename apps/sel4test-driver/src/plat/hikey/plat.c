/*
 *  Copyright 2016, Data61
 *  Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 *  ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(D61_BSD)
 */
#include "../../test.h"

#include <platsupport/io.h>
#include <platsupport/plat/clock.h>
#include <sel4platsupport/io.h>
#include <sel4platsupport/device.h>
#include <sel4platsupport/plat/timer.h>

void
plat_init_timer_caps(env_t env)
{
    int error;

    /* Get the hikey RTC0 timestamp device's device-ut. We don't need an IRQ cap
     * for it because we don't use the RTC's "alarm" IRQ feature.
     */
    env->clock_timer_paddr = RTC0_PADDR;
    error = vka_alloc_untyped_at(&env->vka, seL4_PageBits, env->clock_timer_paddr,
                                 &env->clock_timer_dev_ut_obj);
    ZF_LOGF_IF(error, "Failed to obtain device-ut cap for RTC0 wall-clock "
               "timer.");

    /* Also get another of the dual timers to use as the accompanying
     * downcounter for the virtual upcounter.
     */
    env->extra_timer_paddr = DMTIMER2_PADDR;
    error = vka_alloc_untyped_at(&env->vka, seL4_PageBits, env->extra_timer_paddr,
                                 &env->extra_timer_dev_ut_obj);
    ZF_LOGF_IF(error, "Failed to obtain device-ut cap for DMTIMER2 downcounter "
               "timer.");

    /* Alloc space for the IRQ caps for the IRQs of both timers. */
    error = vka_cspace_alloc_path(&env->vka, &env->clock_irq_path);
    ZF_LOGF_IF(error, "Failed to allocate RTC0 timer IRQ.");

    error = vka_cspace_alloc_path(&env->vka, &env->extra_timer_irq_path);
    ZF_LOGF_IF(error, "Failed to allocate DMTIMER2 timer IRQ.");

    /* Obtain caps to the IRQs. */
    error = sel4platsupport_copy_irq_cap(&env->vka, &env->simple,
                                         RTC0_INTERRUPT,
                                         &env->clock_irq_path);
    ZF_LOGF_IF(error, "Failed to obtain RTC0 timer IRQ cap.");

    error = sel4platsupport_copy_irq_cap(&env->vka, &env->simple,
                                         DMTIMER2_INTERRUPT,
                                         &env->extra_timer_irq_path);
    ZF_LOGF_IF(error, "Failed to obtain DMTIMER2 timer IRQ cap.");
}

void
plat_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    /* Copy the RTC0 device-ut over to the init shmem page. */
    init->clock_timer_paddr = env->clock_timer_paddr;
    init->clock_timer_dev_ut_cap = copy_cap_to_process(test_process,
                                                       env->clock_timer_dev_ut_obj.cptr);
    ZF_LOGF_IF(init->clock_timer_dev_ut_cap == 0,
               "Failed to copy RTC0 wall-clock timer's device-ut to test"
               " child.");

    /* Copy the DMTIMER2 device-ut over to the init shmem page. */
    init->extra_timer_paddr = env->extra_timer_paddr;
    init->extra_timer_dev_ut_cap = copy_cap_to_process(test_process,
                                                       env->extra_timer_dev_ut_obj.cptr);
    ZF_LOGF_IF(init->extra_timer_dev_ut_cap == 0,
               "Failed to copy DMTIMER2 downcounter timer's device-ut to test"
               " child.");

    /* Copy the RTC0 timer IRQ cap over to the init shmem page */
    init->clock_timer_irq_cap = copy_cap_to_process(test_process,
                                                    env->clock_irq_path.capPtr);
    ZF_LOGF_IF(init->clock_timer_irq_cap == 0,
               "Failed to copy RTC0 downcounter timer's IRQ cap to test"
               " child.");

    /* Copy the DMTIMER2 timer IRQ cap over to the init shmem page */
    init->extra_timer_irq_cap = copy_cap_to_process(test_process,
                                                       env->extra_timer_irq_path.capPtr);
    ZF_LOGF_IF(init->extra_timer_irq_cap == 0,
               "Failed to copy DMTIMER2 downcounter timer's IRQ cap to test"
               " child.");
}

int
plat_init_serial_caps(env_t env)
{
    return 0;
}

void
plat_copy_serial_caps(test_init_data_t *init, env_t env,
                       sel4utils_process_t *test_process)
{
}

void
plat_init(env_t env)
{
    /* clock timer not implemented for this platform */
}
