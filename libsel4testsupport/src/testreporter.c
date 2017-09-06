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

#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sel4testsupport/testreporter.h>
#include <sel4test/testutil.h>
#include <sel4test/macros.h>

#include <utils/util.h>

#define MAX_NAME_SIZE 100

#ifdef CONFIG_PRINT_XML
static char sel4test_suitename[MAX_NAME_SIZE];
#endif

void
sel4test_start_suite(const char *name)
{
#ifdef CONFIG_PRINT_XML
    sel4test_reset_buffer_index();
    printf("<testsuite>\n");
    strncpy(sel4test_suitename, name, MAX_NAME_SIZE);
    /* NULL terminate sel4test_suitename in case name was too long. */
    sel4test_suitename[MAX_NAME_SIZE - 1] = '\0';
#else
    printf("Starting test suite %s\n", name);
#endif /* CONFIG_PRINT_XML */
}

void
sel4test_end_suite(int num_tests, int num_tests_passed)
{
#ifdef CONFIG_PRINT_XML
    printf("</testsuite>\n");
    sel4test_disable_buffering();
#else
    if (num_tests_passed != num_tests) {
        printf("Test suite failed. %d/%d tests passed.\n", num_tests_passed, num_tests);
    } else {
        printf("Test suite passed. %d tests passed.\n", num_tests);
    }
#endif /* CONFIG_PRINT_XML */
}

void
sel4test_start_test(const char *name, int num_tests)
{
#ifdef CONFIG_BUFFER_OUTPUT
    sel4test_reset_buffer_index();
    sel4test_clear_buffer();
#endif /* CONFIG_BUFFER_OUTPUT */
#ifdef CONFIG_PRINT_XML
    printf("\t<testcase classname=\"%s\" name=\"%s\">\n", sel4test_suitename, name);
#else
    printf("Starting test %d: %s\n", num_tests, name);
#endif /* CONFIG_PRINT_XML */
}

void
sel4test_end_test(bool current_test_passed)
{
    if (!current_test_passed) {
        _sel4test_failure("Test check failure (see log).", "", 0);
    }
#ifdef CONFIG_BUFFER_OUTPUT
    sel4test_print_buffer();
#endif /* CONFIG_BUFFER_OUTPUT */
#ifdef CONFIG_PRINT_XML
    printf("\t</testcase>\n");
#endif /* CONFIG_PRINT_XML */
}

/* Definitions so that we can find the test types */
extern struct test_type __start__test_type[];
extern struct test_type __stop__test_type[];

/* Definitions so that we can find the test cases */
extern struct testcase __start__test_case[];
extern struct testcase __stop__test_case[];

/* Force the _test_type and _test_case section to be created even if no tests are defined. */
static USED SECTION("_test_type") struct {} dummy_test_type;
static USED SECTION("_test_case") struct {} dummy_test_case;

testcase_t*
sel4test_get_test(const char *name)
{

    for (testcase_t *t = __start__test_case; t < __stop__test_case; t++) {
        if (strcmp(name, t->name) == 0) {
            return t;
        }
    }

    return NULL;
}

void
sel4test_run_tests(const char *name, struct env* e)
{
    /* Iterate through test types. */
    int max_test_types = (int) (__stop__test_type - __start__test_type);
    struct test_type *test_types[max_test_types];
    int num_test_types = 0;
    for (struct test_type *i = __start__test_type; i < __stop__test_type; i++) {
        test_types[num_test_types] = i;
        num_test_types++;
    }

    /* Ensure we iterate through test types in order of ID. */
    qsort(test_types, num_test_types, sizeof(struct test_type*), test_type_comparator);

    /* Count how many tests actually exist and allocate space for them */
    int max_tests = (int)(__stop__test_case - __start__test_case);
    struct testcase *tests[max_tests];

    /* Extract and filter the tests based on the regex */
    regex_t reg;
    int error = regcomp(&reg, CONFIG_TESTPRINTER_REGEX, REG_EXTENDED | REG_NOSUB);
    if (error != 0) {
        printf("Error compiling regex \"%s\"\n", CONFIG_TESTPRINTER_REGEX);
        return;
    }

    int num_tests = 0;
    for (struct testcase *i = __start__test_case; i < __stop__test_case; i++) {
        if (regexec(&reg, i->name, 0, NULL, 0) == 0) {
            tests[num_tests] = i;
            num_tests++;
        }
    }

    regfree(&reg);

    /* Sort the tests to remove any non determinism in test ordering */
    qsort(tests, num_tests, sizeof(struct testcase*), test_comparator);

    /* Now that they are sorted we can easily ensure there are no duplicate tests.
     * this just ensures some sanity as if there are duplicates, they could have some
     * arbitrary ordering, which might result in difficulty reproducing test failures */
    for (int i = 1; i < num_tests; i++) {
        test_assert_fatal(strcmp(tests[i]->name, tests[i - 1]->name) != 0);
    }

    /* Check that we don't miss any tests because of an undeclared test type */
    int tests_done = 0;
    int tests_failed = 0;

    sel4test_start_suite(name);
    /* Iterate through test types so that we run them in order of test type, then name.
     * Test types are ordered by ID in test.h. */
    for (int tt = 0; tt < num_test_types; tt++) {
        if (test_types[tt]->set_up_test_type != NULL) {
            test_types[tt]->set_up_test_type(e);
        }
        /* Run tests */
        test_assert_fatal(num_tests > 0);
        for (int i = 0; i < num_tests; i++) {
            if (tests[i]->test_type == test_types[tt]->id) {
                _sel4test_start_new_test();

                sel4test_start_test(tests[i]->name, num_tests);
                if (test_types[tt]->set_up != NULL) {
                    test_types[tt]->set_up(e);
                }

                int result = test_types[tt]->run_test(tests[i], e);
                if (result != SUCCESS) {
                    tests_failed++;

                    if (is_aborted()) {
                        printf("Halting on fatal assertion...\n");
                        sel4test_end_test(sel4test_get_result());
                        sel4test_end_suite(tests_done, tests_done - tests_failed);
#ifdef CONFIG_DEBUG_BUILD
                        seL4_DebugHalt();
#endif /* CONFIG_DEBUG_BUILD */
                        while (1);
                    }

#ifdef CONFIG_TESTPRINTER_HALT_ON_TEST_FAILURE
                    printf("Halting on first test failure...\n");
                    sel4test_end_test(sel4test_get_result());
                    sel4test_end_suite(tests_done, tests_done - tests_failed);
#ifdef CONFIG_DEBUG_BUILD
                    seL4_DebugHalt();
#endif /* CONFIG_DEBUG_BUILD */
                    while (1);
#endif /* CONFIG_TESTPRINTER_HALT_ON_TEST_FAILURE */
                }

                if (test_types[tt]->tear_down != NULL) {
                    test_types[tt]->tear_down(e);
                }

                sel4test_end_test(sel4test_get_result());
                tests_done++;
            }
        }

        if (test_types[tt]->tear_down_test_type != NULL) {
            test_types[tt]->tear_down_test_type(e);
        }
    }
    sel4test_end_suite(tests_done, tests_done - tests_failed);

    test_assert_fatal(tests_done == num_tests);

    /* Print closing banner. */
    printf("\n");
    printf("%d/%d tests passed.\n", tests_done - tests_failed, tests_done);
    if (tests_failed > 0) {
        printf("*** FAILURES DETECTED ***\n");
    } else {
        printf("All is well in the universe.\n");
    }
    printf("\n\n");
}
