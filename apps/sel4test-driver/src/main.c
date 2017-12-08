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

/* Include Kconfig variables. */
#include <autoconf.h>

#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <platsupport/local_time_manager.h>

#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/serial.h>

#include <sel4debug/register_dump.h>
#include <sel4platsupport/device.h>
#include <sel4platsupport/platsupport.h>
#include <sel4utils/vspace.h>
#include <sel4utils/stack.h>
#include <sel4utils/process.h>
#include <sel4test/test.h>

#include <simple/simple.h>
#include <simple-default/simple-default.h>

#include <utils/util.h>

#include <vka/object.h>
#include <vka/capops.h>

#include <vspace/vspace.h>
#include "test.h"
#include "timer.h"

#include <sel4platsupport/io.h>

/* ammount of untyped memory to reserve for the driver (32mb) */
#define DRIVER_UNTYPED_MEMORY (1 << 25)
/* Number of untypeds to try and use to allocate the driver memory.
 * if we cannot get 32mb with 16 untypeds then something is probably wrong */
#define DRIVER_NUM_UNTYPEDS 16

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE ((1 << seL4_PageBits) * 100)

/* static memory for the allocator to bootstrap with */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 20)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* static memory for virtual memory bootstrapping */
static sel4utils_alloc_data_t data;

/* environment encapsulating allocation interfaces etc */
struct driver_env env;
/* list of untypeds to give out to test processes */
static vka_object_t untypeds[CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS];
/* list of sizes (in bits) corresponding to untyped */
static uint8_t untyped_size_bits_list[CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS];

/* initialise our runtime environment */
static void
init_env(driver_env_t env)
{
    allocman_t *allocman;
    reservation_t virtual_reservation;
    int error;

    /* create an allocator */
    allocman = bootstrap_use_current_simple(&env->simple, ALLOCATOR_STATIC_POOL_SIZE, allocator_mem_pool);
    if (allocman == NULL) {
        ZF_LOGF("Failed to create allocman");
    }

    /* create a vka (interface for interacting with the underlying allocator) */
    allocman_make_vka(&env->vka, allocman);

    /* create a vspace (virtual memory management interface). We pass
     * boot info not because it will use capabilities from it, but so
     * it knows the address and will add it as a reserved region */
    error = sel4utils_bootstrap_vspace_with_bootinfo_leaky(&env->vspace,
                                                           &data, simple_get_pd(&env->simple),
                                                           &env->vka, platsupport_get_bootinfo());
    if (error) {
        ZF_LOGF("Failed to bootstrap vspace");
    }

    /* fill the allocator with virtual memory */
    void *vaddr;
    virtual_reservation = vspace_reserve_range(&env->vspace,
                                               ALLOCATOR_VIRTUAL_POOL_SIZE, seL4_AllRights, 1, &vaddr);
    if (virtual_reservation.res == 0) {
        ZF_LOGF("Failed to provide virtual memory for allocator");
    }

    bootstrap_configure_virtual_pool(allocman, vaddr,
                                     ALLOCATOR_VIRTUAL_POOL_SIZE, simple_get_pd(&env->simple));


    error = sel4platsupport_new_io_mapper(env->vspace, env->vka, &env->ops.io_mapper);
    ZF_LOGF_IF(error, "Failed to initialise IO mapper");

    error = sel4platsupport_new_malloc_ops(&env->ops.malloc_ops);
    ZF_LOGF_IF(error, "Failed to malloc new ops");
}

/* Free a list of objects */
static void
free_objects(vka_object_t *objects, unsigned int num)
{
    for (unsigned int i = 0; i < num; i++) {
        vka_free_object(&env.vka, &objects[i]);
    }
}

/* Allocate untypeds till either a certain number of bytes is allocated
 * or a certain number of untyped objects */
static unsigned int
allocate_untypeds(vka_object_t *untypeds, size_t bytes, unsigned int max_untypeds)
{
    unsigned int num_untypeds = 0;
    size_t allocated = 0;

    /* try to allocate as many of each possible untyped size as possible */
    for (uint8_t size_bits = seL4_WordBits - 1; size_bits > PAGE_BITS_4K; size_bits--) {
        /* keep allocating until we run out, or if allocating would
         * cause us to allocate too much memory*/
        while (num_untypeds < max_untypeds &&
                allocated + BIT(size_bits) <= bytes &&
                vka_alloc_untyped(&env.vka, size_bits, &untypeds[num_untypeds]) == 0) {
            allocated += BIT(size_bits);
            num_untypeds++;
        }
    }
    return num_untypeds;
}

