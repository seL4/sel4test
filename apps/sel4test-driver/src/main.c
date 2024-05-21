/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/* Include Kconfig variables. */
#include <autoconf.h>
#include <sel4test-driver/gen_config.h>

#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include <sel4runtime.h>

#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <cpio/cpio.h>

#include <platsupport/local_time_manager.h>

#include <sel4platsupport/timer.h>

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

extern char _cpio_archive[];
extern char _cpio_archive_end[];

static elf_t tests_elf;

/* initialise our runtime environment */
static void init_env(driver_env_t env)
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

    error = sel4platsupport_new_io_ops(&env->vspace, &env->vka, &env->simple, &env->ops);
    ZF_LOGF_IF(error, "Failed to initialise IO ops");
}

/* Free a list of objects */
static void free_objects(vka_object_t *objects, unsigned int num)
{
    for (unsigned int i = 0; i < num; i++) {
        vka_free_object(&env.vka, &objects[i]);
    }
}

/* Allocate untypeds till either a certain number of bytes is allocated
 * or a certain number of untyped objects */
static unsigned int allocate_untypeds(vka_object_t *untypeds, size_t bytes, unsigned int max_untypeds)
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
static unsigned int populate_untypeds(vka_object_t *untypeds)
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

        /* setup the timers and have our wrapper around simple capture the IRQ caps */
        error = ltimer_default_init(&env.ltimer, env.ops, NULL, NULL);
        ZF_LOGF_IF(error, "Failed to setup the timers");

        error = vka_alloc_notification(&env.vka, &env.timer_notify_test);
        ZF_LOGF_IF(error, "Failed to allocate notification object for tests");

        error = seL4_TCB_BindNotification(simple_get_tcb(&env.simple), env.timer_notification.cptr);
        ZF_LOGF_IF(error, "Failed to bind timer notification to sel4test-driver");

        /* set up the timer manager */
        tm_init(&env.tm, &env.ltimer, &env.ops, 1);
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
                         regex_t *reg, int *skipped_tests)
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

void sel4test_run_tests(struct driver_env *e)
{
    /* Iterate through test types. */
    int max_test_types = (int)(__stop__test_type - __start__test_type);
    struct test_type *test_types[max_test_types];
    int num_test_types = 0;
    for (struct test_type *i = __start__test_type; i < __stop__test_type; i++) {
        test_types[num_test_types] = i;
        num_test_types++;
    }

    /* Ensure we iterate through test types in order of ID. */
    qsort(test_types, num_test_types, sizeof(struct test_type *), test_type_comparator);

    /* Count how many tests actually exist and allocate space for them */
    int driver_tests = (int)(__stop__test_case - __start__test_case);
    uint64_t tc_size = 0;
    testcase_t *sel4test_tests = (testcase_t *) sel4utils_elf_get_section(&tests_elf, "_test_case", &tc_size);
    if (sel4test_tests == NULL) {
        ZF_LOGF(TESTS_APP": Failed to find section: _test_case");
    }
    int tc_tests = tc_size / sizeof(testcase_t);
    int all_tests = driver_tests + tc_tests;
    testcase_t *tests[all_tests];

    /* Extract and filter the tests based on the regex */
    regex_t reg;
    int error = regcomp(&reg, CONFIG_TESTPRINTER_REGEX, REG_EXTENDED | REG_NOSUB);
    ZF_LOGF_IF(error, "Error compiling regex \"%s\"", CONFIG_TESTPRINTER_REGEX);

    int skipped_tests = 0;
    /* get all the tests in the test case section in the driver */
    int num_tests = collate_tests(__start__test_case, driver_tests, tests, 0, &reg, &skipped_tests);
    /* get all the tests in the sel4test_tests app */
    num_tests = collate_tests(sel4test_tests, tc_tests, tests, num_tests, &reg, &skipped_tests);

    /* finished with regex */
    regfree(&reg);

    /* Sort the tests to remove any non determinism in test ordering */
    qsort(tests, num_tests, sizeof(testcase_t *), test_comparator);

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
    test_gt(num_tests, 0);
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

    unsigned long elf_size;
    unsigned long cpio_len = _cpio_archive_end - _cpio_archive;
    const void *elf_file = cpio_get_file(_cpio_archive, cpio_len, TESTS_APP, &elf_size);
    ZF_LOGF_IF(elf_file == NULL, "Error: failed to lookup ELF file");
    int status = elf_newFile(elf_file, elf_size, &tests_elf);
    ZF_LOGF_IF(status, "Error: invalid ELF file");

    /* Print welcome banner. */
    printf("\n");
    printf("seL4 Test\n");
    printf("=========\n");
    printf("\n");

    int error;

    /* allocate a piece of device untyped memory for the frame tests,
     * note that spike doesn't have any device untypes so the tests that require device untypes are turned off */
    if (!config_set(CONFIG_PLAT_SPIKE)) {
        bool allocated = false;
        int untyped_count = simple_get_untyped_count(&env.simple);
        for (int i = 0; i < untyped_count; i++) {
            bool device = false;
            uintptr_t ut_paddr = 0;
            size_t ut_size_bits = 0;
            seL4_CPtr ut_cptr = simple_get_nth_untyped(&env.simple, i, &ut_size_bits, &ut_paddr, &device);
            if (device) {
                error = vka_alloc_frame_at(&env.vka, seL4_PageBits, ut_paddr, &env.device_obj);
                if (!error) {
                    allocated = true;
                    /* we've allocated a single device frame and that's all we need */
                    break;
                }
            }
        }
        ZF_LOGF_IF(allocated == false, "Failed to allocate a device frame for the frame tests");
    }

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
    num_elf_regions = sel4utils_elf_num_regions(&tests_elf);
    assert(num_elf_regions <= MAX_REGIONS);
    sel4utils_elf_reserve(NULL, &tests_elf, elf_regions);

    /* copy the region list for the process to clone itself */
    memcpy(env.init->elf_regions, elf_regions, sizeof(sel4utils_elf_region_t) * num_elf_regions);
    env.init->num_elf_regions = num_elf_regions;

    /* setup init data that won't change test-to-test */
    env.init->priority = seL4_MaxPrio - 1;
    if (plat_init) {
        plat_init(&env);
    }

    /* Allocate a reply object for the RT kernel. */
    if (config_set(CONFIG_KERNEL_MCS)) {
        error = vka_alloc_reply(&env.vka, &env.reply);
        ZF_LOGF_IF(error, "Failed to allocate reply");
    }

    /* now run the tests */
    sel4test_run_tests(&env);

    return NULL;
}

