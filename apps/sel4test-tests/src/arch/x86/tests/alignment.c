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
#ifdef HAVE_AUTOCONF
#include <autoconf.h>
#endif

#include <sel4/sel4.h>
#include <utils/attribute.h>

#include "../../../helpers.h"
#include "../../../test.h"

#include <autoconf.h>

/* Implemented in assembly */
void align_test_asm(void);

static inline int
test_stack_alignment(struct env *env)
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