/* extract a large number of untypeds from the allocator */
static unsigned int
populate_untypeds(vka_object_t *untypeds)
{
    /* First reserve some memory for the driver */
    vka_object_t reserve[DRIVER_NUM_UNTYPEDS];
    unsigned int reserve_num = allocate_untypeds(reserve, DRIVER_UNTYPED_MEMORY, DRIVER_NUM_UNTYPEDS);

    /* Now allocate everything else for the tests */
    unsigned int num_untypeds = allocate_untypeds(untypeds, UINT_MAX, ARRAY_SIZE(untyped_size_bits_list));
    /* Fill out the size_bits list */
    for (unsigned int i = 0; i < num_untypeds; i++) {
        untyped_size_bits_list[i] = untypeds[i].size_bits;
    }

    /* Return reserve memory */
    free_objects(reserve, reserve_num);

    /* Return number of untypeds for tests */
    if (num_untypeds == 0) {
        ZF_LOGF("No untypeds for tests!");
    }

    return num_untypeds;
}

static void init_timer(void)
{
    if (config_set(CONFIG_HAVE_TIMER)) {
        int error;

        error = vka_alloc_notification(&env.vka, &env.timer_notification);
        ZF_LOGF_IF(error, "Failed to allocate notification object");

        error = vka_alloc_notification(&env.vka, &env.timer_notify_test);
        ZF_LOGF_IF(error, "Failed to allocate notification object for tests");

        error = sel4platsupport_init_default_timer(&env.vka, &env.vspace, &env.simple,
                  env.timer_notification.cptr, &env.timer);
        ZF_LOGF_IF(error, "Failed to initialise default timer");

        error = seL4_TCB_BindNotification(simple_get_tcb(&env.simple), env.timer_notification.cptr);
        ZF_LOGF_IF(error, "Failed to bind timer notification to sel4test-driver\n");

        /* set up the timer manager */
        tm_init(&env.tm, &env.timer.ltimer, &env.ops, 1);
    }

}

void sel4test_start_suite(const char *name)
{
    if (config_set(CONFIG_PRINT_XML)) {
        printf("<testsuite>\n");
    } else {
        printf("Starting test suite %s\n", name);
    }
}

void sel4test_start_test(const char *name, int n)
{
    if (config_set(CONFIG_PRINT_XML)) {
        printf("\t<testcase classname=\"%s\" name=\"%s\">\n", "sel4test", name);
    } else {
        printf("Starting test %d: %s\n", n, name);
    }
    sel4test_reset();
    sel4test_start_printf_buffer();
}

void sel4test_end_test(test_result_t result)
{
    sel4test_end_printf_buffer();
    test_check(result == SUCCESS);

    if (config_set(CONFIG_PRINT_XML)) {
        printf("\t</testcase>\n");
    }

    if (config_set(CONFIG_HAVE_TIMER)) {
        timer_reset(&env);
    }
}

void sel4test_end_suite(int num_tests, int num_tests_passed, int skipped_tests)
{
    if (config_set(CONFIG_PRINT_XML)) {
        printf("</testsuite>\n");
    } else {
        if (num_tests_passed != num_tests) {
            printf("Test suite failed. %d/%d tests passed.\n", num_tests_passed, num_tests);
        } else {
            printf("Test suite passed. %d tests passed. %d tests disabled.\n", num_tests, skipped_tests);
        }
    }
}

void sel4test_stop_tests(test_result_t result, int tests_done, int tests_failed, int num_tests, int skipped_tests)
{
    /* if its a special abort case, output why we are aborting */
    switch (result) {
    case ABORT:
        printf("Halting on fatal assertion...\n");
        break;
    case FAILURE:
        assert(config_set(CONFIG_TESTPRINTER_HALT_ON_TEST_FAILURE));
        printf("Halting on first test failure\n");
        break;
    default:
        /* nothing to output if its successful */
        break;
    }

    /* last test - test all tests ran */
    sel4test_start_test("Test all tests ran", num_tests + 1);
    test_eq(tests_done, num_tests);
    if (sel4test_get_result() != SUCCESS) {
        tests_failed++;
    }
    tests_done++;
    num_tests++;
    sel4test_end_test(sel4test_get_result());

    sel4test_end_suite(tests_done, tests_done - tests_failed, skipped_tests);

    if (tests_failed > 0) {
        printf("*** FAILURES DETECTED ***\n");
    } else if (tests_done < num_tests) {
        printf("*** ALL tests not run ***\n");
    } else {
        printf("All is well in the universe\n");
    }
    printf("\n\n");
}

