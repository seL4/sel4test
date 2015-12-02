/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <allocman/vka.h>
#include <allocman/bootstrap.h>

#include <sel4/sel4.h>
#include <sel4/types.h>

#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <platsupport/timer.h>

#include <sel4utils/util.h>
#include <sel4utils/mapping.h>
#include <sel4utils/vspace.h>

#include <sel4test/test.h>

#include <vka/capops.h>

#include "helpers.h"
#include "test.h"

/* dummy global for libsel4muslcsys */
char _cpio_archive[1];

/* endpoint to call back to the test driver on */
static seL4_CPtr endpoint;

/* global static memory for init */
static sel4utils_alloc_data_t alloc_data;

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE ((1 << seL4_PageBits) * 200)

/* allocator static pool */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 10)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* override abort, called by exit (and assert fail) */
void
abort(void)
{
    /* send back a failure */
    seL4_MessageInfo_t info = seL4_MessageInfo_new(seL4_NoFault, 0, 0, 1);
    seL4_SetMR(0, -1);
    seL4_Send(endpoint, info);

    /* we should not get here */
    assert(0);
    while (1);
}

/* this app won't print in release mode */
void
__arch_putchar(int c)
{
#ifdef CONFIG_DEBUG_BUILD
    seL4_DebugPutChar(c);
#endif
}

static testcase_t *
find_test(char *name)
{
    testcase_t *test = sel4test_get_test(name);
    if (test == NULL) {
        ZF_LOGF("Failed to find test %s", name);
    }

    return test;
}

static test_init_data_t *
receive_init_data(seL4_CPtr endpoint)
{
    /* wait for a message */
    seL4_Word badge;
    UNUSED seL4_MessageInfo_t info;

    info = seL4_Recv(endpoint, &badge);

    /* check the label is correct */
    assert(seL4_MessageInfo_get_label(info) == seL4_NoFault);
    assert(seL4_MessageInfo_get_length(info) == 1);

    test_init_data_t *init_data = (test_init_data_t *) seL4_GetMR(0);
    assert(init_data->free_slots.start != 0);
    assert(init_data->free_slots.end != 0);

    return init_data;
}

static void
init_allocator(env_t env, test_init_data_t *init_data)
{
    UNUSED int error;
    UNUSED reservation_t virtual_reservation;

    /* initialise allocator */
    allocman_t *allocator = bootstrap_use_current_1level(init_data->root_cnode,
                                                         init_data->cspace_size_bits, init_data->free_slots.start,
                                                         init_data->free_slots.end, ALLOCATOR_STATIC_POOL_SIZE,
                                                         allocator_mem_pool);
    if (allocator == NULL) {
        ZF_LOGF("Failed to bootstrap allocator");
    }
    allocman_make_vka(&env->vka, allocator);

    /* fill the allocator with untypeds */
    seL4_CPtr slot;
    unsigned int size_bits_index;
    for (slot = init_data->untypeds.start, size_bits_index = 0;
            slot <= init_data->untypeds.end;
            slot++, size_bits_index++) {

        cspacepath_t path;
        vka_cspace_make_path(&env->vka, slot, &path);
        /* allocman doesn't require the paddr unless we need to ask for phys addresses,
         * which we don't. */
        uintptr_t fake_paddr = 0;
        size_t size_bits = init_data->untyped_size_bits_list[size_bits_index];
        error = allocman_utspace_add_uts(allocator, 1, &path, &size_bits, &fake_paddr);
        if (error) {
            ZF_LOGF("Failed to add untyped objects to allocator");
        }
    }

    /* create a vspace */
    void *existing_frames[init_data->stack_pages + 2];
    existing_frames[0] = (void *) init_data;
    existing_frames[1] = seL4_GetIPCBuffer();
    assert(init_data->stack_pages > 0);
    for (int i = 0; i < init_data->stack_pages; i++) {
        existing_frames[i + 2] = init_data->stack + (i * PAGE_SIZE_4K);
    }

    error = sel4utils_bootstrap_vspace(&env->vspace, &alloc_data, init_data->page_directory, &env->vka,                 NULL, NULL, existing_frames);

    /* switch the allocator to a virtual memory pool */
    void *vaddr;
    virtual_reservation = vspace_reserve_range(&env->vspace, ALLOCATOR_VIRTUAL_POOL_SIZE,
                                               seL4_AllRights, 1, &vaddr);
    if (virtual_reservation.res == 0) {
        ZF_LOGF("Failed to switch allocator to virtual memory pool");
    }

    bootstrap_configure_virtual_pool(allocator, vaddr, ALLOCATOR_VIRTUAL_POOL_SIZE,
                                     env->page_directory);

}

