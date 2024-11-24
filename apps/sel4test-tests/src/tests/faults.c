/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <autoconf.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <sel4utils/arch/util.h>
#include <utils/util.h>

#include <vka/object.h>

#include "../test.h"
#include "../helpers.h"

enum {
    FAULT_DATA_READ_PAGEFAULT = 1,
    FAULT_DATA_WRITE_PAGEFAULT = 2,
    FAULT_INSTRUCTION_PAGEFAULT = 3,
    FAULT_BAD_SYSCALL = 4,
    FAULT_BAD_INSTRUCTION = 5,
};

enum {
    BADGED = seL4_WordBits - 1,
    RESTART = seL4_WordBits - 2,
};

/* Use a different test virtual address on 32 and 64-bit systems so that we can exercise
   the full address space to make sure fault messages are not truncating fault information. */
#if CONFIG_WORD_SIZE == 32
#ifdef CONFIG_ARCH_RISCV32
#define BAD_VADDR 0x7ffedcba    /* 32-bit RISCV userspace */
#else
#define BAD_VADDR 0xf123456C
#endif
#elif CONFIG_WORD_SIZE == 64
#ifdef CONFIG_ARCH_RISCV64
#define BAD_VADDR 0x3CBA987650  /* Valid Sv39 Virtual Address */
#else
/* virtual address we test is in the valid 48-bit portion of the virtual address space */
#define BAD_VADDR 0x7EDCBA987650
#endif
#endif
#define GOOD_MAGIC 0x15831851
#define BAD_MAGIC ~GOOD_MAGIC
#define BAD_SYSCALL_NUMBER 0xc1

#define EXPECTED_BADGE 0xabababa

#ifdef CONFIG_ARCH_RISCV
#if __riscv_xlen == 32
#define LOAD  lw
#define STORE sw
#else /* __riscv_xlen == 64 */
#define LOAD  ld
#define STORE sd
#endif

#define LOAD_S STRINGIFY(LOAD)
#define STORE_S STRINGIFY(STORE)
#endif

extern char read_fault_address[];
extern char read_fault_restart_address[];
static void __attribute__((noinline))
do_read_fault(void)
{
    int *x = (int *)BAD_VADDR;
    int val = BAD_MAGIC;
    /* Do a read fault. */
#if defined(CONFIG_ARCH_AARCH32)
    asm volatile(
        "mov r0, %[val]\n\t"
        "read_fault_address:\n\t"
        "ldr r0, [%[addrreg]]\n\t"
        "read_fault_restart_address:\n\t"
        "mov %[val], r0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "r0"
    );
#elif defined(CONFIG_ARCH_AARCH64)
    asm volatile(
        "mov w0, %w[val]\n\t"
        "read_fault_address:\n\t"
        "ldr x0, [%[addrreg]]\n\t"
        "read_fault_restart_address:\n\t"
        "mov %w[val], w0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "x0"
    );
#elif defined(CONFIG_ARCH_RISCV)
    asm volatile(
        "mv a0, %[val]\n\t"
        "read_fault_address:\n\t"
        LOAD_S " a0, 0(%[addrreg])\n\t"
        "read_fault_restart_address:\n\t"
        "mv %[val], a0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "a0"
    );
#elif defined(CONFIG_ARCH_X86)
    asm volatile(
        "mov %[val], %%eax\n\t"
        "read_fault_address:\n\t"
        "mov (%[addrreg]), %%eax\n\t"
        "read_fault_restart_address:\n\t"
        "mov %%eax, %[val]\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "eax"
    );
#else
#error "Unknown architecture."
#endif
    test_check(val == GOOD_MAGIC);
}

extern char write_fault_address[];
extern char write_fault_restart_address[];
static void __attribute__((noinline))
do_write_fault(void)
{
    int *x = (int *)BAD_VADDR;
    int val = BAD_MAGIC;
    /* Do a write fault. */
#if defined(CONFIG_ARCH_AARCH32)
    asm volatile(
        "mov r0, %[val]\n\t"
        "write_fault_address:\n\t"
        "str r0, [%[addrreg]]\n\t"
        "write_fault_restart_address:\n\t"
        "mov %[val], r0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "r0"
    );
#elif defined(CONFIG_ARCH_AARCH64)
    asm volatile(
        "mov w0, %w[val]\n\t"
        "write_fault_address:\n\t"
        "str x0, [%[addrreg]]\n\t"
        "write_fault_restart_address:\n\t"
        "mov %w[val], w0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "x0"
    );
#elif defined(CONFIG_ARCH_RISCV)
    asm volatile(
        "mv a0, %[val]\n\t"
        "write_fault_address:\n\t"
        STORE_S " a0, 0(%[addrreg])\n\t"
        "write_fault_restart_address:\n\t"
        "mv %[val], a0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "a0"
    );
#elif defined(CONFIG_ARCH_X86)
    asm volatile(
        "mov %[val], %%eax\n\t"
        "write_fault_address:\n\t"
        "mov %%eax, (%[addrreg])\n\t"
        "write_fault_restart_address:\n\t"
        "mov %%eax, %[val]\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "eax"
    );
#else
#error "Unknown architecture."
#endif
    test_check(val == GOOD_MAGIC);
}

