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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <arch_stdio.h>
#include <allocman/vka.h>
#include <allocman/bootstrap.h>

#include <sel4/sel4.h>
#include <sel4/types.h>

#include <sel4platsupport/timer.h>

#include <sel4utils/util.h>
#include <sel4utils/mapping.h>
#include <sel4utils/vspace.h>

#include <sel4test/test.h>

#include <vka/capops.h>

#include "helpers.h"
#include "test.h"
#include "init.h"

/* dummy global for libsel4muslcsys */
char _cpio_archive[1];
char _cpio_archive_end[1];

/* endpoint to call back to the test driver on */
static seL4_CPtr endpoint;

/* global static memory for init */
static sel4utils_alloc_data_t alloc_data;

/* dimensions of virtual memory for the allocator to use */
#define ALLOCATOR_VIRTUAL_POOL_SIZE ((1 << seL4_PageBits) * 4000)

/* allocator static pool */
#define ALLOCATOR_STATIC_POOL_SIZE ((1 << seL4_PageBits) * 20)
static char allocator_mem_pool[ALLOCATOR_STATIC_POOL_SIZE];

/* override abort, called by exit (and assert fail) */
void abort(void)
{
    /* send back a failure */
    seL4_MessageInfo_t info = seL4_MessageInfo_new(seL4_Fault_NullFault, 0, 0, 1);
    seL4_SetMR(0, -1);
    seL4_Send(endpoint, info);

    /* we should not get here */
    assert(0);
    while (1);
}

void __plat_putchar(int c);
static size_t write_buf(void *data, size_t count)
{
    char *buf = data;
    for (int i = 0; i < count; i++) {
        __plat_putchar(buf[i]);
    }
    return count;
}

static testcase_t *find_test(const char *name)
{
    testcase_t *test = sel4test_get_test(name);
    if (test == NULL) {
        ZF_LOGF("Failed to find test %s", name);
    }

    return test;
}

static void init_allocator(env_t env, test_init_data_t *init_data)
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
    size_t size_bits;
    cspacepath_t path;
    for (slot = init_data->untypeds.start, size_bits_index = 0;
         slot <= init_data->untypeds.end;
         slot++, size_bits_index++) {

        vka_cspace_make_path(&env->vka, slot, &path);
        /* allocman doesn't require the paddr unless we need to ask for phys addresses,
         * which we don't. */
        size_bits = init_data->untyped_size_bits_list[size_bits_index];
        error = allocman_utspace_add_uts(allocator, 1, &path, &size_bits, NULL,
                                         ALLOCMAN_UT_KERNEL);
        if (error) {
            ZF_LOGF("Failed to add untyped objects to allocator");
        }
    }

    /* add any arch specific objects to the allocator */
    arch_init_allocator(env, init_data);

    /* create a vspace */
    void *existing_frames[init_data->stack_pages + 2];
    existing_frames[0] = (void *) init_data;
    existing_frames[1] = seL4_GetIPCBuffer();
    assert(init_data->stack_pages > 0);
    for (int i = 0; i < init_data->stack_pages; i++) {
        existing_frames[i + 2] = init_data->stack + (i * PAGE_SIZE_4K);
    }

    error = sel4utils_bootstrap_vspace(&env->vspace, &alloc_data, init_data->page_directory, &env->vka,
                                       NULL, NULL, existing_frames);

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

static uint8_t cnode_size_bits(void *data)
{
    test_init_data_t *init = (test_init_data_t *) data;
    return init->cspace_size_bits;
}

static seL4_CPtr sched_ctrl(void *data, int core)
{
    return ((test_init_data_t *) data)->sched_ctrl + core;
}

static int core_count(UNUSED void *data)
{
    return ((test_init_data_t *) data)->cores;
}

void init_simple(env_t env, test_init_data_t *init_data)
{
    /* minimal simple implementation */
    env->simple.data = (void *) init_data;
    env->simple.arch_simple.data = (void *) init_data;
    env->simple.init_cap = sel4utils_process_init_cap;
    env->simple.cnode_size = cnode_size_bits;
    env->simple.sched_ctrl = sched_ctrl;
    env->simple.core_count = core_count;

    arch_init_simple(env, &env->simple);
}

int main(int argc, char **argv)
{
    sel4muslcsys_register_stdio_write_fn(write_buf);

    test_init_data_t *init_data;
    struct env env;

    /* parse args */
    assert(argc == 2);
    endpoint = (seL4_CPtr) atoi(argv[0]);

    /* read in init data */
    init_data = (void *) atol(argv[1]);

    /* configure env */
    env.cspace_root = init_data->root_cnode;
    env.page_directory = init_data->page_directory;
    env.endpoint = endpoint;
    env.priority = init_data->priority;
    env.cspace_size_bits = init_data->cspace_size_bits;
    env.tcb = init_data->tcb;
    env.domain = init_data->domain;
    env.asid_pool = init_data->asid_pool;
    env.asid_ctrl = init_data->asid_ctrl;
    env.sched_ctrl = init_data->sched_ctrl;
#ifdef CONFIG_IOMMU
    env.io_space = init_data->io_space;
#endif
#ifdef CONFIG_TK1_SMMU
    env.io_space_caps = init_data->io_space_caps;
#endif
    env.cores = init_data->cores;
    env.num_regions = init_data->num_elf_regions;
    memcpy(env.regions, init_data->elf_regions, sizeof(sel4utils_elf_region_t) * env.num_regions);

    env.timer_notification.cptr = init_data->timer_ntfn;

    env.device_frame = init_data->device_frame_cap;

    /* initialse cspace, vspace and untyped memory allocation */
    init_allocator(&env, init_data);

    /* initialise simple */
    init_simple(&env, init_data);

    /* initialise rpc client */
    sel4rpc_client_init(&env.rpc_client, env.endpoint, SEL4TEST_PROTOBUF_RPC);

    /* find the test */
    testcase_t *test = find_test(init_data->name);

    /* run the test */
    sel4test_reset();
    test_result_t result = SUCCESS;
    if (test) {
        printf("Running test %s (%s)\n", test->name, test->description);
        result = test->function((uintptr_t)&env);
    } else {
        result = FAILURE;
        ZF_LOGF("Cannot find test %s\n", init_data->name);
    }

    printf("Test %s %s\n", init_data->name, result == SUCCESS ? "passed" : "failed");
    /* send our result back */
    seL4_MessageInfo_t info = seL4_MessageInfo_new(seL4_Fault_NullFault, 0, 0, 1);
    seL4_SetMR(0, result);
    seL4_Send(endpoint, info);

    /* It is expected that we are torn down by the test driver before we are
     * scheduled to run again after signalling them with the above send.
     */
    assert(!"unreachable");
    return 0;
}
