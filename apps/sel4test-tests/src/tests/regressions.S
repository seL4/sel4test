/*
 * Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
/* test_registers (below) runs in a context without a valid SP. At the end it
 * needs to call into a C function and needs a valid stack to do this. We
 * provide it a stack it can switch to here.
 */
.section .bss
.align  4
.space 4096
_safe_stack:
.section .text
#if defined(CONFIG_ARCH_AARCH32)

/* Trampoline for providing the thread a valid stack before entering
 * reply_to_parent. No blx required because reply_to_parent does not
 * return.
 */
reply_trampoline:
    ldr sp, =_safe_stack
    b reply_to_parent

.global test_registers
test_registers:
    /* Assume the PC and CPSR are correct because there's no good way of
     * testing them. Test each of the registers against the magic numbers our
     * parent set.
     */
    cmp sp, #13
    bne test_registers_fail
    cmp r0, #15
    bne test_registers_fail
    cmp r1, #1
    bne test_registers_fail
    cmp r2, #2
    bne test_registers_fail
    cmp r3, #3
    bne test_registers_fail
    cmp r4, #4
    bne test_registers_fail
    cmp r5, #5
    bne test_registers_fail
    cmp r6, #6
    bne test_registers_fail
    cmp r7, #7
    bne test_registers_fail
    cmp r8, #8
    bne test_registers_fail
    cmp r9, #9
    bne test_registers_fail
    cmp r10, #10
    bne test_registers_fail
    cmp r11, #11
    bne test_registers_fail
    cmp r12, #12
    bne test_registers_fail
    cmp r14, #14
    bne test_registers_fail

    /* Return success. Note that we don't bother saving registers or bl because
     * we're not planning to return here and we don't have a valid stack.
     */
    mov r0, #0
    b reply_trampoline

    /* Return failure. */
test_registers_fail:
    mov r0, #1
    b reply_trampoline

#elif defined(CONFIG_ARCH_AARCH64)
    /* Trampoline for providing the thread a valid stack before entering
     * reply_to_parent. No blx required because reply_to_parent does not
     * return.
     */
reply_trampoline:
    ldr x1, =_safe_stack
    mov sp, x1
    b reply_to_parent

.global test_registers
test_registers:
    cmp sp, #1
    bne test_registers_fail
    cmp x0, #2
    bne test_registers_fail
    cmp x1, #3
    bne test_registers_fail
    cmp x2, #4
    bne test_registers_fail
    cmp x3, #5
    bne test_registers_fail
    cmp x4, #6
    bne test_registers_fail
    cmp x5, #7
    bne test_registers_fail
    cmp x6, #8
    bne test_registers_fail
    cmp x7, #9
    bne test_registers_fail
    cmp x8, #10
    bne test_registers_fail
    cmp x9, #11
    bne test_registers_fail
    cmp x10, #12
    bne test_registers_fail
    cmp x11, #13
    bne test_registers_fail
    cmp x12, #14
    bne test_registers_fail
    cmp x13, #15
    bne test_registers_fail
    cmp x14, #16
    bne test_registers_fail
    cmp x15, #17
    bne test_registers_fail
    cmp x16, #18
    bne test_registers_fail
    cmp x17, #19
    bne test_registers_fail
    cmp x18, #20
    bne test_registers_fail
    cmp x19, #21
    bne test_registers_fail
    cmp x20, #22
    bne test_registers_fail
    cmp x21, #23
    bne test_registers_fail
    cmp x22, #24
    bne test_registers_fail
    cmp x23, #25
    bne test_registers_fail
    cmp x24, #26
    bne test_registers_fail
    cmp x25, #27
    bne test_registers_fail
    cmp x26, #28
    bne test_registers_fail
    cmp x27, #29
    bne test_registers_fail
    cmp x28, #30
    bne test_registers_fail
    cmp x29, #31
    bne test_registers_fail
    cmp x30, #32
    bne test_registers_fail

    /* Return success. Note that we don't bother saving registers or bl because
     * we're not planning to return here and we don't have a valid stack.
     */
    mov x0, #0
    b reply_trampoline

/* Return failure. */
test_registers_fail:
    mov x0, #1
    b reply_trampoline

#elif defined(CONFIG_ARCH_X86_64)
reply_trampoline:
    leaq  _safe_stack, %rsp
    movq  %rax, %rdi
    call  reply_to_parent
