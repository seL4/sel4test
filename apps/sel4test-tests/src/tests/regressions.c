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

/* Include Kconfig variables. */
#include <autoconf.h>

#include <stdio.h>

#include "../helpers.h"

/* This file contains tests related to bugs that have previously occured. Tests
 * should validate that the bug no longer exists.
 */

/* Previously the layout of seL4_UserContext in libsel4 has been inconsistent
 * with frameRegisters/gpRegisters in the kernel. This causes the syscalls
 * seL4_TCB_ReadRegisters and seL4_TCB_WriteRegisters to function incorrectly.
 * The following tests whether this issue has been re-introduced. For more
 * information, see SELFOUR-113.
 */

/* An endpoint for the helper thread and the main thread to synchronise on. */
static seL4_CPtr shared_endpoint;

/* This function provides a wrapper around seL4_Send to the parent thread. It
 * can't be called directly from asm because seL4_Send typically gets inlined
 * and its likely that no visible copy of this function exists to branch to.
 */
void reply_to_parent(seL4_Word result)
__attribute__((noinline));
void reply_to_parent(seL4_Word result)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(result, 0, 0, 0);
    seL4_Word badge = 0; /* ignored */

    seL4_Send(shared_endpoint, info);

    /* Block to avoid returning and assume our parent will kill us. */
    seL4_Wait(shared_endpoint, &badge);
}

/* Test the registers we have been setup with and pass the result back to our
 * parent. This function is really static, but GCC doesn't like a static
 * declaration when the definition is in asm.
 */
void test_registers(void)
#if defined(CONFIG_ARCH_AARCH32)
/* Probably not necessary to mark this function naked as we define the
 * whole thing in asm anyway, but just in case GCC tries to do anything
 * sneaky.
 */
__attribute__((naked))
#endif
;

/* test_registers (below) runs in a context without a valid SP. At the end it
 * needs to call into a C function and needs a valid stack to do this. We
 * provide it a stack it can switch to here.
 */
asm ("\n"
     ".bss                        \n"
     ".align  4                   \n"
     ".space 4096                 \n"
     "_safe_stack:                \n"
     ".text                       \n");
#if defined(CONFIG_ARCH_AARCH32)
asm ("\n"

     /* Trampoline for providing the thread a valid stack before entering
      * reply_to_parent. No blx required because reply_to_parent does not
      * return.
      */
     "reply_trampoline:           \n"
     "\tldr sp, =_safe_stack      \n"
     "\tb reply_to_parent         \n"

     "test_registers:             \n"
     /* Assume the PC and CPSR are correct because there's no good way of
      * testing them. Test each of the registers against the magic numbers our
      * parent set.
      */
     "\tcmp sp, #13               \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r0, #15               \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r1, #1                \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r2, #2                \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r3, #3                \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r4, #4                \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r5, #5                \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r6, #6                \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r7, #7                \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r8, #8                \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r9, #9                \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r10, #10              \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r11, #11              \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r12, #12              \n"
     "\tbne test_registers_fail   \n"
     "\tcmp r14, #14              \n"
     "\tbne test_registers_fail   \n"

     /* Return success. Note that we don't bother saving registers or bl because
      * we're not planning to return here and we don't have a valid stack.
      */
     "\tmov r0, #0                \n"
     "\tb reply_trampoline        \n"

     /* Return failure. */
     "test_registers_fail:        \n"
     "\tmov r0, #1                \n"
     "\tb reply_trampoline        \n"
    );
