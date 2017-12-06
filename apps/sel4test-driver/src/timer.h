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

#include <sel4test/test.h>
#include "test.h"
#include <sel4testsupport/testreporter.h>

#define TIMER_ID 0

/* Timing related functions used only by in sel4test-driver */
void wait_for_timer_interrupt(driver_env_t env);
void timeout(driver_env_t env, uint64_t ns, timeout_type_t timeout);
uint64_t timestamp(driver_env_t env);
void timer_reset(driver_env_t env);
void timer_cleanup(driver_env_t env);
