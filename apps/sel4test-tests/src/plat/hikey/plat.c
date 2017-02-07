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

void
plat_init_env(env_t env, test_init_data_t *data)
{
    env->clock_timer = sel4platsupport_hikey_get_vupcounter_timer(
                                                 &env->vka, &env->vspace,
                                                 &env->simple,
                                                 seL4_CapNull);
    ZF_LOGF_IF(env->clock_timer == NULL, "Failed to initialize vupcounter with "
               "seL4platsupport_get_timer.");
}

seL4_Error
plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    test_init_data_t *init = (test_init_data_t *) data;

    switch (irq) {
    case RTC0_INTERRUPT:
        return seL4_CNode_Copy(root, index, depth,
                               init->root_cnode,
                               init->clock_timer_irq_cap,
                               seL4_WordBits, seL4_AllRights);
    case DMTIMER2_INTERRUPT:
        return seL4_CNode_Copy(root, index, depth,
                               init->root_cnode,
                               init->extra_timer_irq_cap,
                               seL4_WordBits, seL4_AllRights);
    default:
        return -1;
    }
}
