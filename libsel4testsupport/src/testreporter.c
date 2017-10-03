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

#include <serial_server/test.h>

/* Force the _test_type and _test_case section to be created even if no tests are defined. */
static USED SECTION("_test_type") struct {} dummy_test_type;
static USED SECTION("_test_case") struct {} dummy_test_case;

/* Used to ensure that serial server parent tests are included */
UNUSED void dummy_func()
{
    get_serial_server_parent_tests();
}

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

