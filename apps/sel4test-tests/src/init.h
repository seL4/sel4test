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
#pragma once

#include <allocman/allocman.h>

#include "helpers.h"

void arch_init_allocator(env_t env, test_init_data_t *data);
void arch_init_timer(env_t env, test_init_data_t *data);
void arch_init_simple(simple_t *simple);
seL4_CPtr get_irq_cap(void *data, int id, irq_type_t irq);