#elif defined(CONFIG_ARCH_AARCH64)
asm (
     /* Trampoline for providing the thread a valid stack before entering
      * reply_to_parent. No blx required because reply_to_parent does not
      * return.
      */
     "reply_trampoline:           \n"
     "ldr x1, =_safe_stack        \n"
     "mov sp, x1                  \n"
     "b reply_to_parent           \n"

     "test_registers:             \n"
     "cmp sp, #1                  \n"
     "bne test_registers_fail     \n"
     "cmp x0, #2                  \n"
     "bne test_registers_fail     \n"
     "cmp x1, #3                  \n"
     "bne test_registers_fail     \n"
     "cmp x2, #4                  \n"
     "bne test_registers_fail     \n"
     "cmp x3, #5                  \n"
     "bne test_registers_fail     \n"
     "cmp x4, #6                  \n"
     "bne test_registers_fail     \n"
     "cmp x5, #7                  \n"
     "bne test_registers_fail     \n"
     "cmp x6, #8                  \n"
     "bne test_registers_fail     \n"
     "cmp x7, #9                  \n"
     "bne test_registers_fail     \n"
     "cmp x8, #10                 \n"
     "bne test_registers_fail     \n"
     "cmp x9, #11                 \n"
     "bne test_registers_fail     \n"
     "cmp x10, #12                \n"
     "bne test_registers_fail     \n"
     "cmp x11, #13                \n"
     "bne test_registers_fail     \n"
     "cmp x12, #14                \n"
     "bne test_registers_fail     \n"
     "cmp x13, #15                \n"
     "bne test_registers_fail     \n"
     "cmp x14, #16                \n"
     "bne test_registers_fail     \n"
     "cmp x15, #17                \n"
     "bne test_registers_fail     \n"
     "cmp x16, #18                \n"
     "bne test_registers_fail     \n"
     "cmp x17, #19                \n"
     "bne test_registers_fail     \n"
     "cmp x18, #20                \n"
     "bne test_registers_fail     \n"
     "cmp x19, #21                \n"
     "bne test_registers_fail     \n"
     "cmp x20, #22                \n"
     "bne test_registers_fail     \n"
     "cmp x21, #23                \n"
     "bne test_registers_fail     \n"
     "cmp x22, #24                \n"
     "bne test_registers_fail     \n"
     "cmp x23, #25                \n"
     "bne test_registers_fail     \n"
     "cmp x24, #26                \n"
     "bne test_registers_fail     \n"
     "cmp x25, #27                \n"
     "bne test_registers_fail     \n"
     "cmp x26, #28                \n"
     "bne test_registers_fail     \n"
     "cmp x27, #29                \n"
     "bne test_registers_fail     \n"
     "cmp x28, #30                \n"
     "bne test_registers_fail     \n"
     "cmp x29, #31                \n"
     "bne test_registers_fail     \n"
     "cmp x30, #32                \n"
     "bne test_registers_fail     \n"

     /* Return success. Note that we don't bother saving registers or bl because
      * we're not planning to return here and we don't have a valid stack.
      */
     "mov x0, #0                \n"
     "b reply_trampoline        \n"

     /* Return failure. */
     "test_registers_fail:        \n"
     "mov x0, #1                \n"
     "b reply_trampoline        \n"
    );
