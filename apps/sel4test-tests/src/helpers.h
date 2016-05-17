/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#ifndef __HELPERS_H
#define __HELPERS_H

#include <vka/vka.h>
#include <vspace/vspace.h>
#include <sel4utils/thread.h>
#include <sel4utils/process.h>
#include <sel4utils/mapping.h>

#include <sel4platsupport/timer.h>
#include <platsupport/timer.h>

#include "test.h"

#define OUR_PRIO (env->priority)
/* args provided by the user */
#define HELPER_THREAD_MAX_ARGS 4
/* metadata helpers adds */
#define HELPER_THREAD_META     4
/* total args (user + meta) */
#define HELPER_THREAD_TOTAL_ARGS (HELPER_THREAD_MAX_ARGS + HELPER_THREAD_META)

typedef int (*helper_fn_t)(seL4_Word, seL4_Word, seL4_Word, seL4_Word);

typedef struct helper_thread {
    sel4utils_elf_region_t regions[MAX_REGIONS];
    int num_regions;
    sel4utils_process_t process;

    sel4utils_thread_t thread;
    vka_object_t local_endpoint;
    seL4_CPtr fault_endpoint;

    void *arg0;
    void *arg1;
    char *args[HELPER_THREAD_TOTAL_ARGS];
    char args_strings[HELPER_THREAD_TOTAL_ARGS][WORD_STRING_SIZE];

    bool is_process;
} helper_thread_t;

/* Helper thread/process functions */

/* create a helper in the current vspace and current cspace */
void create_helper_thread(env_t env, helper_thread_t *thread);

/* create a helper with a clone of the current vspace loadable elf segments,
 * and a new cspace */
void create_helper_process(env_t env, helper_thread_t *thread);

int create_passive_thread(env_t env, helper_thread_t *passive, helper_fn_t fn, seL4_CPtr ep, 
                      seL4_Word arg1, seL4_Word arg2, seL4_Word arg3);
int create_passive_thread_with_tfep(env_t env, helper_thread_t *passive, seL4_CPtr tfep,
                                seL4_Word badge, helper_fn_t fn, seL4_CPtr ep, seL4_Word arg1, 
                                seL4_Word arg2, seL4_Word arg3, sel4utils_checkpoint_t *cp);

/* set a helper threads priority */
void set_helper_priority(helper_thread_t *thread, uint8_t prio);

/* set a helper threads maximum controlled priority */
void set_helper_mcp(helper_thread_t *thread, uint8_t mcp);

/* set a helper threads criticality */
void set_helper_criticality(helper_thread_t *thread, uint8_t crit);

/* set a helper threads maximum controlled criticality */
void set_helper_mcc(helper_thread_t *thread, uint8_t mcc); 

/* set a helper threads scheduling parameters */
int set_helper_sched_params(env_t env, helper_thread_t *thread, seL4_Time budget, 
                            seL4_Time period, seL4_Word data);

/* set a helpers temporal fault endpoint */
void set_helper_tfep(env_t env, helper_thread_t *thread, seL4_CPtr tfep);

/* Start a helper. Note: arguments to helper processes will be copied into
 * the address space of that process. Do not pass pointers to data only in
 * the local vspace, this will fail. */
void start_helper(env_t env, helper_thread_t *thread, helper_fn_t entry_point,
                  seL4_Word arg0, seL4_Word arg1, seL4_Word arg2, seL4_Word arg3);

/* save a threads seL4_UserContext, increment instruction pointer, and resume */
int restart_after_syscall(env_t env, helper_thread_t *thread);

/* wait for a helper thread to finish */
int wait_for_helper(helper_thread_t *thread);

/* end timeslice - this may block until the thread is due to have its timeslice recharged */
void helper_yield(helper_thread_t *thread);

/* free all resources associated with a helper and tear it down */
void cleanup_helper(env_t env, helper_thread_t *thread);
/*
 * Check whether a given region of memory is zeroed out.
 */
int check_zeroes(seL4_Word addr, seL4_Word size_bytes);


/* Determine if two TCBs in the init thread's CSpace are not equal. Note that we
 * assume the thread is not currently executing.
 *
 * Serves as != comparator for caps.
 */
int are_tcbs_distinct(seL4_CPtr tcb1, seL4_CPtr tcb2);

/* cnode_ops wrappers */
int cnode_copy(env_t env, seL4_CPtr src, seL4_CPtr dest, seL4_Word rights);
int cnode_delete(env_t env, seL4_CPtr slot);
int cnode_mint(env_t env, seL4_CPtr src, seL4_CPtr dest, seL4_Word rights, seL4_CapData_t badge);
int cnode_move(env_t env, seL4_CPtr src, seL4_CPtr dest);
int cnode_mutate(env_t env, seL4_CPtr src, seL4_CPtr dest);
int cnode_recycle(env_t env, seL4_CPtr cap);
int cnode_revoke(env_t env, seL4_CPtr cap);
int cnode_rotate(env_t env, seL4_CPtr src, seL4_CPtr pivot, seL4_CPtr dest);
int cnode_swapcaller(env_t env, seL4_CPtr cap);
int cnode_swapTCBcaller(env_t env, seL4_CPtr cap, vka_object_t *tcb);
/* Determine whether a given slot in the init thread's CSpace is empty by
 * examining the error when moving a slot onto itself.
 *
 * Serves as == 0 comparator for caps.
 */
int is_slot_empty(env_t env, seL4_Word slot);

/* Get a free slot */
seL4_Word get_free_slot(env_t env);

/* timer */
void wait_for_timer_interrupt(env_t env);
void sleep(env_t env, uint64_t ns);
/* returns an increasing timestamp in ns (may rollover on platforms without 64 bit upcounters) */
uint64_t timestamp(env_t env);

#endif /* __HELPERS_H */