.global test_registers
test_registers:
    jb    rflags_ok
    jmp   test_registers_fail
    rflags_ok:
    cmpq  $0x0000000a, %rax
    jne   test_registers_fail
    movq  $2, %rax
    cmpq  $0x0000000b, %rbx
    jne   test_registers_fail
    movq  $3, %rax
    cmpq  $0x0000000c, %rcx
    jne   test_registers_fail
    movq  $4, %rax
    cmpq  $0x0000000d, %rdx
    jne   test_registers_fail
    movq  $5, %rax
    cmpq  $0x00000005, %rsi
    jne   test_registers_fail
    movq  $6, %rax
    cmpq  $0x00000002, %rdi
    jne   test_registers_fail
    movq  $7, %rax
    cmpq  $0x00000003, %rbp
    jne   test_registers_fail
    movq  $8, %rax
    cmpq  $0x00000004, %rsp
    jne   test_registers_fail
    movq  $9, %rax
    cmpq  $0x00000088, %r8
    jne   test_registers_fail
    movq  $100, %rax
    cmpq  $0x00000099, %r9
    jne   test_registers_fail
    movq  $11, %rax
    cmpq  $0x00000010, %r10
    jne   test_registers_fail
    movq  $12, %rax
    cmpq  $0x00000011, %r11
    jne   test_registers_fail
    movq  $13, %rax
    cmpq  $0x00000012, %r12
    jne   test_registers_fail
    movq  $14, %rax
    cmpq  $0x00000013, %r13
    jne   test_registers_fail
    movq  $15, %rax
    cmpq  $0x00000014, %r14
    jne   test_registers_fail
    movq  $16, %rax
    cmpq  $0x00000015, %r15
    jne   test_registers_fail
    movq  $0, %rax
    jmp   reply_trampoline
    test_registers_fail:
    movq  $1, %rax
    jmp   reply_trampoline

#elif defined(CONFIG_ARCH_X86)
    /* As for the ARM implementation above, but we also need to massage the
     * calling convention by taking the value test_registers passed us in EAX
     * and put it on the stack where reply_to_parent expects it.
     */
reply_trampoline:
    leal _safe_stack, %esp
    pushl %eax
    call reply_to_parent

.global test_registers
test_registers:
    /* Assume EIP, GS and FS are correct. Is there a good way to
     * test these?
     *  EIP - If this is incorrect we'll never arrive at this function.
     *  GS - With an incorrect GDT selector we fault and die immediately.
     *  FS - Overwritten by the kernel before we jump here.
     */

    /* We need to test EFLAGS indirectly because we can't cmp it. The jb
     * should only be taken if CF (bit 0) is set.
     */
    jb eflags_ok
    jmp test_registers_fail
    eflags_ok:
    cmpl $0x0000000a, %eax
    jne test_registers_fail
    cmpl $0x0000000b, %ebx
    jne test_registers_fail
    cmpl $0x0000000c, %ecx
    jne test_registers_fail
    cmpl $0x0000000d, %edx
    jne test_registers_fail
    cmpl $0x00000005, %esi
    jne test_registers_fail
    cmpl $0x00000002, %edi
    jne test_registers_fail
    cmpl $0x00000003, %ebp
    jne test_registers_fail
    cmpl $0x00000004, %esp
    jne test_registers_fail

    /* Return success. Note we use a custom calling convention because we
     * don't have a valid stack.
     */
    movl $0, %eax
    jmp reply_trampoline

    /* Return failure. */
test_registers_fail:
    movl $1, %eax
    jmp reply_trampoline
#elif defined(CONFIG_ARCH_RISCV)
/* Trampoline for providing the thread a valid stack before entering
 * reply_to_parent. No jal required because reply_to_parent does not
 * return.
 */
reply_trampoline:
    la a1, _safe_stack
    mv sp, a1
    j reply_to_parent

.global test_registers
test_registers:

    li a0, 1
    bne ra, a0, test_registers_fail
    li a0, 2
    bne sp, a0, test_registers_fail
    li a0, 4
    bne t0, a0, test_registers_fail
    li a0, 5
    bne t1, a0, test_registers_fail
    li a0, 6
    bne t2, a0, test_registers_fail
    li a0, 7
    bne s0, a0, test_registers_fail
    li a0, 8
    bne s1, a0, test_registers_fail
    li a0, 10
    bne a1, a0, test_registers_fail
    li a0, 11
    bne a2, a0, test_registers_fail
    li a0, 12
    bne a0, a3, test_registers_fail
    li a0, 13
    bne a0, a4, test_registers_fail
    li a0, 14
    bne a0, a5, test_registers_fail
    li a0, 15
    bne a0, a6, test_registers_fail
#if 0
    /* skip x3, see below */
    context.x4 = 3;
#endif

    /* Return success. Note that we don't bother saving registers or bl because
     * we're not planning to return here and we don't have a valid stack.
     */
    mv a0, x0
    j reply_trampoline

    /* Return failure. */
test_registers_fail:
    li a0, 1
    j reply_trampoline

#else
#error Unsupported architecture
#endif