#elif defined(CONFIG_ARCH_X86_64)
asm ("\n\t"
     "reply_trampoline:         \n"
     "leaq  _safe_stack, %rsp   \n"
     "movq  %rax, %rdi          \n"
     "call  reply_to_parent     \n"
     "test_registers:           \n"
     "jb    rflags_ok           \n"
     "jmp   test_registers_fail \n"
     "rflags_ok:                \n"
     "cmpq  $0x0000000a, %rax   \n"
     "jne   test_registers_fail \n"
     "movq  $2, %rax\n"
     "cmpq  $0x0000000b, %rbx   \n"
     "jne   test_registers_fail \n"
     "movq  $3, %rax\n"
     "cmpq  $0x0000000c, %rcx   \n"
     "jne   test_registers_fail \n"
     "movq  $4, %rax\n"
     "cmpq  $0x0000000d, %rdx   \n"
     "jne   test_registers_fail \n"
     "movq  $5, %rax\n"
     "cmpq  $0x00000005, %rsi   \n"
     "jne   test_registers_fail \n"
     "movq  $6, %rax\n"
     "cmpq  $0x00000002, %rdi   \n"
     "jne   test_registers_fail \n"
     "movq  $7, %rax\n"
     "cmpq  $0x00000003, %rbp   \n"
     "jne   test_registers_fail \n"
     "movq  $8, %rax            \n"
     "cmpq  $0x00000004, %rsp   \n"
     "jne   test_registers_fail \n"
     "movq  $9, %rax\n"
     "cmpq  $0x00000088, %r8    \n"
     "jne   test_registers_fail \n"
     "movq  $100, %rax\n"
     "cmpq  $0x00000099, %r9    \n"
     "jne   test_registers_fail \n"
     "movq  $11, %rax\n"
     "cmpq  $0x00000010, %r10   \n"
     "jne   test_registers_fail \n"
     "movq  $12, %rax\n"
     "cmpq  $0x00000011, %r11   \n"
     "jne   test_registers_fail \n"
     "movq  $13, %rax\n"
     "cmpq  $0x00000012, %r12   \n"
     "jne   test_registers_fail \n"
     "movq  $14, %rax\n"
     "cmpq  $0x00000013, %r13   \n"
     "jne   test_registers_fail \n"
     "movq  $15, %rax\n"
     "cmpq  $0x00000014, %r14   \n"
     "jne   test_registers_fail \n"
     "movq  $16, %rax\n"
     "cmpq  $0x00000015, %r15   \n"
     "jne   test_registers_fail \n"
     "movq  $0, %rax            \n"
     "jmp   reply_trampoline    \n"
     "test_registers_fail:      \n"
     "movq  $1, %rax            \n"
     "jmp   reply_trampoline    \n"
    );
#elif defined(CONFIG_ARCH_X86)
asm ("\n"

     /* As for the ARM implementation above, but we also need to massage the
      * calling convention by taking the value test_registers passed us in EAX
      * and put it on the stack where reply_to_parent expects it.
      */
     "reply_trampoline:           \n"
     "\tleal _safe_stack, %esp    \n"
     "\tpushl %eax                \n"
     "\tcall reply_to_parent      \n"

     "test_registers:             \n"
     /* Assume EIP, GS and FS are correct. Is there a good way to
      * test these?
      *  EIP - If this is incorrect we'll never arrive at this function.
      *  GS - With an incorrect GDT selector we fault and die immediately.
      *  FS - Overwritten by the kernel before we jump here.
      */

     /* We need to test EFLAGS indirectly because we can't cmp it. The jb
      * should only be taken if CF (bit 0) is set.
      */
     "\tjb eflags_ok              \n"
     "\tjmp test_registers_fail   \n"
     "eflags_ok:                  \n"
     "\tcmpl $0x0000000a, %eax    \n"
     "\tjne test_registers_fail   \n"
     "\tcmpl $0x0000000b, %ebx    \n"
     "\tjne test_registers_fail   \n"
     "\tcmpl $0x0000000c, %ecx    \n"
     "\tjne test_registers_fail   \n"
     "\tcmpl $0x0000000d, %edx    \n"
     "\tjne test_registers_fail   \n"
     "\tcmpl $0x00000005, %esi    \n"
     "\tjne test_registers_fail   \n"
     "\tcmpl $0x00000002, %edi    \n"
     "\tjne test_registers_fail   \n"
     "\tcmpl $0x00000003, %ebp    \n"
     "\tjne test_registers_fail   \n"
     "\tcmpl $0x00000004, %esp    \n"
     "\tjne test_registers_fail   \n"

     /* Return success. Note we use a custom calling convention because we
      * don't have a valid stack.
      */
     "\tmovl $0, %eax             \n"
     "\tjmp reply_trampoline      \n"

     /* Return failure. */
     "test_registers_fail:        \n"
     "\tmovl $1, %eax             \n"
     "\tjmp reply_trampoline      \n"
    );
