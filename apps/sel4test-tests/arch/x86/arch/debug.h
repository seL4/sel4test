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
#pragma once

#define TEST_SOFTWARE_BREAK_ASM() \
    asm volatile( \
        ".att_syntax\n\t" \
        ".global post_sbreak\n\t" \
        ".type post_sbreak, @function\n\t" \
        "int $3\n\t" \
        "post_sbreak:\n\t")

/* Tell C about the symbols exported by the ASM above. */
extern char post_sbreak;
#define TEST_SOFTWARE_BREAK_EXPECTED_FAULT_LABEL post_sbreak
#define SINGLESTEP_EXPECTED_BP_CONSUMPTION_VALUE (false)
#define TEST_NUM_DATA_WPS seL4_NumDualFunctionMonitors
#define TEST_NUM_INSTR_BPS seL4_NumDualFunctionMonitors
#define TEST_FIRST_DATA_WP seL4_FirstDualFunctionMonitor
#define TEST_FIRST_INSTR_BP seL4_FirstDualFunctionMonitor
