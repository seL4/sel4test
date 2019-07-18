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
#include <autoconf.h>
#define STACK_OFFSET_BYTES $(16 - (CONFIG_WORD_SIZE / 8))

#ifdef CONFIG_ARCH_IA32
#define SP esp
#else
#define SP rsp
#endif

.global align_test_asm
align_test_asm:
    /* Align the stack to 16 bytes assuming:
     * - 16 byte stack alignment before this function was called
     * - the register-sized return address has been pushed by the
     *   call and nothing else
     */
  sub STACK_OFFSET_BYTES, %SP

  /* Push and pop the %xmm0 */
  sub $16, %SP

  movdqa %xmm0, (%SP)
  movdqa (%SP), %xmm0

  add $16, %SP

  /* Compensate for stack alignment */
  add STACK_OFFSET_BYTES, %SP

  ret
