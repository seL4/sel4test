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

#include <platsupport/timer.h>

#include <sel4debug/register_dump.h>
#include <sel4platsupport/platsupport.h>
#include <sel4platsupport/plat/timer.h>
#include <sel4utils/vspace.h>
#include <sel4utils/stack.h>
#include <sel4utils/process.h>

#include <simple/simple.h>
#ifdef CONFIG_KERNEL_STABLE
#include <simple-stable/simple-stable.h>
#else
#include <simple-default/simple-default.h>
#endif

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
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 10)
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
                                                           &env->vka, seL4_GetBootInfo());
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
    seL4_MessageInfo_t info = seL4_MessageInfo_new(seL4_NoFault, 0, 0, 1);
    seL4_SetMR(0, (seL4_Word) remote_vaddr);
    seL4_Send(endpoint, info);

    return remote_vaddr;
}

/* copy the caps required to set up the sel4platsupport default timer */
static void
copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process)
{
    /* irq cap for the timer irq */
    init->timer_irq = copy_cap_to_process(test_process, env->irq_path.capPtr);
    assert(init->timer_irq != 0);

    arch_copy_timer_caps(init, env, test_process);
    
    init->sched_ctrl = copy_cap_to_process(test_process, simple_get_sched_ctrl(&env->simple));
    assert(init->sched_ctrl != 0);
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

    error = sel4utils_configure_process(&test_process, &env.simple, &env.vka, &env.vspace,
                                        env.init->priority, TESTS_APP);
    assert(error == 0);

    /* set up caps about the process */
    env.init->stack_pages = CONFIG_SEL4UTILS_STACK_SIZE / PAGE_SIZE_4K;
    env.init->stack = test_process.thread.stack_top - CONFIG_SEL4UTILS_STACK_SIZE;
    env.init->page_directory = copy_cap_to_process(&test_process, test_process.pd.cptr);
    env.init->root_cnode = SEL4UTILS_CNODE_SLOT;
    env.init->tcb = copy_cap_to_process(&test_process, test_process.thread.tcb.cptr);
    env.init->domain = copy_cap_to_process(&test_process, simple_get_init_cap(&env.simple, seL4_CapDomain));
#ifndef CONFIG_KERNEL_STABLE
    env.init->asid_pool = copy_cap_to_process(&test_process, simple_get_init_cap(&env.simple, seL4_CapInitThreadASIDPool));
    env.init->asid_ctrl = copy_cap_to_process(&test_process, simple_get_init_cap(&env.simple, seL4_CapASIDControl));
#endif /* CONFIG_KERNEL_STABLE */
#ifdef CONFIG_IOMMU
    env.init->io_space = copy_cap_to_process(&test_process, simple_get_init_cap(&env.simple, seL4_CapIOSpace));
#endif /* CONFIG_IOMMU */
    /* setup data about untypeds */
    env.init->untypeds = copy_untypeds_to_process(&test_process, untypeds, num_untypeds);
    copy_timer_caps(env.init, &env, &test_process);
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
#ifdef SEL4_DEBUG_KERNEL
    seL4_DebugNameThread(test_process.thread.tcb.cptr, env.init->name);
#endif

    /* set up args for the test process */
    char endpoint_string[WORD_STRING_SIZE];
    char sel4test_name[] = { TESTS_APP };
    char zero_string[] = {"0"};
    char *argv[] = {sel4test_name, zero_string, endpoint_string};
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
    if (seL4_MessageInfo_get_label(info) != seL4_NoFault) {
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
    /* get the irq control cap */
    seL4_CPtr cap;
    int error = vka_cspace_alloc(&env->vka, &cap);
    if (error != 0) {
        ZF_LOGF("Failed to allocate cslot, error %d", error);
    }

    vka_cspace_make_path(&env->vka, cap, &env->irq_path);
    error = simple_get_IRQ_control(&env->simple, DEFAULT_TIMER_INTERRUPT, env->irq_path);

    if (error != 0) {
        ZF_LOGF("Failed to get IRQ control, error %d", error);
    }

    arch_init_timer_caps(env);
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

    /* get the caps we need to send to tests to set up a timer */
    init_timer_caps(&env);

    /* setup init data that won't change test-to-test */
    env.init->priority = seL4_MaxPrio - 1;

    /* now run the tests */
    sel4test_run_tests("sel4test", run_test);

    return NULL;
}

int main(void)
{
    seL4_BootInfo *info = seL4_GetBootInfo();

#ifdef SEL4_DEBUG_KERNEL
    seL4_DebugNameThread(seL4_CapInitThreadTCB, "sel4test-driver");
#endif

    compile_time_assert(init_data_fits_in_ipc_buffer, sizeof(test_init_data_t) < PAGE_SIZE_4K);
    /* initialise libsel4simple, which abstracts away which kernel version
     * we are running on */
#ifdef CONFIG_KERNEL_STABLE
    simple_stable_init_bootinfo(&env.simple, info);
#else
    simple_default_init_bootinfo(&env.simple, info);
#endif

    /* initialise the test environment - allocator, cspace manager, vspace manager, timer */
    init_env(&env);

    /* enable serial driver */
    platsupport_serial_setup_simple(NULL, &env.simple, &env.vka);

    /* switch to a bigger, safer stack with a guard page
     * before starting the tests */
    printf("Switching to a safer, bigger stack... ");
    fflush(stdout);
    void *res;
    int error = sel4utils_run_on_stack(&env.vspace, main_continued, NULL, &res);
    test_assert_fatal(error == 0);
    test_assert_fatal(res == 0);

    return 0;
}

