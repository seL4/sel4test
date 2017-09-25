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

/* Include Kconfig variables. */
#include <autoconf.h>

#include <stdbool.h>

#include <sel4/sel4.h>
#include <sel4test/test.h>

/*
 * Get a testcase.
 *
 * @param name the name of the test to retrieve.
 * @return the test corresponding to name, NULL if test not found.
 */
testcase_t* sel4test_get_test(const char *name);

