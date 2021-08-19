/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>
#include <sel4/types.h>
#include <sel4/sel4.h>

/* This list must be ordered by size - highest first */
static const frame_type_t frame_types[] = {
    { seL4_X86_LargePageObject, 0, seL4_LargePageBits, },
    { seL4_X86_4K, BIT(seL4_LargePageBits), seL4_PageBits, },
};