#else
#error "Unsupported architecture"
#endif
int test_write_registers(env_t env)
{
    helper_thread_t thread;
    seL4_UserContext context = { 0 };
    int result;
    seL4_MessageInfo_t info;
    seL4_Word badge = 0; /* ignored */

    /* Create a thread without starting it. Most of these arguments are
     * ignored.
     */
    create_helper_thread(env, &thread);
    shared_endpoint = thread.local_endpoint.cptr;

#if defined(CONFIG_ARCH_AARCH32)
    context.pc = (seL4_Word)&test_registers;
    context.sp = 13;
    context.r0 = 15;
    context.r1 = 1;
    context.r2 = 2;
    context.r3 = 3;
    context.r4 = 4;
    context.r5 = 5;
    context.r6 = 6;
    context.r7 = 7;
    context.r8 = 8;
    context.r9 = 9;
    context.r10 = 10;
    context.r11 = 11;
    context.r12 = 12;
    /* R13 == SP */
    context.r14 = 14; /* LR */
    /* R15 == PC */
#elif defined(CONFIG_ARCH_AARCH64)
    context.pc = (seL4_Word)&test_registers;
    context.sp = 1;
    context.x0 = 2;
    context.x1 = 3;
    context.x2 = 4;
    context.x3 = 5;
    context.x4 = 6;
    context.x5 = 7;
    context.x6 = 8;
    context.x7 = 9;
    context.x8 = 10;
    context.x9 = 11;
    context.x10 = 12;
    context.x11 = 13;
    context.x12 = 14;
    context.x13 = 15;
    context.x14 = 16;
    context.x15 = 17;
    context.x16 = 18;
    context.x17 = 19;
    context.x18 = 20;
    context.x19 = 21;
    context.x20 = 22;
    context.x21 = 23;
    context.x22 = 24;
    context.x23 = 25;
    context.x24 = 26;
    context.x25 = 27;
    context.x26 = 28;
    context.x27 = 29;
    context.x28 = 30;
    context.x29 = 31;
    context.x30 = 32;
#elif defined(CONFIG_ARCH_X86_64)
    context.rip = (seL4_Word)&test_registers;
    context.rsp = 0x00000004UL;
    context.rax = 0x0000000aUL;
    context.rbx = 0x0000000bUL;
    context.rcx = 0x0000000cUL;
    context.rdx = 0x0000000dUL;
    context.rsi = 0x00000005UL;
    context.rdi = 0x00000002UL;
    context.rbp = 0x00000003UL;
    context.rflags = 0x00000001UL;
    context.r8 = 0x00000088UL;
    context.r9 = 0x00000099UL;
    context.r10 = 0x00000010UL;
    context.r11 = 0x00000011UL;
    context.r12 = 0x00000012UL;
    context.r13 = 0x00000013UL;
    context.r14 = 0x00000014UL;
    context.r15 = 0x00000015UL;
#elif defined(CONFIG_ARCH_X86)
    context.eip = (seL4_Word)&test_registers;
    context.esp = 0x00000004;
    context.fs = IPCBUF_GDT_SELECTOR;
    context.eax = 0x0000000a;
    context.ebx = 0x0000000b;
    context.ecx = 0x0000000c;
    context.edx = 0x0000000d;
    context.esi = 0x00000005;
    context.edi = 0x00000002;
    context.ebp = 0x00000003;
    context.eflags = 0x00000001; /* Set the CF bit */
#else
#error "Unsupported architecture"
#endif

    result = seL4_TCB_WriteRegisters(get_helper_tcb(&thread), true, 0 /* ignored */,
                                     sizeof(seL4_UserContext) / sizeof(seL4_Word), &context);

    if (!result) {
        /* If we've successfully started the thread, block until it's checked
         * its registers.
         */
        info = api_recv(shared_endpoint, &badge, get_helper_reply(&thread));
    }
    cleanup_helper(env, &thread);

    test_assert(result == 0);

    result = seL4_MessageInfo_get_label(info);
    test_assert(result == 0);

    return sel4test_get_result();
}
DEFINE_TEST(REGRESSIONS0001, "Ensure WriteRegisters functions correctly", test_write_registers, true)

