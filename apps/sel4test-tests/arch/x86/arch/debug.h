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

#define TEST_SOFTWARE_BREAK_ASM() asm volatile("int $3\n\t")
#define SINGLESTEP_EXPECTED_BP_CONSUMPTION_VALUE (false)
#define TEST_NUM_DATA_WPS seL4_NumDualFunctionMonitors
#define TEST_NUM_INSTR_BPS seL4_NumDualFunctionMonitors
#define TEST_FIRST_DATA_WP seL4_FirstDualFunctionMonitor
#define TEST_FIRST_INSTR_BP seL4_FirstDualFunctionMonitor

