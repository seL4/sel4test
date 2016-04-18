/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef SEL4TEST_TESTS_ARCH_FRAME_TYPE_H_
#define SEL4TEST_TESTS_ARCH_FRAME_TYPE_H_

#include <autoconf.h>
#include <stdint.h>
#include <sel4/types.h>
#include <sel4/sel4.h>

/* This list must be ordered by size - highest first */
static const frame_type_t frame_types[] = {
    { seL4_ARM_SuperSectionObject, 0, 24, },
    { seL4_ARM_SectionObject, BIT(24), 20, },
    { seL4_ARM_LargePageObject, BIT(24) + BIT(20), 16, },
    { seL4_ARM_SmallPageObject, BIT(24) + BIT(20) + BIT(16), seL4_PageBits, },
};

#endif /* SEL4TEST_TESTS_ARCH_FRAME_TYPE_H_ */