extern char instruction_fault_restart_address[];
static void __attribute__((noinline))
do_instruction_fault(void)
{
    int *x = (int *)BAD_VADDR;
    int val = BAD_MAGIC;
    /* Jump to a crazy address. */
#if defined(CONFIG_ARCH_AARCH32)
    asm volatile(
        "mov r0, %[val]\n\t"
        "blx %[addrreg]\n\t"
        "instruction_fault_restart_address:\n\t"
        "mov %[val], r0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "r0", "lr"
    );
#elif defined(CONFIG_ARCH_AARCH64)
    asm volatile(
        "mov w0, %w[val]\n\t"
        "blr %[addrreg]\n\t"
        "instruction_fault_restart_address:\n\t"
        "mov %w[val], w0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "x0", "x30"
    );
#elif defined(CONFIG_ARCH_RISCV)
    asm volatile(
        "mv a0, %[val]\n\t"
        "jalr %[addrreg]\n\t"
        "instruction_fault_restart_address:\n\t"
        "mv %[val], a0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "a0", "ra"
    );
#elif defined(CONFIG_ARCH_X86)
    asm volatile(
        "mov %[val], %%eax\n\t"
        "instruction_fault_address:\n\t"
        "jmp *%[addrreg]\n\t"
        "instruction_fault_restart_address:\n\t"
        "mov %%eax, %[val]\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x)
        : "eax"
    );
#else
#error "Unknown architecture."
#endif
    test_check(val == GOOD_MAGIC);
}

extern char bad_syscall_address[];
extern char bad_syscall_restart_address[];
static void __attribute__((noinline))
do_bad_syscall(void)
{
    int *x = (int *)BAD_VADDR;
    int val = BAD_MAGIC;
    /* Do an undefined system call. */
#if defined(CONFIG_ARCH_AARCH32)
    asm volatile(
        "mov r7, %[scno]\n\t"
        "mov r0, %[val]\n\t"
        "bad_syscall_address:\n\t"
        "svc %[scno]\n\t"
        "bad_syscall_restart_address:\n\t"
        "mov %[val], r0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x),
        [scno] "i"(BAD_SYSCALL_NUMBER)
        : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "memory", "cc"
    );
#elif defined(CONFIG_ARCH_AARCH64)
    asm volatile(
        "mov x7, %[scno]\n\t"
        "mov w0, %w[val]\n\t"
        "bad_syscall_address:\n\t"
        "svc %[scno]\n\t"
        "bad_syscall_restart_address:\n\t"
        "mov %w[val], w0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x),
        [scno] "i"(BAD_SYSCALL_NUMBER)
        : "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "memory", "cc"
    );
#elif defined(CONFIG_ARCH_RISCV)
    asm volatile(
        "li a7, %[scno]\n\t"
        "mv a0, %[val]\n\t"
        "bad_syscall_address:\n\t"
        "ecall \n\t"
        "bad_syscall_restart_address:\n\t"
        "mv %[val], a0\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x),
        [scno] "i"(BAD_SYSCALL_NUMBER)
        : "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "memory", "cc"
    );
#elif defined(CONFIG_ARCH_X86_64) && defined(CONFIG_SYSENTER)
    asm volatile(
        "movl   %[val], %%ebx\n\t"
        "movq   %%rsp, %%rcx\n\t"
        "leaq   1f, %%rdx\n\t"
        "bad_syscall_address:\n\t"
        "1: \n\t"
        "sysenter\n\t"
        "bad_syscall_restart_address:\n\t"
        "movl   %%ebx, %[val]\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x),
        [scno] "a"(BAD_SYSCALL_NUMBER)
        : "rbx", "rcx", "rdx"
    );
#elif defined(CONFIG_SYSCALL)
    asm volatile(
        "movl   %[val], %%ebx\n\t"
        "movq   %%rsp, %%r12\n\t"
        "bad_syscall_address:\n\t"
        "syscall\n\t"
        "bad_syscall_restart_address:\n\t"
        "movq   %%r12, %%rsp\n"
        "movl   %%ebx, %[val]\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x),
        [scno] "d"(BAD_SYSCALL_NUMBER)
        : "rax", "rbx", "rcx", "r11", "r12"
    );
#elif defined(CONFIG_ARCH_IA32)
    asm volatile(
        "mov %[scno], %%eax\n\t"
        "mov %[val], %%ebx\n\t"
        "mov %%esp, %%ecx\n\t"
        "leal 1f, %%edx\n\t"
        "bad_syscall_address:\n\t"
        "1:\n\t"
        "sysenter\n\t"
        "bad_syscall_restart_address:\n\t"
        "mov %%ebx, %[val]\n\t"
        : [val] "+r"(val)
        : [addrreg] "r"(x),
        [scno] "i"(BAD_SYSCALL_NUMBER)
        : "eax", "ebx", "ecx", "edx"
    );
#else
#error "Unknown architecture."
#endif
    test_check(val == GOOD_MAGIC);
}

