/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
/* this file is shared between sel4test-driver an sel4test-tests */
#ifndef __TEST_H
#define __TEST_H

#include <autoconf.h>
#include <sel4/bootinfo.h>

#include <vka/vka.h>
#include <vka/object.h>
#include <sel4test/test.h>
#include <sel4utils/elf.h>
#include <simple/simple.h>
#include <vspace/vspace.h>

/* max test name size */
#define TEST_NAME_MAX 20

/* Increase if the sel4test-tests binary
 * has new loadable sections added */
#define MAX_REGIONS 4

/* data shared between sel4test-driver and the sel4test-tests app.
 * all caps are in the sel4test-tests process' cspace */
typedef struct {
    /* page directory of the test process */
    seL4_CPtr page_directory;
    /* root cnode of the test process */
    seL4_CPtr root_cnode;
    /* tcb of the test process */
    seL4_CPtr tcb;
#ifndef CONFIG_KERNEL_STABLE
    /* asid pool cap for the test process to use when creating new processes */
    seL4_CPtr asid_pool;
    seL4_CPtr asid_ctrl;
#endif
#ifdef CONFIG_IOMMU
    seL4_CPtr io_space;
#endif /* CONFIG_IOMMU */
    /* cap to the sel4platsupport default timer irq handler */
    seL4_CPtr timer_irq;
    /* cap to the sel4platsupport default timer physical frame */
    seL4_CPtr timer_frame;
    /* cap to the clock timer irq handler */
    seL4_CPtr clock_timer_irq;
    /* cap to the clock timer frame cap */
    seL4_CPtr clock_timer_frame;
    /* frequency of the tsc (if applicable) */
    seL4_Word tsc_freq;
    /* cap to the sched ctrl capability */
    seL4_SchedControl sched_ctrl;
    /* size of the test processes cspace */
    seL4_Word cspace_size_bits;
    /* range of free slots in the cspace */
    seL4_SlotRegion free_slots;

    /* range of untyped memory in the cspace */
    seL4_SlotRegion untypeds;
    /* size of untyped that each untyped cap corresponds to
     * (size of the cap at untypeds.start is untyped_size_bits_lits[0]) */
    uint8_t untyped_size_bits_list[CONFIG_MAX_NUM_BOOTINFO_UNTYPED_CAPS];
    /* name of the test to run */
    char name[TEST_NAME_MAX];
    /* priority the test process is running at */
    int priority;

    /* List of elf regions in the test process image, this
     * is provided so the test process can launch copies of itself.
     *
     * Note: copies should not rely on state from the current process
     * or the image. Only use copies to run code functions, pass all
     * required state as arguments. */
    sel4utils_elf_region_t elf_regions[MAX_REGIONS];

    /* the number of elf regions */
    int num_elf_regions;

    /* the number of pages in the stack */
    int stack_pages;

    /* address of the stack */
    void *stack;
} test_init_data_t;

struct env {
    /* An initialised vka that may be used by the test. */
    vka_t vka;
    /* virtual memory management interface */
    vspace_t vspace;
    /* initialised timer for timeouts */
    seL4_timer_t *timer;
    /* initialised timer for reading a timestamp */
    seL4_timer_t *clock_timer;
    /* abstract interface over application init */
    simple_t simple;
    /* notification for timer */
    vka_object_t timer_notification;

    /* caps for the current process */
    seL4_CPtr cspace_root;
    seL4_CPtr page_directory;
    seL4_CPtr endpoint;
    seL4_CPtr tcb;
#ifndef CONFIG_KERNEL_STABLE
    seL4_CPtr asid_pool;
    seL4_CPtr asid_ctrl;
#endif /* CONFIG_KERNEL_STABLE */
#ifdef CONFIG_IOMMU
    seL4_CPtr io_space;
#endif /* CONFIG_IOMMU */
    seL4_CPtr domain;

    int priority;
    int cspace_size_bits;
    int num_regions;
    sel4utils_elf_region_t regions[MAX_REGIONS];
};


void arch_init_simple(simple_t *simple);
void plat_init_env(env_t env, test_init_data_t *data);
seL4_Error plat_get_frame_cap(void *data, void *paddr, int size_bits, cspacepath_t *path);
seL4_Error plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth);
#endif /* __TEST_H */

