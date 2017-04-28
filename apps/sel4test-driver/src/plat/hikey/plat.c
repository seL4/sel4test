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
