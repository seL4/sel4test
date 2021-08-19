/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <sel4/sel4.h>
#include <utils/attribute.h>

#include "../../../helpers.h"
#include "../../../test.h"

#include <autoconf.h>
#include <sel4test/gen_config.h>

/* Implemented in assembly */
void align_test_asm(void);

static inline int test_stack_alignment(struct env *env)
{
    /* Stack should be aligned to 16-bytes, especially when SSEx is enabled
     * and movaps/movdqa instructions are emitted by the compiler.
     * This test will fail if it's not 16-bytes aligned. If you came
     * here because this test fails, this means that seL4 libraries
     * and especially assembly code that sets up the stack failed to maintain
     * 16-bytes alignment, and it needs to be fixed.
     * Note: the compiler, by default, maintains 16-bytes alignment.
     */
    align_test_asm();

    return sel4test_get_result();
}
DEFINE_TEST(STACK_ALIGNMENT_001, "Testing x86 Stack Alignment", test_stack_alignment, true);