extern char bad_instruction_address[];
extern char bad_instruction_restart_address[];
static seL4_Word bad_instruction_sp; /* To reset afterwards. */
static seL4_Word bad_instruction_cpsr; /* For checking against. */
static void __attribute__((noinline))
do_bad_instruction(void)
{
    int val = BAD_MAGIC;
    /* Execute an undefined instruction. */
#if defined(CONFIG_ARCH_AARCH32)
    asm volatile(
        /* Save SP */
        "str sp, [%[sp]]\n\t"

        /* Save CPSR */
        "mrs r0, cpsr\n\t"
        "str r0, [%[cpsr]]\n\t"

        /* Set SP to val. */
        "mov sp, %[valptr]\n\t"

        "bad_instruction_address:\n\t"
        ".word 0xe7f000f0\n\t" /* Guaranteed to be undefined by ARM. */
        "bad_instruction_restart_address:\n\t"
        :
        : [sp] "r"(&bad_instruction_sp),
        [cpsr] "r"(&bad_instruction_cpsr),
        [valptr] "r"(&val)
        : "r0", "memory"
    );
#elif defined(CONFIG_ARCH_AARCH64)
    asm volatile(
        /* Save SP */
        "mov x0, sp\n\t"
        "str x0, [%[sp]]\n\t"

        /* Save PSTATE.nzcv */
        "mrs x0, nzcv\n\t"
        "str x0, [%[cpsr]]\n\t"

        /* Set SP to val. */
        "mov sp, %[valptr]\n\t"

        "bad_instruction_address:\n\t"
        ".word 0xe7f000f0\n\t" /* Guaranteed to be undefined by ARM. */
        "bad_instruction_restart_address:\n\t"
        :
        : [sp] "r"(&bad_instruction_sp),
        [cpsr] "r"(&bad_instruction_cpsr),
        [valptr] "r"(&val)
        : "x0", "memory"
    );
#elif defined(CONFIG_ARCH_RISCV)
    asm volatile(
        /* Save SP */
        "mv  a0, sp\n\t"
        STORE_S " a0, 0(%[sp])\n\t"

        /* Set SP to val. */
        "mv sp, %[valptr]\n\t"

        /* All ones is used as the illegal instruction because it is reasonable
         * to assume that all current targets support a maximum instruction
         * length (ILEN) of 32 bits. All zeros was considered and not used
         * because all ones is easier to validated for the purposes of this
         * test. Note: on targets that require 32 bit aligned instructions,
         * this will be 32 bit aligned due to the previous instruction. */
        "bad_instruction_address:\n\t"
        ".word 0xffffffff\n\t"
        "bad_instruction_restart_address:\n\t"
        :
        : [sp] "r"(&bad_instruction_sp),
        [valptr] "r"(&val)
        : "a0", "memory"
    );
#elif defined(CONFIG_ARCH_X86_64)
    asm volatile(
        /* save RSP */
        "movq   %%rsp, (%[sp])\n\t"
        "pushf\n\t"
        "pop    %%rax\n\t"
        "movq   %%rax, (%[cpsr])\n\t"
        "movq   %[valptr], %%rsp\n\t"
        "bad_instruction_address:\n\t"
        "ud2\n\t"
        "bad_instruction_restart_address:\n\t"
        :
        : [sp] "r"(&bad_instruction_sp),
        [cpsr] "r"(&bad_instruction_cpsr),
        [valptr] "r"(&val)
        : "rax", "memory"
    );
#elif defined(CONFIG_ARCH_IA32)
    asm volatile(
        /* Save SP */
        "mov %%esp, (%[sp])\n\t"

        /* Save CPSR */
        "pushf\n\t"
        "pop %%eax\n\t"
        "mov %%eax, (%[cpsr])\n\t"

        /* Set SP to val. */
        "mov %[valptr], %%esp\n\t"

        "bad_instruction_address:\n\t"
        "ud2\n\t"
        "bad_instruction_restart_address:\n\t"
        :
        : [sp] "r"(&bad_instruction_sp),
        [cpsr] "r"(&bad_instruction_cpsr),
        [valptr] "r"(&val)
        : "eax", "memory"
    );
#else
#error "Unknown architecture."
#endif
    test_check(val == GOOD_MAGIC);
}

static void __attribute__((noinline))
set_good_magic_and_set_pc(seL4_CPtr tcb, seL4_Word new_pc)
{
    /* Set their register to GOOD_MAGIC and set PC past fault. */
    int error;
    seL4_UserContext ctx;
    error = seL4_TCB_ReadRegisters(tcb,
                                   false,
                                   0,
                                   sizeof(ctx) / sizeof(seL4_Word),
                                   &ctx);
    test_check(!error);
#if defined(CONFIG_ARCH_AARCH32)
    test_check(ctx.r0 == BAD_MAGIC);
    ctx.r0 = GOOD_MAGIC;
    ctx.pc = new_pc;
#elif defined(CONFIG_ARCH_AARCH64)
    test_check((int)ctx.x0 == BAD_MAGIC);
    ctx.x0 = GOOD_MAGIC;
    ctx.pc = new_pc;
#elif defined(CONFIG_ARCH_RISCV)
    test_check((int)ctx.a0 == BAD_MAGIC);
    ctx.a0 = GOOD_MAGIC;
    ctx.pc = new_pc;
#elif defined(CONFIG_ARCH_X86_64)
    test_check((int)ctx.rax == BAD_MAGIC);
    ctx.rax = GOOD_MAGIC;
    ctx.rip = new_pc;
#elif defined(CONFIG_ARCH_IA32)
    test_check(ctx.eax == BAD_MAGIC);
    ctx.eax = GOOD_MAGIC;
    ctx.eip = new_pc;
#else
#error "Unknown architecture."
#endif
    error = seL4_TCB_WriteRegisters(tcb,
                                    false,
                                    0,
                                    sizeof(ctx) / sizeof(seL4_Word),
                                    &ctx);
    test_check(!error);
}

