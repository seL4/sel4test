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
#include "../../helpers.h"

#include <platsupport/plat/timer.h>
#include <sel4platsupport/io.h>
#include <sel4platsupport/plat/timer.h>

void
plat_add_uts(env_t env, allocman_t *allocator, test_init_data_t *data)
{
    int error;
    cspacepath_t path;
    size_t size_bits = seL4_PageBits;

    /* Add the RTC0's device untyped to the allocman allocator, so that
     * when sel4platsupport_get_timer() asks for it, it'll get it.
     */
    vka_cspace_make_path(&env->vka, data->clock_timer_dev_ut_cap, &path);
    error = allocman_utspace_add_uts(allocator, 1, &path, &size_bits,
                                     &data->clock_timer_paddr, ALLOCMAN_UT_DEV);
    ZF_LOGF_IF(error, "Failed to add RTC0 wall-clock timer ut to allocator");

    /* Also add the DMTIMER2's device untyped for the same reason. */
    size_bits = seL4_PageBits;
    vka_cspace_make_path(&env->vka, data->extra_timer_dev_ut_cap, &path);
    error = allocman_utspace_add_uts(allocator, 1, &path, &size_bits,
                                     &data->extra_timer_paddr, ALLOCMAN_UT_DEV);
    ZF_LOGF_IF(error, "Failed to add DMTIMER2 wall-clock timer ut to allocator");
}

static timer_config_t vupcounter_config;
pstimer_t *ps_hikey_vupcounter;

static void
sel4test_hikey_vupcounter_destroy(seL4_timer_t *timer,
                                              vka_t *vka, vspace_t *vspace)
{
    int error;
    static ps_io_mapper_t hikey_io_mapper;

    error = sel4platsupport_new_io_mapper(*vspace, *vka, &hikey_io_mapper);
    ZF_LOGF_IF(error, "Failed to get ps_io_mapper to map hikey timer regs.");

    if (timer->timer) {
        timer_stop(timer->timer);
    }

    if (vupcounter_config.vupcounter_config.rtc_vaddr != NULL) {
        ps_io_unmap(&hikey_io_mapper,
                    vupcounter_config.vupcounter_config.rtc_vaddr,
                    BIT(seL4_PageBits));
    }

    if (vupcounter_config.vupcounter_config.dualtimer_vaddr != NULL) {
        ps_io_unmap(&hikey_io_mapper,
                    vupcounter_config.vupcounter_config.dualtimer_vaddr,
                    BIT(seL4_PageBits));
    }
}

static void
sel4test_hikey_vupcounter_handle_irq(UNUSED seL4_timer_t *timer,
                                     UNUSED uint32_t irq)
{
    /* Nothing to do */
}

seL4_timer_t seL4_hikey_vupcounter = {
    .timer = NULL,
    .frame = { 0 },
    .irq = 0,
    .vaddr = NULL,
    .data = &vupcounter_config,
    .handle_irq = sel4test_hikey_vupcounter_handle_irq,
    .destroy = sel4test_hikey_vupcounter_destroy
};

void
plat_init_env(env_t env, test_init_data_t *data)
{
    int error;
    static ps_io_mapper_t hikey_io_mapper;

    error = sel4platsupport_new_io_mapper(env->vspace, env->vka, &hikey_io_mapper);
    ZF_LOGF_IF(error, "Failed to get ps_io_mapper to map hikey timer regs.");

    /* Now initialize both the other timer devices, and then finally, initialize
     * the vupcounter device.
     *
     * We can't use seL4platsupport_get_timer() timer_common_init because it
     * calls timer_common_init() which tries to set up IRQ caps and
     * notifications, etc, but these drivers don't emit any IRQs.
     *
     * Start by mapping in the device registers for both of the underlying
     * timers.
     *
     * RTC0 first, then DMTIMER2.
     */
    vupcounter_config.rtc_id = RTC0;
    vupcounter_config.vupcounter_config.rtc_vaddr = ps_io_map(&hikey_io_mapper,
                                            data->clock_timer_paddr,
                                            BIT(seL4_PageBits), false, PS_MEM_NORMAL);
    ZF_LOGF_IF(vupcounter_config.vupcounter_config.rtc_vaddr == NULL,
               "Failed to ps_io_map memory for the RTC0 timer.");

    vupcounter_config.dualtimer_id = DMTIMER2;
    vupcounter_config.vupcounter_config.dualtimer_vaddr = ps_io_map(&hikey_io_mapper,
                                                  data->extra_timer_paddr,
                                                  BIT(seL4_PageBits),
                                                  false, PS_MEM_NORMAL);
    ZF_LOGF_IF(vupcounter_config.vupcounter_config.rtc_vaddr == NULL,
               "Failed to ps_io_map memory for the DMTIMER2 dualtimer.");

    /* Next, pass the mapped device registers' virtual addresses to platsupport
     * and ask for a handle to the virtual upcounter device.
     */
    ps_hikey_vupcounter = ps_get_timer(VIRTUAL_UPCOUNTER, &vupcounter_config);
    ZF_LOGF_IF(ps_hikey_vupcounter == NULL,
               "Failed to ps_get_timer for the VIRTUAL_UPCOUNTER device.");

    /* Penultimately, wrap the virtual upcounter within a seL4_timer_t. */
    seL4_hikey_vupcounter.timer = ps_hikey_vupcounter;

    /* And finally, tell seL4test that it should use this virtual upcounter as
     * its clock_timer.
     */
    env->clock_timer = &seL4_hikey_vupcounter;
}

seL4_Error
plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    /* clock timer not implemented for this platform */
    return -1;
}
