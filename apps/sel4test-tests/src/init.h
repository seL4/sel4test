/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <allocman/allocman.h>

#include "helpers.h"

void arch_init_allocator(env_t env, test_init_data_t *data);
void arch_init_simple(env_t env, simple_t *simple);
seL4_CPtr get_irq_cap(void *data, int id, irq_type_t irq);
