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

#include "../helpers.h"

static double
fpu_calculation(void)
{
    double a = (double)3.141;
    for (int i = 0; i < 10000; i++) {
        a = a * 1.123 + (a / 3);
        a /= 1.111;
        while (a > 100.0) {
            a /= 3.1234563212;
        }
        while (a < 2.0) {
            a += 1.1232132131;
        }
    }

    return a;
}

/*
 * Ensure basic FPU functionality works.
 *
 * For processors without a FPU, this tests that maths libraries link
 * correctly.
 */
static int
test_fpu_trivial(env_t env)
{
    int i;
    volatile double b;
    double a = (double)3.141592653589793238462643383279;

    for (i = 0; i < 100; i++) {
        a = a * 3 + (a / 3);
    }
    b = a;
    (void)b;
    return sel4test_get_result();
}
DEFINE_TEST(FPU0000, "Ensure that simple FPU operations work", test_fpu_trivial, true)

static int
fpu_worker(seL4_Word p1, seL4_Word p2, seL4_Word p3, seL4_Word p4)
{
    volatile double *state = (volatile double *)p1;
    int num_iterations = p2;
    static volatile int preemption_counter = 0;
    int num_preemptions = 0;

    while (num_iterations >= 0) {
        int counter_init = preemption_counter;

        /* Do some random calculation (where we know the result). */
        double a = fpu_calculation();

        /* It's workaround to solve precision discrepancy when comparing
         * floating value in FPU from different sources */
        *state = a;
        a = *state;

        /* Determine if we were preempted mid-calculation. */
        if (counter_init != preemption_counter) {
            num_preemptions++;
        }
        preemption_counter++;

        num_iterations--;
    }

    return num_preemptions;
}

/*
 * Test that multiple threads using the FPU simulataneously can't corrupt each
 * other.
 *
 * Early versions of seL4 had a bug here because we were not context-switching
 * the FPU at all. Oops.
 */
static int
test_fpu_multithreaded(struct env* env)
{
    const int NUM_THREADS = 4;
    helper_thread_t thread[NUM_THREADS];
    volatile double thread_state[NUM_THREADS];
    seL4_Word iterations = 1;
    int num_preemptions = 0;

    /*
     * We keep increasing the number of iterations ours users should calculate
     * for until they notice themselves being preempted a few times.
     */
    do {
        /* Start the threads running. */
        for (int i = 0; i < NUM_THREADS; i++) {
            create_helper_thread(env, &thread[i]);
            set_helper_priority(env, &thread[i], 100);
            start_helper(env, &thread[i], fpu_worker,
                         (seL4_Word) &thread_state[i], iterations, 0, 0);
        }

        /* Wait for the threads to finish. */
        num_preemptions = 0;
        for (int i = 0; i < NUM_THREADS; i++) {
            num_preemptions += wait_for_helper(&thread[i]);
            cleanup_helper(env, &thread[i]);
        }

        /* Ensure they all got the same result. An assert failure here
         * indicates FPU corrupt (i.e., a kernel bug). */
        for (int i = 0; i < NUM_THREADS; i++) {
            test_assert(thread_state[i] == thread_state[(i + 1) % NUM_THREADS]);
        }

        /* If we didn't get enough preemptions, restart everything again. */
        iterations *= 2;
    } while (num_preemptions < 20);

    return sel4test_get_result();
}
DEFINE_TEST(FPU0001, "Ensure multiple threads can use FPU simultaneously", test_fpu_multithreaded, !config_set(CONFIG_FT))

static int
smp_fpu_worker(volatile seL4_Word *ex, volatile seL4_Word *run)
{
    double a = 0;
    while (*run) {
        /* Do some random calculation where we know the result in 'ex'. */
        a = fpu_calculation();

        /* Values should always be the same.
         * We use 'memcmp' to compare the results as otherwise this comparison could happen
         * in FPU directly but 'a' is already in FPU and 'ex' is copied from register.
         * This may result in different precision when comparing in FPU */
        if (memcmp(&a, (seL4_Word *) ex, sizeof(double)) != 0) {
            return 1;
        }

        a = 0;
    }
    return 0;
}

int smp_test_fpu(env_t env)
{
    volatile seL4_Word run = 1;
    volatile double ex = fpu_calculation();
    helper_thread_t t[env->cores];
    ZF_LOGD("smp_test_fpu\n");

    for(int i = 0; i < env->cores; i++) {
        create_helper_thread(env, &t[i]);

        set_helper_affinity(env, &t[i], i);
        start_helper(env, &t[i], (helper_fn_t) smp_fpu_worker, (seL4_Word) &ex, (seL4_Word) &run, 0, 0);
    }

    /* Lets threads check in and do some calculation */
    sel4test_sleep(env, 10 * NS_IN_MS);

    /* Do lots of migrations */
    for(int it = 0; it < 100; it++) {
        for(int i = 0; i < env->cores; i++) {
            /* Migrate threads to next core... */
            set_helper_affinity(env, &t[i], (i + 1) % env->cores);
        }
        /* Lets do some calculation */
        sel4test_sleep(env, 10 * NS_IN_MS);
    }

    /* Notify threads to return */
    run = 0;

    for(int i = 0; i < env->cores; i++) {
        test_check(wait_for_helper(&t[i]) == 0);
        cleanup_helper(env, &t[i]);
    }

    return sel4test_get_result();
}
DEFINE_TEST(FPU0002, "Test FPU remain valid across core migration", smp_test_fpu, config_set(CONFIG_MAX_NUM_NODES) && config_set(CONFIG_HAVE_TIMER) && CONFIG_MAX_NUM_NODES > 1)