#if defined(CONFIG_ARCH_ARM)
#if defined(CONFIG_ARCH_AARCH32)
/* Performs an ldrex and strex sequence with a context switch in between. See
 * the comment in the function following for an explanation of purpose.
 */
static int do_ldrex(void)
{
    seL4_Word dummy1, dummy2, result;

    /* We don't really care where we are loading from here. This is just to set
     * the exclusive access tag.
     */
    asm volatile ("ldrex %[rt], [%[rn]]"
                  : [rt]"=&r"(dummy1)
                  : [rn]"r"(&dummy2));

    /* Force a context switch to our parent. */
    seL4_Signal(shared_endpoint);

    /* Again, we don't care where we are storing to. This is to see whether the
     * exclusive access tag is still set.
     */
    asm volatile ("strex %[rd], %[rt], [%[rn]]"
                  : [rd]"=&r"(result)
                  : [rt]"r"(dummy2), [rn]"r"(&dummy1));

    /* The strex should have failed (and returned 1) because the context switch
     * should have cleared the exclusive access tag.
     */
    return result == 0 ? FAILURE : SUCCESS;
}
#elif defined(CONFIG_ARCH_AARCH64)
static int do_ldrex(void)
{
    seL4_Word dummy1, dummy2, result;

    /* We don't really care where we are loading from here. This is just to set
     * the exclusive access tag.
     */
    asm volatile ("ldxr %[rt], [%[rn]]"
                  : [rt]"=&r"(dummy1)
                  : [rn]"r"(&dummy2));

    /* Force a context switch to our parent. */
    seL4_Signal(shared_endpoint);

    /* Again, we don't care where we are storing to. This is to see whether the
     * exclusive access tag is still set.
     */
    asm volatile ("mov %x0, #0\t\n"
                  "stxr %w0, %[rt], [%[rn]]"
                  : [rd]"=&r"(result)
                  : [rt]"r"(dummy2), [rn]"r"(&dummy1));

    /* The stxr should have failed (and returned 1) because the context switch
     * should have cleared the exclusive access tag.
     */
    return result == 0 ? FAILURE : SUCCESS;
}
#else
#error "Unsupported architecture"
#endif

/* Prior to kernel changeset a4656bf3066e the load-exclusive monitor was not
 * cleared on a context switch. This causes unexpected and incorrect behaviour
 * for any userspace program relying on ldrex/strex to implement exclusion
 * mechanisms. This test checks that the monitor is cleared correctly on
 * switch. See SELFOUR-141 for more information.
 */
int test_ldrex_cleared(env_t env)
{
    helper_thread_t thread;
    seL4_Word result;
    seL4_Word badge = 0; /* ignored */

    /* Create a child to perform the ldrex/strex. */
    create_helper_thread(env, &thread);
    shared_endpoint = thread.local_endpoint.cptr;
    start_helper(env, &thread, (helper_fn_t) do_ldrex, 0, 0, 0, 0);

    /* Wait for the child to do ldrex and signal us. */
    seL4_Wait(shared_endpoint, &badge);

    /* Wait for the child to do strex and exit. */
    result = wait_for_helper(&thread);

    cleanup_helper(env, &thread);

    return result;
}
DEFINE_TEST(REGRESSIONS0002, "Test the load-exclusive monitor is cleared on context switch", test_ldrex_cleared, true)
#endif