static seL4_Error
get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    test_init_data_t *init = (test_init_data_t *) data;
    if (irq == DEFAULT_TIMER_INTERRUPT) {
        return seL4_CNode_Copy(root, index, depth, init->root_cnode,
                               init->timer_irq, seL4_WordBits, seL4_AllRights);
    }

    return plat_get_irq(data, irq, root, index, depth);
}

static seL4_CPtr
get_init_cap_fn(void *data, seL4_CPtr cap)
{
    assert(cap == seL4_CapSchedControl);
    test_init_data_t *init = (test_init_data_t *) data;
    return init->sched_ctrl;
}


void init_timers(env_t env, test_init_data_t *init_data)
{
    /* minimal simple implementation to get the platform
     * default timer off the ground */
    env->simple.irq = get_irq;
    env->simple.init_cap = get_init_cap_fn;
    env->simple.data = (void *) init_data;

    UNUSED int error;

    arch_init_simple(&env->simple);

    error = vka_alloc_notification(&env->vka, &env->timer_notification);
    if (error != 0) {
        ZF_LOGF("Failed to allocate notification object");
    }


    ZF_LOGV("Init env\n");
    plat_init_env(env, init_data);
}

int
main(int argc, char **argv)
{

    test_init_data_t *init_data;
    struct env env;

    assert(argc >= 2);
    /* in order to have some shitty almost-fork-like semantics
     * main can get run multiple times. Look in src/helpers.c
     * for where this is used. Just means we check the first
     * arg, and if not NULL jmp to it */
    void (*helper_thread)(int argc,char **argv) = (void(*)(int, char**))atol(argv[1]);
    if (helper_thread) {
        helper_thread(argc, argv);
    }

    /* parse args */
    assert(argc == 3);
    endpoint = (seL4_CPtr) atoi(argv[2]);

    /* read in init data */
    init_data = receive_init_data(endpoint);

    /* configure env */
    env.cspace_root = init_data->root_cnode;
    env.page_directory = init_data->page_directory;
    env.endpoint = endpoint;
    env.priority = init_data->priority;
    env.cspace_size_bits = init_data->cspace_size_bits;
    env.tcb = init_data->tcb;
#ifndef CONFIG_KERNEL_STABLE
    env.asid_pool = init_data->asid_pool;
    env.asid_ctrl = init_data->asid_ctrl;
#endif /* CONFIG_KERNEL_STABLE */
#ifdef CONFIG_IOMMU
    env.io_space = init_data->io_space;
#endif
    env.num_regions = init_data->num_elf_regions;
    ZF_LOGV("Copying data\n");
    memcpy(env.regions, init_data->elf_regions, sizeof(sel4utils_elf_region_t) * env.num_regions);

    /* initialse cspace, vspace and untyped memory allocation */
    ZF_LOGV("Init allocator\n");
    init_allocator(&env, init_data);

    /* initialise the timer */
    ZF_LOGV("Init timers\n");
    init_timers(&env, init_data);

    ZF_LOGV("Finding test\n");
    /* find the test */
    testcase_t *test = find_test(init_data->name);

    /* run the test */
    int result = 0;
    if (test) {
        printf("Running test %s (%s)\n", test->name, test->description);
        result = test->function(&env);
    } else {
        result = FAILURE;
        ZF_LOGF("Cannot find test %s\n", init_data->name);
    }

    printf("Test %s %s\n", init_data->name, result == SUCCESS ? "passed" : "failed");
    /* send our result back */
    seL4_MessageInfo_t info = seL4_MessageInfo_new(seL4_NoFault, 0, 0, 1);
    seL4_SetMR(0, result);
    seL4_Send(endpoint, info);

    /* It is expected that we are torn down by the test driver before we are
     * scheduled to run again after signalling them with the above send.
     */
    assert(!"unreachable");
    return 0;
}


