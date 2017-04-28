/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/* Include Kconfig variables. */
#include <autoconf.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include <allocman/bootstrap.h>
#include <allocman/vka.h>

#include <sel4platsupport/plat/timer.h>
#include <sel4platsupport/plat/serial.h>

#include <sel4debug/register_dump.h>
#include <sel4platsupport/device.h>
#include <sel4platsupport/platsupport.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4platsupport/timer.h>
#include <sel4utils/vspace.h>
#include <sel4utils/stack.h>
#include <sel4utils/process.h>

#include <simple/simple.h>
#include <simple-default/simple-default.h>

#include <utils/util.h>

#include <vka/object.h>
#include <vka/capops.h>

#include <vspace/vspace.h>

#include "test.h"

#define TESTS_APP "sel4test-tests"

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
static struct env env;
/* the number of untyped objects we have to give out to processes */
static int num_untypeds;
/* list of untypeds to give out to test processes */
static vka_object_t untypeds[CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS];
/* list of sizes (in bits) corresponding to untyped */
static uint8_t untyped_size_bits_list[CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS];

/* initialise our runtime environment */
static void
init_env(env_t env)
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
}


/* copy a cap to a process, returning the cptr in the process' cspace */
seL4_CPtr
copy_cap_to_process(sel4utils_process_t *process, seL4_CPtr cap)
{
    seL4_CPtr copied_cap;
    cspacepath_t path;

    vka_cspace_make_path(&env.vka, cap, &path);
    copied_cap = sel4utils_copy_cap_to_process(process, path);
    if (copied_cap == 0) {
        ZF_LOGF("Failed to copy cap to process");
    }

    return copied_cap;
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

/* copy untyped caps into a processes cspace, return the cap range they can be found in */
static seL4_SlotRegion
copy_untypeds_to_process(sel4utils_process_t *process, vka_object_t *untypeds, int num_untypeds)
{
    seL4_SlotRegion range = {0};

    for (int i = 0; i < num_untypeds; i++) {
        seL4_CPtr slot = copy_cap_to_process(process, untypeds[i].cptr);

        /* set up the cap range */
        if (i == 0) {
            range.start = slot;
        }
        range.end = slot;
    }
    assert((range.end - range.start) + 1 == num_untypeds);
    return range;
}

/* map the init data into the process, and send the address via ipc */
static void *
send_init_data(env_t env, seL4_CPtr endpoint, sel4utils_process_t *process)
{
    /* map the cap into remote vspace */
    void *remote_vaddr = vspace_map_pages(&process->vspace, &env->init_frame_cap_copy, NULL, seL4_AllRights, 1, PAGE_BITS_4K, 1);
    assert(remote_vaddr != 0);

    /* now send a message telling the process what address the data is at */
    seL4_MessageInfo_t info = seL4_MessageInfo_new(seL4_Fault_NullFault, 0, 0, 1);
    seL4_SetMR(0, (seL4_Word) remote_vaddr);
    seL4_Send(endpoint, info);

    return remote_vaddr;
}

/* copy the caps required to set up the sel4platsupport default timer */
static void
copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    /* Copy PS default timer's IRQ cap to child process. */
    init->timer_irq_cap = copy_cap_to_process(test_process, env->timer_irq_path.capPtr);
    ZF_LOGF_IF(init->timer_irq_cap == 0,
               "Failed to copy PS default timer IRQ cap to test child "
               "process.");

    /* untyped cap for timer device untyped */
    init->timer_paddr = env->timer_paddr;
    init->timer_dev_ut_cap = copy_cap_to_process(test_process, env->timer_dev_ut_obj.cptr);
    ZF_LOGF_IF(init->timer_dev_ut_cap == 0,
               "Failed to copy PS default timer device-ut to test child.");

    arch_copy_timer_caps(init, env, test_process);
}

static void
copy_serial_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    init->serial_irq_cap = copy_cap_to_process(test_process,
                                               env->serial_irq_path.capPtr);
    ZF_LOGF_IF(init->serial_irq_cap == 0,
               "Failed to copy PS default serial IRQ cap to test child "
               "process.");

    arch_copy_serial_caps(init, env, test_process);
}

/* Run a single test.
 * Each test is launched as its own process. */
