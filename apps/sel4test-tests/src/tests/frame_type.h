/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>
#include <sel4/types.h>
#include <sel4/sel4.h>

typedef struct frame_type {
    seL4_Word type;
    seL4_Word vaddr_offset;
    seL4_Word size_bits;
} frame_type_t;

#include <arch_frame_type.h>

/* define a couple of constants to aid creating virtual reservations for
 * mapping in all the frame types. This region needs to be big enough to
 * hold one mapping of every frame (this can be simplified to being twice
 * the size of the largest frame) and aligned to the largest frame size */
#define VSPACE_RV_ALIGN_BITS (frame_types[0].size_bits)
#define VSPACE_RV_SIZE (2 * BIT(VSPACE_RV_ALIGN_BITS))

