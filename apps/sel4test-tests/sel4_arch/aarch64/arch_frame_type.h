/*
 * Copyright 2016, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the GNU General Public License version 2. Note that NO WARRANTY is provided.
 * See "LICENSE_GPLv2.txt" for details.
 *
 * @TAG(D61_GPL)
 */

#ifndef SEL4TEST_TESTS_ARCH_FRAME_TYPE_H_
#define SEL4TEST_TESTS_ARCH_FRAME_TYPE_H_

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

#endif /* SEL4TEST_TESTS_ARCH_FRAME_TYPE_H_ */
