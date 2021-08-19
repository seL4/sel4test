/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

/* Include Kconfig variables. */
#include <stdbool.h>

#include <sel4/sel4.h>
#include <sel4test/test.h>

/*
 * Get a testcase.
 *
 * @param name the name of the test to retrieve.
 * @return the test corresponding to name, NULL if test not found.
 */
testcase_t *sel4test_get_test(const char *name);

