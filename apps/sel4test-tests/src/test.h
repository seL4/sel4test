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
/* this file is shared between sel4test-driver an sel4test-tests */
#pragma once

#include <allocman/allocman.h>
#include <sel4/bootinfo.h>

#include <vka/vka.h>
#include <vka/object.h>
#include <sel4test/test.h>
#include <sel4platsupport/timer.h>
#include <sel4utils/api.h>
#include <simple/simple.h>
#include <vspace/vspace.h>

/* This file is a symlink to the original in sel4test-driver. */
#include <test_init_data.h>

void arch_init_simple(env_t env, simple_t *simple);

