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
#include <sel4platsupport/plat/serial.h>

void
arch_init_allocator(allocman_t *alloc, vka_t *vka, test_init_data_t *data)
{

    /* add serial frame */
    size_t size_bits = seL4_PageBits;
    uintptr_t paddr = DEFAULT_SERIAL_PADDR;
    cspacepath_t path;
    vka_cspace_make_path(vka, data->serial_frame, &path);
    int error = allocman_utspace_add_uts(alloc, 1, &path,
                                         &size_bits, &paddr,
                                         ALLOCMAN_UT_DEV);
    ZF_LOGF_IF(error, "Failed to add serial ut to allocator");
}

seL4_timer_t *
arch_init_timer(env_t env, test_init_data_t *data)
{
   return sel4platsupport_get_default_timer(&env->vka, &env->vspace, &env->simple,
                                            env->timer_notification.cptr);
}

void
arch_init_simple(simple_t *simple) {
    /* nothing to do */
}
