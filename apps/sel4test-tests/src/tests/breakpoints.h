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

#ifdef HAVE_AUTOCONF
#include <autoconf.h>
#endif

#include <vka/capops.h>

#ifdef CONFIG_HARDWARE_DEBUG_API

#define FAULT_EP_KERNEL_BADGE_VALUE           (1)

#define SINGLESTEP_TEST_MAX_LOOP_ITERATIONS 10

#define BREAKPOINT_TEST_FAULTER_PRIO  (seL4_MinPrio + 10)
#define BREAKPOINT_TEST_HANDLER_PRIO  (seL4_MinPrio + 20)

/* Other (receiver) end of the Endpoint object on which the kernel will queue
 * the Fault events triggered by the fault thread.
 */
extern cspacepath_t fault_ep_cspath;

struct {
    seL4_Word vaddr, vaddr2, reason, bp_num;
} static fault_data;

int setup_caps_for_test(struct env *env);
int setup_faulter_thread_for_test(struct env *env, helper_thread_t *faulter_thread);

#endif /* CONFIG_HARDWARE_DEBUG_API*/
