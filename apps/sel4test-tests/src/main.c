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

#include <allocman/vka.h>
#include <allocman/bootstrap.h>

#include <sel4/sel4.h>
#include <sel4/types.h>

#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/serial.h>

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
void
abort(void)
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
void
__arch_putchar(int c)
{
    __plat_putchar(c);
}

static testcase_t *
find_test(const char *name)
{
    testcase_t *test = sel4test_get_test(name);
    if (test == NULL) {
        ZF_LOGF("Failed to find test %s", name);
    }

    return test;
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
    error = allocman_add_untypeds_from_timer_objects(allocator, &init_data->to);
    ZF_LOGF_IF(error, "allocman failed to add timer_objects");

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

seL4_CPtr get_irq_cap(void *data, int id, irq_type_t type) {
    test_init_data_t *init = (test_init_data_t *) data;
    for (size_t i = 0; i < init->to.nirqs; i++) {
        if (init->to.irqs[i].irq.type == type) {
            switch (type) {
            case PS_MSI:
                if (init->to.irqs[i].irq.msi.vector == id) {
                    return init->to.irqs[i].handler_path.capPtr;
                }
                break;
            case PS_IOAPIC:
                if (init->to.irqs[i].irq.ioapic.vector == id) {
                    return init->to.irqs[i].handler_path.capPtr;
                }
                break;
            case PS_INTERRUPT:
                if (init->to.irqs[i].irq.irq.number == id) {
                    return init->to.irqs[i].handler_path.capPtr;
                }
                break;
            case PS_NONE:
                ZF_LOGF("Invalid irq type");
            }
        }
    }

    ZF_LOGF("Could not find irq");
    return seL4_CapNull;
}

static seL4_Error
get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth)
{
    test_init_data_t *init = (test_init_data_t *) data;
    return seL4_CNode_Copy(root, index, depth, init->root_cnode,
                            sel4platsupport_timer_objs_get_irq_cap(&init->to, irq, PS_INTERRUPT), seL4_WordBits, seL4_AllRights);
}

static uint8_t
cnode_size_bits(void *data)
{
    test_init_data_t *init = (test_init_data_t *) data;
    return init->cspace_size_bits;
}

static seL4_CPtr
sched_ctrl(void *data, int core)
{
    return ((test_init_data_t *) data)->sched_ctrl + core;
}

static int
core_count(UNUSED void *data)
{
    return ((test_init_data_t *) data)->cores;
}

void init_timer(env_t env, test_init_data_t *init_data)
{
    /* minimal simple implementation to get the platform
     * default timer off the ground */
    env->simple.arch_simple.irq = get_irq;
    env->simple.data = (void *) init_data;
    env->simple.arch_simple.data = (void *) init_data;
    env->simple.init_cap = sel4utils_process_init_cap;
    env->simple.cnode_size = cnode_size_bits;
    env->simple.sched_ctrl = sched_ctrl;
    env->simple.core_count = core_count;
    sync_mutex_new(&env->vka, &env->timer_mutex);

    arch_init_simple(&env->simple);

    int error = vka_alloc_notification(&env->vka, &env->timer_notification);
    if (error != 0) {
        ZF_LOGF("Failed to allocate notification object");
    }

    if (config_set(CONFIG_HAVE_TIMER)) {
        arch_init_timer(env, init_data);
        ltimer_reset(&env->timer.ltimer);
    }
}

int
main(int argc, char **argv)
{

    test_init_data_t *init_data;
    struct env env;

    /* parse args */
    assert(argc == 2);
    endpoint = (seL4_CPtr) atoi(argv[0]);

    /* read in init data */
    init_data = (void*) atol(argv[1]);

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
#ifdef CONFIG_ARM_SMMU
    env.io_space_caps = init_data->io_space_caps;
#endif
    env.cores = init_data->cores;
    env.num_regions = init_data->num_elf_regions;
    memcpy(env.regions, init_data->elf_regions, sizeof(sel4utils_elf_region_t) * env.num_regions);

    /* initialse cspace, vspace and untyped memory allocation */
    init_allocator(&env, init_data);

    /* initialise the timer */
    init_timer(&env, init_data);

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

    /* turn off the timer */
    if (config_set(CONFIG_HAVE_TIMER)) {
        sel4platsupport_destroy_timer(&env.timer, &env.vka);
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