#if defined(CONFIG_ARCH_IA32)
static volatile int got_cpl = 0;
static volatile uintptr_t stack_after_cpl = 0;
static volatile uint32_t kernel_hash;
void VISIBLE do_after_cpl_change(void) {
    printf("XOR hash for first MB of kernel region 0x%x\n", kernel_hash);
    test_check(false);
    /* we don't have a stack to pop back up to message the test parent,
     * but we can just fault, the result is that the test output
     * will have a 'spurious' invalid instruction error, too bad */
    asm volatile("hlt");
}
static int do_wait_for_cpl(void) {
    /* check our current CPL */
    uint16_t cs;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    if ((cs & 3) == 0) {
        got_cpl = 1;
        /* prove we have root by doing something only the kernel can do */
        /* like disabling interrupts */
        asm volatile("cli");
        /* let's hash a meg of kernel code */
        int i;
        uint32_t *kernel = (uint32_t*)0xe0000000;
        for (i = 0; i < BIT(20) / sizeof(uint32_t); i++) {
            kernel_hash ^= kernel[i];
        }
        /* take away our privileges (and put interupts back on) by constructing
         * an iret. we need to lose root so that we can call the kernel again. We
         * also need to stop using the kernel stack */
        asm volatile(
            "andl $0xFFFFFFE0, %%esp\n"
            "push %[SS] \n"
            "push %[STACK] \n"
            "pushf \n"
            "orl $0x200,(%%esp) \n"
            "push %[CS] \n"
            "push $do_after_cpl_change\n"
            "iret\n"
            :
            : [SS]"r"(0x23),
              [CS]"r"(0x1b),
              [STACK]"r"(stack_after_cpl));
        /* this is unreachable */
    }
    while (1) {
        /* Sit here calling the kernel to maximize the chance that when the
         * the timer interrupt finally fires it will actually happen when
         * we are inside the kernel, this will result in the exception being
         * delayed until we switch back to user mode */
        seL4_Yield();
    }
    return 0;
}

int test_no_ret_with_cpl0(env_t env)
{
    helper_thread_t thread;
    int error;

    /* start a low priority helper thread that we will attempt to change the CPL of */
    create_helper_thread(env, &thread);
    start_helper(env, &thread, (helper_fn_t) do_wait_for_cpl, 0, 0, 0, 0);
    stack_after_cpl = (uintptr_t)get_helper_initial_stack_pointer(&thread);

    for (int i = 0; i < 20; i++) {
        sel4test_sleep(env, NS_IN_S / 10);
        if (got_cpl) {
            wait_for_helper(&thread);
            break;
        }
        /* reset the helper threads registers */
        seL4_UserContext context;
        error = seL4_TCB_ReadRegisters(get_helper_tcb(&thread), false, 0, sizeof(seL4_UserContext) / sizeof(seL4_Word), &context);
        test_eq(error, 0);
        context.eip = (seL4_Word)do_wait_for_cpl;
        /* If all went well in the helper thread then the interrupt came in
         * whilst it was performing a kernel invocation. This means the interrupt
         * would have been masked until it performed a 'sysexit' to return to user.
         * Should an interrupt occur right then, however, the trap frame that is
         * constructed is to the 'sysexit' instruction, and the stored CS and SS
         * are CPL0 (kernel privilege). Kernel privilige is needed because once this
         * thread is resumed (via iret) we will resume at the sysexit (and hence will
         * need kernel privilege), then the sysexit will happen forcively removing
         * kernel privilege.
         * Right now, however, the interrupt has occured and we have woken up. The
         * below call to WriteRegisters will overwrite the return address (which
         * was going to be sysexit) to our own function, which will then be running
         * at CPL0 */
        error = seL4_TCB_WriteRegisters(get_helper_tcb(&thread), true, 0, sizeof(seL4_UserContext) / sizeof(seL4_Word), &context);
        test_eq(error, 0);
    }

    cleanup_helper(env, &thread);

    return sel4test_get_result();
}
DEFINE_TEST(REGRESSIONS0003, "Test return to user with CPL0 exploit", test_no_ret_with_cpl0, config_set(CONFIG_HAVE_TIMER))
#endif /* defined(CONFIG_ARCH_IA32) */