static int collate_tests(testcase_t *tests_in, int n, testcase_t *tests_out[], int out_index,
                                  regex_t *reg, int* skipped_tests)
{
    for (int i = 0; i < n; i++) {
        /* make sure the string is null terminated */
        tests_in[i].name[TEST_NAME_MAX - 1] = '\0';
        if (regexec(reg, tests_in[i].name, 0, NULL, 0) == 0) {
            if (tests_in[i].enabled) {
                tests_out[out_index] = &tests_in[i];
                out_index++;
            } else {
                (*skipped_tests)++;
            }
        }
    }

    return out_index;
}

void sel4test_run_tests(struct driver_env* e)
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
    int driver_tests = (int)(__stop__test_case - __start__test_case);
    uint64_t tc_size;
    testcase_t *sel4test_tests = (testcase_t *) sel4utils_elf_get_section("sel4test-tests", "_test_case", &tc_size);
    int tc_tests = tc_size / sizeof(testcase_t);
    int all_tests = driver_tests + tc_tests;
    testcase_t *tests[all_tests];

    /* Extract and filter the tests based on the regex */
    regex_t reg;
    int error = regcomp(&reg, CONFIG_TESTPRINTER_REGEX, REG_EXTENDED | REG_NOSUB);
    ZF_LOGF_IF(error, "Error compiling regex \"%s\"\n", CONFIG_TESTPRINTER_REGEX);

    int skipped_tests = 0;
    /* get all the tests in the test case section in the driver */
    int num_tests = collate_tests(__start__test_case, driver_tests, tests, 0, &reg, &skipped_tests);
    /* get all the tests in the sel4test_tests app */
    num_tests = collate_tests(sel4test_tests, tc_tests, tests, num_tests, &reg, &skipped_tests);

    /* finished with regex */
    regfree(&reg);

    /* Sort the tests to remove any non determinism in test ordering */
    qsort(tests, num_tests, sizeof(testcase_t*), test_comparator);

    /* Now that they are sorted we can easily ensure there are no duplicate tests.
     * this just ensures some sanity as if there are duplicates, they could have some
     * arbitrary ordering, which might result in difficulty reproducing test failures */
    for (int i = 1; i < num_tests; i++) {
        ZF_LOGF_IF(strcmp(tests[i]->name, tests[i - 1]->name) == 0, "tests have no strict order! %s %s",
                   tests[i]->name, tests[i - 1]->name);
    }

    /* Check that we don't miss any tests because of an undeclared test type */
    int tests_done = 0;
    int tests_failed = 0;

    sel4test_start_suite("sel4test");
    /* First: test that there are tests to run */
    sel4test_start_test("Test that there are tests", tests_done);
    test_geq(num_tests, 0);
    sel4test_end_test(sel4test_get_result());
    tests_done++;

    /* Iterate through test types so that we run them in order of test type, then name.
       * Test types are ordered by ID in test.h. */
    for (int tt = 0; tt < num_test_types; tt++) {
        /* set up */
        if (test_types[tt]->set_up_test_type != NULL) {
            test_types[tt]->set_up_test_type((uintptr_t)e);
        }

        for (int i = 0; i < num_tests; i++) {
            if (tests[i]->test_type == test_types[tt]->id) {
                sel4test_start_test(tests[i]->name, tests_done);
                if (test_types[tt]->set_up != NULL) {
                    test_types[tt]->set_up((uintptr_t)e);
                }

                test_result_t result = test_types[tt]->run_test(tests[i], (uintptr_t)e);

                if (test_types[tt]->tear_down != NULL) {
                    test_types[tt]->tear_down((uintptr_t)e);
                }
                sel4test_end_test(result);

                if (result != SUCCESS) {
                    tests_failed++;
                    if (config_set(CONFIG_TESTPRINTER_HALT_ON_TEST_FAILURE) || result == ABORT) {
                        sel4test_stop_tests(result, tests_done + 1, tests_failed, num_tests + 1, skipped_tests);
                        return;
                    }
                }
                tests_done++;
            }
        }

        /* tear down */
        if (test_types[tt]->tear_down_test_type != NULL) {
            test_types[tt]->tear_down_test_type((uintptr_t)e);
        }
    }

    /* and we're done */
    sel4test_stop_tests(SUCCESS, tests_done, tests_failed, num_tests + 1, skipped_tests);
}

