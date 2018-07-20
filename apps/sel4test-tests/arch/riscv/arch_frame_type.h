/*
 * Copyright 2018, Data61
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

/* This list must be ordered by size - highest first */
static const frame_type_t frame_types[] = {
    /* Rocket-Chip for zedboard only has 256MiB of RAM, so we can't allocate a 1GiB page */
#if __riscv_xlen == 64 && !defined(CONFIG_BUILD_ROCKET_CHIP_ZEDBOARD)
    { seL4_RISCV_Giga_Page, 0, seL4_HugePageBits, },
#endif
    { seL4_RISCV_Mega_Page, 0, seL4_LargePageBits, },
    { seL4_RISCV_4K_Page, BIT(seL4_LargePageBits), seL4_PageBits, },
};
