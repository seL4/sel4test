/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#pragma once

#include <sel4test/test.h>
#include "test.h"
#include <sel4testsupport/testreporter.h>

#define TIMER_ID 0

/* Timing related functions used only within sel4test-driver */
void handle_timer_interrupts(driver_env_t env, seL4_Word badge);
void wait_for_timer_interrupt(driver_env_t env);
void timeout(driver_env_t env, uint64_t ns, timeout_type_t timeout);
uint64_t timestamp(driver_env_t env);
void timer_reset(driver_env_t env);
void timer_cleanup(driver_env_t env);