int
run_test(struct testcase *test)
{
    UNUSED int error;
    sel4utils_process_t test_process;

    /* Test intro banner. */
    printf("  %s\n", test->name);

    error = sel4utils_configure_process(&test_process, &env.vka, &env.vspace,
                                        env.init->priority, TESTS_APP);
    assert(error == 0);

    /* set up caps about the process */
    env.init->stack_pages = CONFIG_SEL4UTILS_STACK_SIZE / PAGE_SIZE_4K;
    env.init->stack = test_process.thread.stack_top - CONFIG_SEL4UTILS_STACK_SIZE;
    env.init->page_directory = copy_cap_to_process(&test_process, test_process.pd.cptr);
    env.init->root_cnode = SEL4UTILS_CNODE_SLOT;
    env.init->tcb = copy_cap_to_process(&test_process, test_process.thread.tcb.cptr);
    env.init->domain = copy_cap_to_process(&test_process, simple_get_init_cap(&env.simple, seL4_CapDomain));
    env.init->asid_pool = copy_cap_to_process(&test_process, simple_get_init_cap(&env.simple, seL4_CapInitThreadASIDPool));
    env.init->asid_ctrl = copy_cap_to_process(&test_process, simple_get_init_cap(&env.simple, seL4_CapASIDControl));
#ifdef CONFIG_IOMMU
    env.init->io_space = copy_cap_to_process(&test_process, simple_get_init_cap(&env.simple, seL4_CapIOSpace));
#endif /* CONFIG_IOMMU */
#ifdef CONFIG_ARM_SMMU
    env.init->io_space_caps = arch_copy_iospace_caps_to_process(&test_process, &env);
#endif
    env.init->cores = simple_get_core_count(&env.simple);
    /* setup data about untypeds */
    env.init->untypeds = copy_untypeds_to_process(&test_process, untypeds, num_untypeds);
    copy_timer_caps(env.init, &env, &test_process);
    copy_serial_caps(env.init, &env, &test_process);
    /* copy the fault endpoint - we wait on the endpoint for a message
     * or a fault to see when the test finishes */
    seL4_CPtr endpoint = copy_cap_to_process(&test_process, test_process.fault_endpoint.cptr);

    /* WARNING: DO NOT COPY MORE CAPS TO THE PROCESS BEYOND THIS POINT,
     * AS THE SLOTS WILL BE CONSIDERED FREE AND OVERRIDDEN BY THE TEST PROCESS. */
    /* set up free slot range */
    env.init->cspace_size_bits = CONFIG_SEL4UTILS_CSPACE_SIZE_BITS;
    env.init->free_slots.start = endpoint + 1;
    env.init->free_slots.end = (1u << CONFIG_SEL4UTILS_CSPACE_SIZE_BITS);
    assert(env.init->free_slots.start < env.init->free_slots.end);
    /* copy test name */
    strncpy(env.init->name, test->name + strlen("TEST_"), TEST_NAME_MAX);
    /* ensure string is null terminated */
    env.init->name[TEST_NAME_MAX - 1] = '\0';
#ifdef CONFIG_DEBUG_BUILD
    seL4_DebugNameThread(test_process.thread.tcb.cptr, env.init->name);
#endif

    /* set up args for the test process */
    char endpoint_string[WORD_STRING_SIZE];
    char sel4test_name[] = { TESTS_APP };
    char *argv[] = {sel4test_name, endpoint_string};
    snprintf(endpoint_string, WORD_STRING_SIZE, "%lu", (unsigned long)endpoint);
    /* spawn the process */
    error = sel4utils_spawn_process_v(&test_process, &env.vka, &env.vspace,
                            ARRAY_SIZE(argv), argv, 1);
    assert(error == 0);

    /* send env.init_data to the new process */
    void *remote_vaddr = send_init_data(&env, test_process.fault_endpoint.cptr, &test_process);

    /* wait on it to finish or fault, report result */
    seL4_MessageInfo_t info = seL4_Recv(test_process.fault_endpoint.cptr, NULL);

    int result = seL4_GetMR(0);
    if (seL4_MessageInfo_get_label(info) != seL4_Fault_NullFault) {
        sel4utils_print_fault_message(info, test->name);
        sel4debug_dump_registers(test_process.thread.tcb.cptr);
        result = FAILURE;
    }

    /* unmap the env.init data frame */
    vspace_unmap_pages(&test_process.vspace, remote_vaddr, 1, PAGE_BITS_4K, NULL);

    /* reset all the untypeds for the next test */
    for (int i = 0; i < num_untypeds; i++) {
        cspacepath_t path;
        vka_cspace_make_path(&env.vka, untypeds[i].cptr, &path);
        vka_cnode_revoke(&path);
    }

    /* destroy the process */
    sel4utils_destroy_process(&test_process, &env.vka);

    test_assert(result == SUCCESS);
    return result;
}