static int handle_fault(seL4_CPtr fault_ep, seL4_CPtr tcb, seL4_Word expected_fault,
                        seL4_Word flags_and_reply)
{
    seL4_MessageInfo_t tag;
    seL4_Word sender_badge = 0;
    seL4_CPtr reply = flags_and_reply & MASK(RESTART);
    bool badged = flags_and_reply & BIT(BADGED);
    bool restart = flags_and_reply & BIT(RESTART);

    tag = api_recv(fault_ep, &sender_badge, reply);

    if (badged) {
        test_check(sender_badge == EXPECTED_BADGE);
    } else {
        test_check(sender_badge == 0);
    }

    switch (expected_fault) {
    case FAULT_DATA_READ_PAGEFAULT:
        test_check(seL4_MessageInfo_get_label(tag) == seL4_Fault_VMFault);
        test_check(seL4_MessageInfo_get_length(tag) == seL4_VMFault_Length);
        test_check(seL4_GetMR(seL4_VMFault_IP) == (seL4_Word)read_fault_address);
        test_check(seL4_GetMR(seL4_VMFault_Addr) == BAD_VADDR);
        test_check(seL4_GetMR(seL4_VMFault_PrefetchFault) == 0);
        test_check(sel4utils_is_read_fault());

        /* Clear MRs to ensure they get repopulated. */
        seL4_SetMR(seL4_VMFault_Addr, 0);

        set_good_magic_and_set_pc(tcb, (seL4_Word)read_fault_restart_address);
        if (restart) {
            api_reply(reply, tag);
        }
        break;

    case FAULT_DATA_WRITE_PAGEFAULT:
        test_check(seL4_MessageInfo_get_label(tag) == seL4_Fault_VMFault);
        test_check(seL4_MessageInfo_get_length(tag) == seL4_VMFault_Length);
        test_check(seL4_GetMR(seL4_VMFault_IP) == (seL4_Word)write_fault_address);
        test_check(seL4_GetMR(seL4_VMFault_Addr) == BAD_VADDR);
        test_check(seL4_GetMR(seL4_VMFault_PrefetchFault) == 0);
        test_check(!sel4utils_is_read_fault());

        /* Clear MRs to ensure they get repopulated. */
        seL4_SetMR(seL4_VMFault_Addr, 0);

        set_good_magic_and_set_pc(tcb, (seL4_Word)write_fault_restart_address);
        if (restart) {
            api_reply(reply, tag);
        }
        break;

    case FAULT_INSTRUCTION_PAGEFAULT:
        test_check(seL4_MessageInfo_get_label(tag) == seL4_Fault_VMFault);
        test_check(seL4_MessageInfo_get_length(tag) == seL4_VMFault_Length);
        test_check(seL4_GetMR(seL4_VMFault_IP) == BAD_VADDR);
        test_check(seL4_GetMR(seL4_VMFault_Addr) == BAD_VADDR);
#if defined(CONFIG_ARCH_ARM) || defined(CONFIG_ARCH_RISCV)
        /* Prefetch fault is only set on ARM and RISCV. */
        test_check(seL4_GetMR(seL4_VMFault_PrefetchFault) == 1);
#endif
        test_check(sel4utils_is_read_fault());

        /* Clear MRs to ensure they get repopulated. */
        seL4_SetMR(seL4_VMFault_Addr, 0);

        set_good_magic_and_set_pc(tcb, (seL4_Word)instruction_fault_restart_address);
        if (restart) {
            api_reply(reply, tag);
        }
        break;

    case FAULT_BAD_SYSCALL:
        test_eq(seL4_MessageInfo_get_label(tag), (seL4_Word) seL4_Fault_UnknownSyscall);
        test_eq(seL4_MessageInfo_get_length(tag), (seL4_Word) seL4_UnknownSyscall_Length);
        test_eq(seL4_GetMR(seL4_UnknownSyscall_FaultIP), (seL4_Word) bad_syscall_address);
        test_eq((int)seL4_GetMR(seL4_UnknownSyscall_Syscall), BAD_SYSCALL_NUMBER);
        seL4_SetMR(seL4_UnknownSyscall_FaultIP, (seL4_Word)bad_syscall_restart_address);
#if defined(CONFIG_ARCH_AARCH32)
        test_eq(seL4_GetMR(seL4_UnknownSyscall_R0), BAD_MAGIC);
        seL4_SetMR(seL4_UnknownSyscall_R0, GOOD_MAGIC);
#elif defined(CONFIG_ARCH_AARCH64)
        test_eq((int)seL4_GetMR(seL4_UnknownSyscall_X0), BAD_MAGIC);
        seL4_SetMR(seL4_UnknownSyscall_X0, GOOD_MAGIC);
#elif defined(CONFIG_ARCH_X86_64)
        test_eq((int)seL4_GetMR(seL4_UnknownSyscall_RBX), BAD_MAGIC);
        test_eq(seL4_GetMR(seL4_UnknownSyscall_FaultIP), (seL4_Word) bad_syscall_restart_address);
        seL4_SetMR(seL4_UnknownSyscall_RBX, GOOD_MAGIC);
#elif defined(CONFIG_ARCH_RISCV)
        test_eq((int)seL4_GetMR(seL4_UnknownSyscall_A0), BAD_MAGIC);
        test_eq(seL4_GetMR(seL4_UnknownSyscall_FaultIP), (seL4_Word) bad_syscall_restart_address);
        seL4_SetMR(seL4_UnknownSyscall_A0, GOOD_MAGIC);
        seL4_SetMR(seL4_UnknownSyscall_FaultIP, (seL4_Word)bad_syscall_restart_address);
#elif defined(CONFIG_ARCH_IA32)
        test_eq(seL4_GetMR(seL4_UnknownSyscall_EBX), BAD_MAGIC);
        seL4_SetMR(seL4_UnknownSyscall_EBX, GOOD_MAGIC);
        /* Syscalls on ia32 seem to restart themselves with sysenter. */
#else
#error "Unknown architecture."
#endif

        /* Flag that the thread should be restarted. */
        if (restart) {
            seL4_MessageInfo_ptr_set_label(&tag, 0);
        } else {
            seL4_MessageInfo_ptr_set_label(&tag, 1);
        }
        api_reply(reply, tag);
        break;

    case FAULT_BAD_INSTRUCTION:
        test_check(seL4_MessageInfo_get_label(tag) == seL4_Fault_UserException);
        test_check(seL4_MessageInfo_get_length(tag) == seL4_UserException_Length);
        test_check(seL4_GetMR(0) == (seL4_Word)bad_instruction_address);
        int *valptr = (int *)seL4_GetMR(1);
        test_check(*valptr == BAD_MAGIC);
#if defined(CONFIG_ARCH_AARCH32)
        test_check(seL4_GetMR(2) == bad_instruction_cpsr);
        test_check(seL4_GetMR(3) == 0);
        test_check(seL4_GetMR(4) == 0);
#elif defined(CONFIG_ARCH_AARCH64)
        /* We only can access PSTATE.nzcv flags in EL0 in aarch64
         * so, just make sure ther are preserved ... */
        test_check((seL4_GetMR(2) & ~MASK(27)) == bad_instruction_cpsr);
        /* instruction fault on a 32 bit instruction with ISS = 0 */
        test_check(seL4_GetMR(3) == 0x02000000);
        test_check(seL4_GetMR(4) == 0);
#elif defined(CONFIG_ARCH_RISCV)
        test_check(seL4_GetMR(2) == 2);
        test_check(seL4_GetMR(3) == 0);
#elif defined(CONFIG_ARCH_X86)
        /*
         * Curiously, the "resume flag" (bit 16) is set between the
         * undefined syscall and seL4 grabbing the tasks's flags. This only
         * happens on x86 hardware, but not qemu. Just ignore it when
         * checking flags.
         */
        seL4_Word mask_out = ~(1 << 16);
        test_check(((seL4_GetMR(2) ^ bad_instruction_cpsr) & mask_out) == 0);
#else
#error "Unknown architecture."
#endif

        *valptr = GOOD_MAGIC;
        seL4_SetMR(0, (seL4_Word)bad_instruction_restart_address);
        seL4_SetMR(1, bad_instruction_sp);

        /* Flag that the thread should be restarted. */
        if (restart) {
            seL4_MessageInfo_ptr_set_label(&tag, 0);
        } else {
            seL4_MessageInfo_ptr_set_label(&tag, 1);
        }

        api_reply(reply, tag);
        break;

    default:
        /* What? Why are we here? What just happened? */
        test_assert(0);
        break;
    }

    return 0;
}

