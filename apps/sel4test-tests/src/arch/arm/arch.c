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
#include <vka/capops.h>

static cspacepath_t serial_path = {0};
static vka_t old_vka;

static int
serial_utspace_alloc_at_fn(void *data, const cspacepath_t *dest, seL4_Word type, seL4_Word size_bits,
        uintptr_t paddr, seL4_Word *cookie)
{
    if (paddr == DEFAULT_SERIAL_PADDR) {
        return vka_cnode_copy(dest, &serial_path, seL4_AllRights);
    }
    return old_vka.utspace_alloc_at(data, dest, type, size_bits, paddr, cookie);
}

void
arch_init_allocator(env_t env, test_init_data_t *data)
{
    vka_cspace_make_path(&env->vka, data->serial_frame, &serial_path);
    old_vka = env->vka;
    env->vka.utspace_alloc_at = serial_utspace_alloc_at_fn;
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
