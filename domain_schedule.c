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

/* This is a domain schedule that is suitable for the domains tests in sel4test. All
 * sel4test actually needs is for every domain to be executable for some period of time
 * in order for the tests to make progress
 */

/* remember that this is compiled as part of the kernel, and so is referencing kernel headers */

#include <config.h>
#include <object/structures.h>
#include <model/statedata.h>

/* Default schedule. */
const dschedule_t ksDomSchedule[] = {
    { .domain = 0, .length = 1 },
#if CONFIG_NUM_DOMAINS > 1
    { .domain = 1, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 2
    { .domain = 2, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 3
    { .domain = 3, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 4
    { .domain = 4, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 5
    { .domain = 5, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 6
    { .domain = 6, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 7
    { .domain = 7, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 8
    { .domain = 8, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 9
    { .domain = 9, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 10
    { .domain = 10, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 11
    { .domain = 11, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 12
    { .domain = 12, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 13
    { .domain = 13, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 14
    { .domain = 14, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 15
    { .domain = 15, .length = 1 },
#endif
#if CONFIG_NUM_DOMAINS > 16
#error Unsupportd number of domains set
#endif
};

const word_t ksDomScheduleLength = sizeof(ksDomSchedule) / sizeof(dschedule_t);
