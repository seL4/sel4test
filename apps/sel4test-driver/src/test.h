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
#include <sel4utils/process.h>
#include <simple/simple.h>
#include <vspace/vspace.h>

/* This file is shared with seltest-tests. */
#include <test_init_data.h>

struct env {
    /* An initialised vka that may be used by the test. */
    vka_t vka;
    /* virtual memory management interface */
    vspace_t vspace;
    /* abtracts over kernel version and boot environment */
    simple_t simple;

    /* Paddr of platsupport default timer. */
    uintptr_t  timer_paddr;
    /* Path to platsupport default timer's IRQ cap. */
    cspacepath_t timer_irq_path;
    /* VKA object for platsupport default timer's device-untyped. */
    vka_object_t timer_dev_ut_obj;

    /* clock timer */
    cspacepath_t clock_irq_path;
    vka_object_t clock_timer_dev_ut_obj;
    uintptr_t clock_timer_paddr;

    /* Extra timer */
    cspacepath_t extra_timer_irq_path;
    vka_object_t extra_timer_dev_ut_obj;
    uintptr_t extra_timer_paddr;

    /* Paddr of the platsupport default serial's frame */
    uintptr_t serial_frame_paddr;
    /* VKA object for the platsupport default serial's frame */
    vka_object_t serial_frame_obj;
    /* Path for the default serial irq handler */
    cspacepath_t serial_irq_path;
    /* I/O port for platsupport default serial; arch-specific. */
    seL4_CPtr serial_io_port_cap;

    /* init data frame vaddr */
    test_init_data_t *init;
    /* extra cap to the init data frame for mapping into the remote vspace */
    seL4_CPtr init_frame_cap_copy;
};

void plat_init(env_t env);
void arch_init_timer_caps(env_t env);
void plat_init_timer_caps(env_t env);
int arch_init_serial_caps(env_t env);
int plat_init_serial_caps(env_t env);
void arch_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process);
void plat_copy_timer_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process);
void arch_copy_serial_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process);
void plat_copy_serial_caps(test_init_data_t *init, env_t env, sel4utils_process_t *test_process);

seL4_CPtr copy_cap_to_process(sel4utils_process_t *process, seL4_CPtr cap);

void init_irq_cap(env_t env, int irq, cspacepath_t *path);
void init_frame_cap(env_t env, void *paddr, cspacepath_t *path);
#ifdef CONFIG_ARM_SMMU
seL4_SlotRegion arch_copy_iospace_caps_to_process(sel4utils_process_t *process, env_t env);
#endif

#endif /* __TEST_H */
