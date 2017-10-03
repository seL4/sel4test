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

#pragma once

#include <autoconf.h>
#include <stdint.h>
#include <sel4/types.h>
#include <sel4/sel4.h>

/* This list must be ordered by size - highest first
 * We do not include the huge page as none of our supported platforms have enough
 * memory to allocate a 1gb frame after kernel and user space is loaded */
static const frame_type_t frame_types[] = {
    { seL4_ARM_LargePageObject, 0, seL4_LargePageBits, },
    { seL4_ARM_SmallPageObject, BIT(seL4_LargePageBits), seL4_PageBits, },
};