/* Note that the following globals are place here because it is not expected that
 * this function be refactored out of sel4test-driver in its current form. */
/* Number of objects to track allocation of. Currently all serial devices are
 * initialised with a single Frame object.  Future devices may need more than 1.
 */
#define NUM_ALLOC_AT_TO_TRACK 1
/* Static global to store the original vka_utspace_alloc_at function. It
 * isn't expected for this to dynamically change after initialisation.*/
static vka_utspace_alloc_at_fn vka_utspace_alloc_at_base;
/* State that serial_utspace_alloc_at_fn uses to determine whether to cache
 * allocations. It is intended that this flag gets set before the serial device
 * is initialised and then unset afterwards. */
static bool serial_utspace_record = false;

typedef struct uspace_alloc_at_args {
    uintptr_t paddr;
    seL4_Word type;
    seL4_Word size_bits;
    cspacepath_t dest;
} uspace_alloc_at_args_t;
/* This instance of vka_utspace_alloc_at_fn will keep a record of allocations up
 * to NUM_ALLOC_AT_TO_TRACK while serial_utspace_record is set. When serial_utspace_record
 * is unset, any allocations matching recorded allocations will instead copy the cap
 * that was originally allocated. These subsequent allocations cannot be freed using
 * vka_utspace_free and instead the caps would have to be manually deleted.
 * Freeing these objects via vka_utspace_free would require also wrapping that function.*/
static int serial_utspace_alloc_at_fn(void *data, const cspacepath_t *dest, seL4_Word type, seL4_Word size_bits,
                                      uintptr_t paddr, seL4_Word *cookie)
{
    static uspace_alloc_at_args_t args_prev[NUM_ALLOC_AT_TO_TRACK] = {};
    static size_t num_alloc = 0;

    ZF_LOGF_IF(!vka_utspace_alloc_at_base, "vka_utspace_alloc_at_base not initialised.");
    if (!serial_utspace_record) {
        for (int i = 0; i < num_alloc; i++) {
            if (paddr == args_prev[i].paddr &&
                type == args_prev[i].type &&
                size_bits == args_prev[i].size_bits) {
                return vka_cnode_copy(dest, &args_prev[i].dest, seL4_AllRights);
            }
        }
        return vka_utspace_alloc_at_base(data, dest, type, size_bits, paddr, cookie);
    } else {
        ZF_LOGF_IF(num_alloc >= NUM_ALLOC_AT_TO_TRACK, "Trying to allocate too many utspace objects");
        int ret = vka_utspace_alloc_at_base(data, dest, type, size_bits, paddr, cookie);
        if (ret) {
            return ret;
        }
        uspace_alloc_at_args_t a = {.paddr = paddr, .type = type, .size_bits = size_bits, .dest = *dest};
        args_prev[num_alloc] = a;
        num_alloc++;
        return ret;

    }
}

static ps_irq_register_fn_t irq_register_fn_copy;