void *main_continued(void *arg UNUSED)
{

    /* elf region data */
    int num_elf_regions;
    sel4utils_elf_region_t elf_regions[MAX_REGIONS];

    /* Print welcome banner. */
    printf("\n");
    printf("seL4 Test\n");
    printf("=========\n");
    printf("\n");

    /* allocate lots of untyped memory for tests to use */
    env.num_untypeds = populate_untypeds(untypeds);
    env.untypeds = untypeds;

    /* create a frame that will act as the init data, we can then map that
     * in to target processes */
    env.init = (test_init_data_t *) vspace_new_pages(&env.vspace, seL4_AllRights, 1, PAGE_BITS_4K);
    assert(env.init != NULL);

    /* copy the untyped size bits list across to the init frame */
    memcpy(env.init->untyped_size_bits_list, untyped_size_bits_list, sizeof(uint8_t) * env.num_untypeds);

    /* parse elf region data about the test image to pass to the tests app */
    num_elf_regions = sel4utils_elf_num_regions(TESTS_APP);
    assert(num_elf_regions < MAX_REGIONS);
    sel4utils_elf_reserve(NULL, TESTS_APP, elf_regions);

    /* copy the region list for the process to clone itself */
    memcpy(env.init->elf_regions, elf_regions, sizeof(sel4utils_elf_region_t) * num_elf_regions);
    env.init->num_elf_regions = num_elf_regions;

    /* setup init data that won't change test-to-test */
    env.init->priority = seL4_MaxPrio - 1;
    plat_init(&env);

    /* Allocate a reply object for the RT kernel. */
    if (config_set(CONFIG_KERNEL_RT)) {
        int error = vka_alloc_reply(&env.vka, &env.reply);
        ZF_LOGF_IF(error, "Failed to allocate reply");
    }

    /* now run the tests */
    sel4test_run_tests(&env);

    return NULL;
}

int main(void)
{
    int error;
    seL4_BootInfo *info = platsupport_get_bootinfo();

#ifdef CONFIG_DEBUG_BUILD
    seL4_DebugNameThread(seL4_CapInitThreadTCB, "sel4test-driver");
#endif

    compile_time_assert(init_data_fits_in_ipc_buffer, sizeof(test_init_data_t) < PAGE_SIZE_4K);
    /* initialise libsel4simple, which abstracts away which kernel version
     * we are running on */
    simple_default_init_bootinfo(&env.simple, info);

    /* initialise the test environment - allocator, cspace manager, vspace
     * manager, timer
     */
    init_env(&env);

    /* Allocate slots for, and obtain the caps for, the hardware we will be
     * using, in the same function.
     */
    sel4platsupport_init_default_serial_caps(&env.vka, &env.vspace, &env.simple, &env.serial_objects);

    /* Construct a vka wrapper for returning the serial frame. We need to
     * create this wrapper as the actual vka implementation will only
     * allocate/return any given device frame once. As we already allocated it
     * in init_serial_caps when we the platsupport_serial_setup_simple attempts
     * to allocate it will fail. This wrapper just returns a copy of the one
     * we already allocated, whilst passing all other requests on to the
     * actual vka
     */
    vka_t serial_vka = env.vka;
    serial_vka.utspace_alloc_at = arch_get_serial_utspace_alloc_at(&env);

    /* enable serial driver */
    platsupport_serial_setup_simple(&env.vspace, &env.simple, &serial_vka);

    /* Initialise ltimer */
    init_timer();

    simple_print(&env.simple);

    /* switch to a bigger, safer stack with a guard page
     * before starting the tests */
    printf("Switching to a safer, bigger stack... ");
    fflush(stdout);
    void *res;

    /* Run sel4test-test related tests */
    error = sel4utils_run_on_stack(&env.vspace, main_continued, NULL, &res);
    test_assert_fatal(error == 0);
    test_assert_fatal(res == 0);

    return 0;
}