static int cause_fault(int fault_type)
{
    switch (fault_type) {
    case FAULT_DATA_READ_PAGEFAULT:
        do_read_fault();
        break;
    case FAULT_DATA_WRITE_PAGEFAULT:
        do_write_fault();
        break;
    case FAULT_INSTRUCTION_PAGEFAULT:
        do_instruction_fault();
        break;
    case FAULT_BAD_SYSCALL:
        do_bad_syscall();
        break;
    case FAULT_BAD_INSTRUCTION:
        do_bad_instruction();
        break;
    }

    return 0;
}

static int test_fault(env_t env, int fault_type, bool inter_as)
{
    helper_thread_t handler_thread;
    helper_thread_t faulter_thread;
    int error;

    vka_object_t reply;
    error = vka_alloc_reply(&env->vka, &reply);

    for (int badged = 0; badged <= 1; badged++) {
        seL4_Word handler_arg0, handler_arg1;
        /* The endpoint on which faults are received. */
        seL4_CPtr fault_ep = vka_alloc_endpoint_leaky(&env->vka);
        if (badged) {
            seL4_CPtr badged_fault_ep = get_free_slot(env);
            cnode_mint(env, fault_ep, badged_fault_ep, seL4_AllRights, EXPECTED_BADGE);

            fault_ep = badged_fault_ep;
        }

        seL4_CPtr faulter_vspace, faulter_cspace, reply_cptr;

        if (inter_as) {
            create_helper_process(env, &faulter_thread);
            create_helper_process(env, &handler_thread);

            /* copy the fault endpoint to the faulter */
            cspacepath_t path;
            vka_cspace_make_path(&env->vka,  fault_ep, &path);
            seL4_CPtr remote_fault_ep = sel4utils_copy_path_to_process(&faulter_thread.process, path);
            assert(remote_fault_ep != -1);

            if (!config_set(CONFIG_KERNEL_MCS)) {
                fault_ep = remote_fault_ep;
            }

            /* copy the fault endpoint to the handler */
            handler_arg0 = sel4utils_copy_path_to_process(&handler_thread.process, path);
            assert(handler_arg0 != -1);

            /* copy the fault tcb to the handler */
            vka_cspace_make_path(&env->vka, get_helper_tcb(&faulter_thread), &path);
            handler_arg1 = sel4utils_copy_path_to_process(&handler_thread.process, path);
            assert(handler_arg1 != -1);

            reply_cptr = sel4utils_copy_cap_to_process(&handler_thread.process, &env->vka, reply.cptr);
            faulter_cspace = faulter_thread.process.cspace.cptr;
            faulter_vspace = faulter_thread.process.pd.cptr;
        } else {
            create_helper_thread(env, &faulter_thread);
            create_helper_thread(env, &handler_thread);
            faulter_cspace = env->cspace_root;
            faulter_vspace = env->page_directory;
            handler_arg0 = fault_ep;
            handler_arg1 = get_helper_tcb(&faulter_thread);
            reply_cptr = reply.cptr;
        }

        set_helper_priority(env, &handler_thread, 101);
        error = api_tcb_set_space(get_helper_tcb(&faulter_thread),
                                  fault_ep,
                                  faulter_cspace,
                                  api_make_guard_skip_word(seL4_WordBits - env->cspace_size_bits),
                                  faulter_vspace, seL4_NilData);
        test_error_eq(error, seL4_NoError);

        // Ensure that the BADGED and RESTART bits are not
        // already set on the cptr.
        test_assert(!(reply_cptr & (BIT(RESTART) | BIT(BADGED))));

        for (int restart = 0; restart <= 1; restart++) {
            seL4_Word flags_and_reply = reply_cptr |
                                        (badged ? BIT(BADGED) : 0) |
                                        (restart ? BIT(RESTART) : 0);
            for (int prio = 100; prio <= 102; prio++) {
                set_helper_priority(env, &faulter_thread, prio);
                start_helper(env, &handler_thread, (helper_fn_t) handle_fault,
                             handler_arg0, handler_arg1, fault_type, flags_and_reply);
                start_helper(env, &faulter_thread, (helper_fn_t) cause_fault,
                             fault_type, 0, 0, 0);
                wait_for_helper(&handler_thread);

                if (restart) {
                    wait_for_helper(&faulter_thread);
                }
            }
        }
        cleanup_helper(env, &handler_thread);
        cleanup_helper(env, &faulter_thread);
    }
    return sel4test_get_result();
}

