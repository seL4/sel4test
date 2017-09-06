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

#define SUCCESS true
#define FAILURE false

/*
 * Example basic run test
 */
static inline int
sel4test_basic_run_test(struct testcase *t)
{
    return t->function(sel4test_get_env());
}

/*
 * Run every test defined with the DEFINE_TEST macro.
 *
 * Use CONFIG_TESTPRINTER_REGEX to filter tests.
 *
 * @param name the name of the test suite
 *
 */
void sel4test_run_tests(const char *name, env_t e);

/*
 * Get a testcase.
 *
 * @param name the name of the test to retrieve.
 * @return the test corresponding to name, NULL if test not found.
 */
testcase_t* sel4test_get_test(const char *name);

/**
 * Start a test suite
 *
 * @name name of test suite
 */
void sel4test_start_suite(const char *name);

/**
 * End test suite
 */
void sel4test_end_suite(int num_tests, int num_tests_passed);

/**
 * Start a test case.
 *
 * @param name the name of the test
 */
void sel4test_start_test(const char *name, int num_tests);

/*
 * End the current test case
 */
void sel4test_end_test(bool current_test_passed);

