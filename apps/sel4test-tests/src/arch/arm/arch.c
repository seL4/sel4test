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

static seL4_Error
get_frame_cap(void *data, void *paddr, int size_bits, cspacepath_t *path)
{
    test_init_data_t *init = (test_init_data_t *) data;
    assert(paddr == (void*) DEFAULT_TIMER_PADDR);
    assert(size_bits == seL4_PageBits);


    int error = seL4_CNode_Copy(path->root, path->capPtr, path->capDepth, init->root_cnode,
                                init->timer_frame, seL4_WordBits, seL4_AllRights);
    assert(error == seL4_NoError);

    return error;
}

void
arch_init_simple(simple_t *simple) 
{
    simple->frame_cap = get_frame_cap;
}