static int test_read_fault(env_t env)
{
    return test_fault(env, FAULT_DATA_READ_PAGEFAULT, false);
}
DEFINE_TEST(PAGEFAULT0001, "Test read page fault", test_read_fault, !config_set(CONFIG_FT))

static int test_write_fault(env_t env)
{
    return test_fault(env, FAULT_DATA_WRITE_PAGEFAULT, false);
}
DEFINE_TEST(PAGEFAULT0002, "Test write page fault", test_write_fault, !config_set(CONFIG_FT))

static int test_execute_fault(env_t env)
{
    return test_fault(env,  FAULT_INSTRUCTION_PAGEFAULT, false);
}
DEFINE_TEST(PAGEFAULT0003, "Test execute page fault", test_execute_fault, !config_set(CONFIG_FT))

static int test_bad_syscall(env_t env)
{
    return test_fault(env, FAULT_BAD_SYSCALL, false);
}
DEFINE_TEST(PAGEFAULT0004, "Test unknown system call", test_bad_syscall, true)

static int test_bad_instruction(env_t env)
{
    return test_fault(env, FAULT_BAD_INSTRUCTION, false);
}
DEFINE_TEST(PAGEFAULT0005, "Test undefined instruction", test_bad_instruction, true)

static int test_read_fault_interas(env_t env)
{
    return test_fault(env, FAULT_DATA_READ_PAGEFAULT, true);
}
DEFINE_TEST(PAGEFAULT1001, "Test read page fault (inter-AS)", test_read_fault_interas, true)

static int test_write_fault_interas(env_t env)
{
    return test_fault(env, FAULT_DATA_WRITE_PAGEFAULT, true);
}
DEFINE_TEST(PAGEFAULT1002, "Test write page fault (inter-AS)", test_write_fault_interas, true)

static int test_execute_fault_interas(env_t env)
{
    return test_fault(env, FAULT_INSTRUCTION_PAGEFAULT, true);
}
DEFINE_TEST(PAGEFAULT1003, "Test execute page fault (inter-AS)", test_execute_fault_interas, true)

static int test_bad_syscall_interas(env_t env)
{
    return test_fault(env, FAULT_BAD_SYSCALL, true);
}
DEFINE_TEST(PAGEFAULT1004, "Test unknown system call (inter-AS)", test_bad_syscall_interas, true)

/* This test currently fails. It is disabled until it can be investigated and fixed. */
static int test_bad_instruction_interas(env_t env)
{
    return test_fault(env, FAULT_BAD_INSTRUCTION, true);
}
DEFINE_TEST(PAGEFAULT1005, "Test undefined instruction (inter-AS)", test_bad_instruction_interas, false)

