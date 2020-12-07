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
    { seL4_ARM_SuperSectionObject, 0, 25, },
    { seL4_ARM_SectionObject, BIT(25), 21, },
    { seL4_ARM_LargePageObject, BIT(25) + BIT(21), 16, },
    { seL4_ARM_SmallPageObject, BIT(25) + BIT(21) + BIT(16), seL4_PageBits, },
};

