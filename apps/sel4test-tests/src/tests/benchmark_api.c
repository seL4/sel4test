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

#include <autoconf.h>

#ifdef CONFIG_BENCHMARK_TRACK_UTILISATION
#include <sel4/sel4.h>
#include <vka/object.h>

#include <utils/util.h>
#include <sel4/benchmark_utilisation_types.h>
#include "../helpers.h"

static int
test_benchmark_utilisation(env_t env)
{

    uint64_t * __attribute__((__may_alias__)) ipcbuffer = (uint64_t *) &(seL4_GetIPCBuffer()->msg[0]);

    seL4_BenchmarkResetThreadUtilisation(simple_get_tcb(&env->simple));

    seL4_BenchmarkResetLog();

    /* Sleep, and allow Idle thread to run */
    sel4test_sleep(env, NS_IN_S);

    seL4_BenchmarkFinalizeLog();

    seL4_BenchmarkGetThreadUtilisation(simple_get_tcb(&env->simple));

    THREAD_MEMORY_FENCE();

    /* Idle thread should run and get us a non-zero value */
    test_neq(ipcbuffer[BENCHMARK_IDLE_LOCALCPU_UTILISATION], (uint64_t) 0);

    /* Current thread should run (overhead of loop and calling ltimer functions) */
    test_neq(ipcbuffer[BENCHMARK_TOTAL_UTILISATION], (uint64_t) 0);

    /* Idle thread should be less than the overall utilisation */
    test_le(ipcbuffer[BENCHMARK_IDLE_LOCALCPU_UTILISATION], ipcbuffer[BENCHMARK_TOTAL_UTILISATION]);

    /*
     * We slept/blocked for 1 second, so idle thread should get scheduled at least 75% of this time.
     * It's assumed (and how sel4test works currently) that THIS thread is the highest
     * priority running thread, and all other threads are blocked, in order for the idle thread to run.
     */
    test_ge(((ipcbuffer[BENCHMARK_IDLE_LOCALCPU_UTILISATION] * 100) /
                ipcbuffer[BENCHMARK_TOTAL_UTILISATION]), (uint64_t) 50);

    return sel4test_get_result();
}
DEFINE_TEST(BENCHMARK_0001, "Test seL4 Benchmarking API - Utilisation", test_benchmark_utilisation, config_set(CONFIG_HAVE_TIMER));
#endif /* CONFIG_BENCHMARK_TRACK_UTILISATION */