static void
timeout_fault_0001_fn(void)
{
    while (1);
}

int test_timeout_fault(env_t env)
{
    helper_thread_t helper;
    seL4_Word data = 1;
    seL4_CPtr endpoint = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr ro = vka_alloc_reply_leaky(&env->vka);

    create_helper_thread(env, &helper);
    set_helper_sched_params(env, &helper, 1 * US_IN_MS, 2 * US_IN_MS, data);
    set_helper_tfep(env, &helper, endpoint);
    start_helper(env, &helper, (helper_fn_t) timeout_fault_0001_fn, 0, 0, 0, 0);

    /* wait for timeout fault */
    UNUSED seL4_MessageInfo_t info = api_recv(endpoint, NULL, ro);
    for (int i = 0; i < 100; i++) {
#ifdef CONFIG_KERNEL_MCS
        test_eq(seL4_MessageInfo_get_length(info), (seL4_Word) seL4_Timeout_Length);
        test_check(seL4_isTimeoutFault_tag(info));
        test_eq(seL4_GetMR(seL4_Timeout_Data), data);
#endif
        info = api_reply_recv(endpoint, seL4_MessageInfo_new(0, 0, 0, 0), NULL, ro);
    }

    return sel4test_get_result();
}
DEFINE_TEST(TIMEOUTFAULT0001, "Test timeout fault", test_timeout_fault, config_set(CONFIG_KERNEL_MCS))

void
timeout_fault_server_fn(seL4_CPtr ep, env_t env, seL4_CPtr ro)
{
    /* signal to initialiser that we are done, and wait for a message from
     * the client */
    ZF_LOGD("Server signal recv");
    api_nbsend_recv(ep, seL4_MessageInfo_new(0, 0, 0, 0), ep, NULL, ro);
    /* spin, this will use up all of the clients budget */
    while (true);
    /* we should not get here, as a timeout fault should have been raised
     * and the handler will reset us */
    ZF_LOGF("Should not get here");
}

static int timeout_fault_client_fn(seL4_CPtr ep)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 0);
    while (1) {
        info = seL4_Call(ep, info);
        /* call should have failed, timeout fault handler will send a -1 */
        test_eq(seL4_GetMR(0), (seL4_Word) - 1);
    }
    return 0;
}

int create_passive_thread_with_tfep(env_t env, helper_thread_t *passive, seL4_CPtr tfep,
                                    seL4_Word badge, helper_fn_t fn, seL4_CPtr ep, seL4_Word arg1,
                                    seL4_Word arg2, seL4_Word arg3, sel4utils_checkpoint_t *cp)
{
    seL4_CPtr minted_tfep = get_free_slot(env);
    int error = cnode_mint(env, tfep, minted_tfep, seL4_AllRights, badge);
    test_eq(error, seL4_NoError);

    error = create_passive_thread(env, passive, fn, ep, arg1, arg2, arg3);
    set_helper_tfep(env, passive, minted_tfep);
    test_eq(error, 0);

    /* checkpoint */
    return sel4utils_checkpoint_thread(&passive->thread, cp, false);
}

static int handle_timeout_fault(seL4_CPtr tfep, seL4_Word expected_badge, sel4utils_thread_t *server,
                                seL4_CPtr reply, sel4utils_checkpoint_t *cp, seL4_CPtr ep,
                                seL4_Word expected_data, env_t env)
{
    seL4_Word badge;
    seL4_CPtr server_reply = vka_alloc_reply_leaky(&env->vka);

    /* wait for timeout fault */
    ZF_LOGD("Wait for tf");
    seL4_MessageInfo_t info = api_recv(tfep, &badge, server_reply);
    test_eq(badge, expected_badge);
#ifdef CONFIG_KERNEL_MCS
    test_check(seL4_isTimeoutFault_tag(info));
    test_eq(seL4_GetMR(seL4_Timeout_Data), expected_data);
    test_eq(seL4_MessageInfo_get_length(info), (seL4_Word) seL4_Timeout_Length);
#endif
    /* reply to client on behalf of server */
    seL4_SetMR(0, -1);
    seL4_Send(reply, info);

    size_t stack_size = (uintptr_t) cp->thread->stack_top - cp->sp;
    memcpy((void *) cp->sp, cp->stack, stack_size);

    /* restore server */
    ZF_LOGD("Restoring server");
    int error = api_sc_bind(server->sched_context.cptr, server->tcb.cptr);
    test_eq(error, seL4_NoError);

    ZF_LOGD("Reply to server");
#ifdef CONFIG_KERNEL_MCS
    info = seL4_TimeoutReply_new(true, cp->regs, sizeof(seL4_UserContext) / sizeof(seL4_Word));
#endif
    /* reply, restoring server state, and wait for server to init */
    api_reply_recv(ep, info, NULL, server_reply);

    error = api_sc_unbind(server->sched_context.cptr);
    test_eq(error, seL4_NoError);

    return 0;
}

