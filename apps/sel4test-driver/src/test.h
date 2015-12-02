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
#include <sel4utils/process.h>
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
    /* abtracts over kernel version and boot environment */
    simple_t simple;
    /* path for the default timer irq handler */
    cspacepath_t irq_path;
    /* path for the default timer irq handler */
    cspacepath_t clock_irq_path;
    /* frame for the default timer */
    cspacepath_t frame_path;
    /* frame for the clock timer */
    cspacepath_t clock_frame_path;
    /* io port for the default timer */
    seL4_CPtr io_port_cap;
    /* init data frame vaddr */
    test_init_data_t *init;
    /* extra cap to the init data frame for mapping into the remote vspace */
    seL4_CPtr init_frame_cap_copy;
    /* frequency of the tsc (if applicable) */
    seL4_Word tsc_freq;
};

void plat_init(env_t env);
void plat_init_caps(env_t env);
void arch_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process);
seL4_CPtr copy_cap_to_process(sel4utils_process_t *process, seL4_CPtr cap);

void init_irq_cap(env_t env, int irq, cspacepath_t *path);
void init_frame_cap(env_t env, void *paddr, cspacepath_t *path);

#endif /* __TEST_H */
