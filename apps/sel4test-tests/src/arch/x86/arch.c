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

#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <platsupport/timer.h>

#include <platsupport/plat/pit.h>

static seL4_Error
get_frame_cap(void *data, void *paddr, int size_bits, cspacepath_t *path)
{
    return plat_get_frame_cap(data, paddr, size_bits, path);
}

void
arch_init_simple(simple_t *simple)
{
    simple->frame_cap = get_frame_cap;
}