static int test_timeout_fault_in_server(env_t env)
{
    helper_thread_t client, server;
    seL4_Word client_data = 1;
    seL4_Word server_badge = 2;
    sel4utils_checkpoint_t cp;

    seL4_CPtr tfep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr ro = vka_alloc_reply_leaky(&env->vka);

    /* create the server */
    int error = create_passive_thread_with_tfep(env, &server, tfep, server_badge,
                                                (helper_fn_t) timeout_fault_server_fn, ep,
                                                (seL4_Word)env, ro, 0, &cp);
    test_eq(error, 0);

    /* create the client */
    create_helper_thread(env, &client);
    set_helper_sched_params(env, &client, 100 * US_IN_MS, 101 * US_IN_MS, client_data);
    start_helper(env, &client, (helper_fn_t) timeout_fault_client_fn, ep, 0, 0, 0);

    /* Ensure the client doesn't preempt the server when the server is
     * being reset */
    set_helper_priority(env, &server, OUR_PRIO - 1);
    set_helper_priority(env, &client, OUR_PRIO - 2);

    /* handle a few faults */
    for (int i = 0; i < 5; i++) {
        ZF_LOGD("Handling fault");
        error = handle_timeout_fault(tfep, server_badge, &server.thread, ro, &cp, ep,
                                     client_data, env);
        test_eq(error, 0);
    }

    return sel4test_get_result();

}
DEFINE_TEST(TIMEOUTFAULT0002, "Handle a timeout fault in a server",
            test_timeout_fault_in_server, config_set(CONFIG_KERNEL_MCS))

static void
timeout_fault_proxy_fn(seL4_CPtr in, seL4_CPtr out, seL4_CPtr ro)
{
    seL4_MessageInfo_t info = seL4_MessageInfo_new(0, 0, 0, 1);
    info = api_nbsend_recv(in, info, in, NULL, ro);
    while (1) {
        info = seL4_Call(out, info);
        api_reply_recv(in, info, NULL, ro);
    }
}

static int test_timeout_fault_nested_servers(env_t env)
{
    helper_thread_t client, server, proxy;
    sel4utils_checkpoint_t proxy_cp, server_cp;

    seL4_Word client_data = 1;
    seL4_Word server_badge = 2;
    seL4_Word proxy_badge = 3;

    seL4_CPtr client_proxy_ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr proxy_server_ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr tfep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr proxy_ro = vka_alloc_reply_leaky(&env->vka);
    seL4_CPtr server_ro = vka_alloc_reply_leaky(&env->vka);

    /* create server */
    int error = create_passive_thread_with_tfep(env, &server, tfep, server_badge,
                                                (helper_fn_t) timeout_fault_server_fn, proxy_server_ep,
                                                (seL4_Word)env, server_ro, 0, &server_cp);
    test_eq(error, 0);

    /* create proxy */
    error = create_passive_thread_with_tfep(env, &proxy, tfep, proxy_badge,
                                            (helper_fn_t) timeout_fault_proxy_fn, client_proxy_ep,
                                            proxy_server_ep, proxy_ro, 0, &proxy_cp);
    test_eq(error, 0);

    /* create client */
    create_helper_thread(env, &client);

    error = api_sched_ctrl_configure(simple_get_sched_ctrl(&env->simple, 0),
                                     client.thread.sched_context.cptr,
                                     100 * US_IN_MS, 101 * US_IN_MS, 0, client_data);
    test_eq(error, 0);

    start_helper(env, &client, (helper_fn_t) timeout_fault_client_fn, client_proxy_ep, 0, 0, 0);

    /* handle some faults */
    for (int i = 0; i < 5; i++) {
        /* server fault */
        ZF_LOGD("server fault\n");
        error = handle_timeout_fault(tfep, server_badge, &server.thread, server_ro, &server_cp,
                                     proxy_server_ep, client_data, env);
        test_eq(error, 0);

        /* proxy fault */
        ZF_LOGD("proxy fault\n");
        error = handle_timeout_fault(tfep, proxy_badge, &proxy.thread, proxy_ro, &proxy_cp,
                                     client_proxy_ep, client_data, env);
        test_eq(error, 0);
    }

    return sel4test_get_result();
}
DEFINE_TEST(TIMEOUTFAULT0003, "Nested timeout fault", test_timeout_fault_nested_servers, config_set(CONFIG_KERNEL_MCS))

static void vm_enter(void)
{
#ifdef CONFIG_VTX
    seL4_VMEnter(NULL);
#endif
}

static int test_vm_enter_non_vm(env_t env)
{
    seL4_Error err;
    helper_thread_t helper;
    seL4_CPtr fault_ep = vka_alloc_endpoint_leaky(&env->vka);
    create_helper_thread(env, &helper);

    seL4_Word guard = seL4_WordBits - env->cspace_size_bits;
    err = api_tcb_set_space(get_helper_tcb(&helper), fault_ep, env->cspace_root,
                            api_make_guard_skip_word(guard),
                            env->page_directory, seL4_NilData);
    test_eq(err, 0);

    seL4_CPtr reply = vka_alloc_reply_leaky(&env->vka);
    start_helper(env, &helper, (helper_fn_t) vm_enter, 0, 0, 0, 0);
    seL4_MessageInfo_t tag = api_recv(fault_ep, NULL, reply);
    test_eq(seL4_MessageInfo_get_label(tag), (seL4_Word) seL4_Fault_UnknownSyscall);
    return sel4test_get_result();
}
DEFINE_TEST(UNKNOWN_SYSCALL_001, "Test seL4_VMEnter in a non-vm thread",
            test_vm_enter_non_vm, config_set(CONFIG_VTX));
