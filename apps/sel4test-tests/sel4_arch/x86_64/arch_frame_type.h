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

#ifdef CONFIG_HUGE_PAGE
#define HUGE_OFFSET BIT(seL4_HugePageBits)
#else
#define HUGE_OFFSET 0
#endif

/* This list must be ordered by size - highest first */
static const frame_type_t frame_types[] = {
#ifdef CONFIG_HUGE_PAGE
    { seL4_X64_HugePageObject, 0, seL4_HugePageBits, },
#endif
    { seL4_X86_LargePageObject, HUGE_OFFSET, seL4_LargePageBits, },
    { seL4_X86_4K, HUGE_OFFSET + BIT(seL4_LargePageBits), seL4_PageBits, },
};