static void
init_timer_caps(env_t env)
{
    int error;

    /* Allocate slot for the timer IRQ. */
    error = vka_cspace_alloc_path(&env->vka, &env->timer_irq_path);
    ZF_LOGF_IF(error, "Failed to allocate timer IRQ slot.");

    /* Obtain frame cap for PS default timer.
     * Note: We keep the timer's MMIO physical address as an untyped, for the
     * timer, but for the serial we retype it immediately as a frame.
     *
     * The reason for this is that we would prefer to pass untypeds, but since
     * the test driver and test child both use the serial-frame, we can't share
     * it between them as an untyped, so we must retype it as a frame first, so
     * that the cap can be cloned.
     */
    env->timer_paddr = sel4platsupport_get_default_timer_paddr(&env->vka,
                                                               &env->vspace);
    error = vka_alloc_untyped_at(&env->vka, seL4_PageBits, env->timer_paddr,
                                 &env->timer_dev_ut_obj);
    ZF_LOGF_IF(error, "Failed to obtain device-ut cap for default timer.");

    /* Then call into the arch- and plat-specific code to init all arch-
     * and plat-specific code. Some platforms need another timer because they
     * use different timers/drivers for the event-timer and the
     * wall-clock-timer.
     */
    arch_init_timer_caps(env);
}

static void
init_serial_caps(env_t env)
{
    int error;

    /* Allocate slot for the PS default serial's IRQ cap. */
    error = vka_cspace_alloc_path(&env->vka, &env->serial_irq_path);
    ZF_LOGF_IF(error, "Failed to allocate serial IRQ slot.");

    /* Call into the arch-specific code for the next step.
     * x86 needs an I/O cap because the serial is accessed through port-I/O,
     * while ARM needs a frame cap because it doesn't have port-I/O.
     * Both architectures need different initialization code.
     */
    error = arch_init_serial_caps(env);
    ZF_LOGF_IF(error, "Arch-specific serial cap init failed.");
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
    num_untypeds = populate_untypeds(untypeds);

    /* create a frame that will act as the init data, we can then map that
     * in to target processes */
    env.init = (test_init_data_t *) vspace_new_pages(&env.vspace, seL4_AllRights, 1, PAGE_BITS_4K);
    assert(env.init != NULL);

    /* copy the cap to map into the remote process */
    cspacepath_t src, dest;
    vka_cspace_make_path(&env.vka, vspace_get_cap(&env.vspace, env.init), &src);

    UNUSED int error = vka_cspace_alloc(&env.vka, &env.init_frame_cap_copy);
    assert(error == 0);
    vka_cspace_make_path(&env.vka, env.init_frame_cap_copy, &dest);
    error = vka_cnode_copy(&dest, &src, seL4_AllRights);
    assert(error == 0);

    /* copy the untyped size bits list across to the init frame */
    memcpy(env.init->untyped_size_bits_list, untyped_size_bits_list, sizeof(uint8_t) * num_untypeds);

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

    /* now run the tests */
    sel4test_run_tests("sel4test", run_test);

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
    init_serial_caps(&env);

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

    /* init_timer_caps calls acpi_init(), which does unconditional printfs,
     * so it can't go before platsupport_serial_setup_simple().
     */
    init_timer_caps(&env);
    simple_print(&env.simple);

    /* switch to a bigger, safer stack with a guard page
     * before starting the tests */
    printf("Switching to a safer, bigger stack... ");
    fflush(stdout);
    void *res;
    error = sel4utils_run_on_stack(&env.vspace, main_continued, NULL, &res);
    test_assert_fatal(error == 0);
    test_assert_fatal(res == 0);

    return 0;
}

