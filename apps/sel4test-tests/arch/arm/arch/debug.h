/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#ifdef CONFIG_ARCH_AARCH32
#define TEST_SOFTWARE_BREAK_ASM() \
    asm volatile( \
        ".global sbreak, post_sbreak\n\t" \
        ".type post_sbreak, function\n\t" \
        "sbreak:\n\t" \
        "bkpt\n\t")
#elif CONFIG_ARCH_AARCH64
#define TEST_SOFTWARE_BREAK_ASM() \
    asm volatile( \
        ".global sbreak, post_sbreak\n\t" \
        ".type post_sbreak, function\n\t" \
        "sbreak:\n\t" \
        "brk #0\n\t")
#endif

/* Tell C about the symbols exported by the ASM above. */
extern char sbreak;
#define TEST_SOFTWARE_BREAK_EXPECTED_FAULT_LABEL sbreak
#define SINGLESTEP_EXPECTED_BP_CONSUMPTION_VALUE (true)
#define TEST_NUM_DATA_WPS seL4_NumExclusiveWatchpoints
#define TEST_NUM_INSTR_BPS seL4_NumExclusiveBreakpoints
#define TEST_FIRST_DATA_WP seL4_FirstWatchpoint
#define TEST_FIRST_INSTR_BP seL4_FirstBreakpoint