static irq_id_t sel4test_timer_irq_register(UNUSED void *cookie, ps_irq_t irq, irq_callback_fn_t callback,
                                            void *callback_data)
{
    static int num_timer_irqs = 0;

    int error;

    ZF_LOGF_IF(!callback, "Passed in a NULL callback");

    ZF_LOGF_IF(num_timer_irqs >= MAX_TIMER_IRQS, "Trying to register too many timer IRQs");

    /* Allocate the IRQ */
    error = sel4platsupport_copy_irq_cap(&env.vka, &env.simple, &irq,
                                         &env.timer_irqs[num_timer_irqs].handler_path);
    ZF_LOGF_IF(error, "Failed to allocate IRQ handler");

    /* Allocate the root notifitcation if we haven't already done so */
    if (env.timer_notification.cptr == seL4_CapNull) {
        error = vka_alloc_notification(&env.vka, &env.timer_notification);
        ZF_LOGF_IF(error, "Failed to allocate notification object");
    }

    /* Mint a notification for the IRQ handler to pair with */
    error = vka_cspace_alloc_path(&env.vka, &env.badged_timer_notifications[num_timer_irqs]);
    ZF_LOGF_IF(error, "Failed to allocate path for the badged notification");
    cspacepath_t root_notification_path = {0};
    vka_cspace_make_path(&env.vka, env.timer_notification.cptr, &root_notification_path);
    error = vka_cnode_mint(&env.badged_timer_notifications[num_timer_irqs], &root_notification_path,
                           seL4_AllRights, BIT(num_timer_irqs));
    ZF_LOGF_IF(error, "Failed to mint notification for timer");

    /* Pair the notification and the handler */
    error = seL4_IRQHandler_SetNotification(env.timer_irqs[num_timer_irqs].handler_path.capPtr,
                                            env.badged_timer_notifications[num_timer_irqs].capPtr);
    ZF_LOGF_IF(error, "Failed to pair the notification and handler together");

    /* Ack the handler so interrupts can come in */
    error = seL4_IRQHandler_Ack(env.timer_irqs[num_timer_irqs].handler_path.capPtr);
    ZF_LOGF_IF(error, "Failed to ack the IRQ handler");

    /* Fill out information about the callbacks */
    env.timer_cbs[num_timer_irqs].callback = callback;
    env.timer_cbs[num_timer_irqs].callback_data = callback_data;

    return num_timer_irqs++;
}

/* When the root task exists, it should simply suspend itself */
static void sel4test_exit(int code)
{
    seL4_TCB_Suspend(seL4_CapInitThreadTCB);
}

int main(void)
{
    /* Set exit handler */
    sel4runtime_set_exit(sel4test_exit);

    int error;
    seL4_BootInfo *info = platsupport_get_bootinfo();

#ifdef CONFIG_DEBUG_BUILD
    seL4_DebugNameThread(seL4_CapInitThreadTCB, "sel4test-driver");
#endif

    /* initialise libsel4simple, which abstracts away which kernel version
     * we are running on */
    simple_default_init_bootinfo(&env.simple, info);

    /* initialise the test environment - allocator, cspace manager, vspace
     * manager, timer
     */
    init_env(&env);

    /* Partially overwrite part of the VKA implementation to cache objects. We need to
     * create this wrapper as the actual vka implementation will only
     * allocate/return any given device frame once.
     * We allocate serial objects for initialising the serial server in
     * platsupport_serial_setup_simple but then we also need to use the objects
     * in some of the tests but attempts to allocate will fail.
     * Instead, this wrapper records the initial serial object allocations and
     * then returns a copy of the one already allocated for future allocations.
     * This requires the allocations for the serial driver to exist for the full
     * lifetime of this application, and for the resources that are allocated to
     * be able to be copied, I.E. frames.
     */
    vka_utspace_alloc_at_base = env.vka.utspace_alloc_at;
    env.vka.utspace_alloc_at = serial_utspace_alloc_at_fn;

    /* enable serial driver */
    serial_utspace_record = true;
    platsupport_serial_setup_simple(&env.vspace, &env.simple, &env.vka);
    serial_utspace_record = false;

    /* Partially overwrite the IRQ interface so that we can record the IRQ caps that were allocated.
     * We need this only for the timer as the ltimer interfaces allocates the caps for us and hides them away.
     * A few of the tests require actual interactions with the caps hence we record them.
     */
    irq_register_fn_copy = env.ops.irq_ops.irq_register_fn;
    env.ops.irq_ops.irq_register_fn = sel4test_timer_irq_register;
    /* Initialise ltimer */
    init_timer();
    /* Restore the IRQ interface's register function */
    env.ops.irq_ops.irq_register_fn = irq_register_fn_copy;

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
