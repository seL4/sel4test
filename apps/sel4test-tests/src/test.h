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
#include <allocman/allocman.h>
#include <sel4/bootinfo.h>

#include <vka/vka.h>
#include <vka/object.h>
#include <sel4test/test.h>
#include <sel4platsupport/timer.h>
#include <simple/simple.h>
#include <vspace/vspace.h>

/* This file is a symlink to the original in sel4test-driver. */
#include <test_init_data.h>

void plat_add_uts(env_t env, allocman_t *alloc, test_init_data_t *data);
void arch_init_simple(simple_t *simple);
void plat_init_env(env_t env, test_init_data_t *data);
seL4_Error plat_get_frame_cap(void *data, void *paddr, int size_bits, cspacepath_t *path);
seL4_Error plat_get_irq(void *data, int irq, seL4_CNode root, seL4_Word index, uint8_t depth);

#endif /* __TEST_H */
