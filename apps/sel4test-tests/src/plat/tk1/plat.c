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

#include "../../test.h"
#include "../../helpers.h"

#include <utils/zf_log.h>
#include <sel4platsupport/timer.h>

void
plat_add_uts(env_t env, allocman_t *alloc, test_init_data_t *data)
{
    /* clock timer not implemented for this platform */
}

void
plat_init_env(env_t env, test_init_data_t *data)
{
    /* On TK1, the NV_TMR can handle both wall-clock and event-timers.
     */
    ZF_LOGF_IF(env->timer == NULL, "Failed to init TK1 NV timer.");

    env->clock_timer = env->timer;
}

seL4_Error
plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    /* The whole purpose of the wall-clock timer is to be a free-running
     * upcounter: doesn't need an IRQ.
     */
    return -1;
}
