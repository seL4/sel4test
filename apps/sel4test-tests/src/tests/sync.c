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

#include <stdio.h>
#include <sync/sem.h>
#include <sync/bin_sem.h>
#include <sync/condition_var.h>
#include <stdbool.h>

#include "../test.h"
#include "../helpers.h"

static volatile int shared = 0;
sync_bin_sem_t bin_sem = {{0}};

sync_sem_t sem = {{0}};

sync_bin_sem_t monitor_lock = {{0}};
sync_cv_t consumer_cv = {{0}};
sync_cv_t producer_cv = {{0}};
sync_cv_t broadcaster_cv = {{0}};

static int
bin_sem_func(env_t env, int threadid)
{
    /* Take the semaphore */
    sync_bin_sem_wait(&bin_sem);

    /* Grab the value of the shared variable */
    int value = shared;

    /* Yield to give the other thread a chance to test the semaphore */
    seL4_Yield();

    /* Write back to the shared variable */
    value += 1;
    shared = value;

    /* Signal the semaphore */
    sync_bin_sem_post(&bin_sem);

    return 0;
}

static int
test_bin_sem(struct env *env)
{
    int error;
    helper_thread_t thread1, thread2;

    /* Create a binary semaphore */
    error = sync_bin_sem_new(&(env->vka), &bin_sem, 1);
    test_eq(error, 0);

    /* Reset the shared variable */
    shared = 0;

    /* Create some threads that need mutual exclusion */
    create_helper_thread(env, &thread1);
    create_helper_thread(env, &thread2);
    start_helper(env, &thread1, (helper_fn_t) bin_sem_func, (seL4_Word) env, 1, 0, 0);
    start_helper(env, &thread2, (helper_fn_t) bin_sem_func, (seL4_Word) env, 2, 0, 0);

    /* Wait for them to do their thing */
    wait_for_helper(&thread1);
    wait_for_helper(&thread2);
    cleanup_helper(env, &thread1);
    cleanup_helper(env, &thread2);

    /* If the semaphore worked then this will be 2 */
    test_eq(shared, 2);

    /* Clean up */
    sync_bin_sem_destroy(&env->vka, &bin_sem);

    return sel4test_get_result();
}
DEFINE_TEST(SYNC001, "libsel4sync Test binary semaphores", test_bin_sem, true)

static int
sem_func(env_t env, int threadid)
{
    /* Take the semaphore */
    sync_sem_wait(&sem);

    /* Grab the value of the shared variable */
    int value = shared;

    /* Yield to give the other thread a chance to test the semaphore */
    seL4_Yield();

    /* Write back to the shared variable */
    value += 1;
    shared = value;

    /* Signal the semaphore */
    sync_sem_post(&sem);

    return 0;
}

static int
test_sem(struct env *env)
{
    int error;
    helper_thread_t thread1, thread2;

    /* Reset the shared variable */
    shared = 0;

    /* Create a binary semaphore */
    error = sync_sem_new(&(env->vka), &sem, 1);
    test_eq(error, 0);

    /* Create some threads that need mutual exclusion */
    create_helper_thread(env, &thread1);
    create_helper_thread(env, &thread2);
    start_helper(env, &thread1, (helper_fn_t) sem_func, (seL4_Word) env, 1, 0, 0);
    start_helper(env, &thread2, (helper_fn_t) sem_func, (seL4_Word) env, 2, 0, 0);

    /* Wait for them to do their thing */
    wait_for_helper(&thread1);
    wait_for_helper(&thread2);
    cleanup_helper(env, &thread1);
    cleanup_helper(env, &thread2);

    /* If the semaphore worked then this will be 2 */
    test_eq(shared, 2);

    /* Clean up */
    sync_sem_destroy(&(env->vka), &sem);

    return sel4test_get_result();
}
DEFINE_TEST(SYNC002, "libsel4sync Test semaphores", test_sem, true)

static int
consumer_func(env_t env, int threadid)
{
    /* Take the monitor */
    sync_bin_sem_wait(&monitor_lock);

    /* Wait for a condition to be true */
    while (shared == 0) {
        sync_cv_wait(&monitor_lock, &consumer_cv);
    }

    /* Grab the value of the shared variable */
    int value = shared;

    seL4_Yield();

    /* Write back to the shared variable */
    value -= 1;
    shared = value;

    /* Signal the condition variable */
    sync_cv_signal(&producer_cv);

    /* Release the monitor */
    sync_bin_sem_post(&monitor_lock);

    return 0;
}

static int
producer_func(env_t env, int threadid)
{
    /* Take the monitor */
    sync_bin_sem_wait(&monitor_lock);

    /* Wait for a condition to be true */
    while (shared == 2) {
        sync_cv_wait(&monitor_lock, &producer_cv);
    }

    /* Grab the value of the shared variable */
    int value = shared;

    seL4_Yield();

    /* Write back to the shared variable */
    value += 1;
    shared = value;

    /* Signal the condition variable */
    sync_cv_signal(&consumer_cv);

    /* Release the monitor */
    sync_bin_sem_post(&monitor_lock);

    return 0;
}

static int
test_monitor(struct env *env)
{
    int error;
    helper_thread_t consumer_thread1, consumer_thread2;
    helper_thread_t producer_thread1, producer_thread2;

    /* Create a lock for the monitor */
    error = sync_bin_sem_new(&(env->vka), &monitor_lock, 1);
    test_eq(error, 0);

    /* Create a condition variable for consumers */
    error = sync_cv_new(&(env->vka), &consumer_cv);
    test_eq(error, 0);

    /* Create a condition variable for producers */
    error = sync_cv_new(&(env->vka), &producer_cv);
    test_eq(error, 0);

    /* Create the producer/consumer threads */
    create_helper_thread(env, &consumer_thread1);
    create_helper_thread(env, &consumer_thread2);
    create_helper_thread(env, &producer_thread1);
    create_helper_thread(env, &producer_thread2);

    shared = 1;

    start_helper(env, &consumer_thread1, (helper_fn_t) consumer_func, (seL4_Word) env, 1, 0, 0);
    start_helper(env, &consumer_thread2, (helper_fn_t) consumer_func, (seL4_Word) env, 2, 0, 0);
    start_helper(env, &producer_thread1, (helper_fn_t) producer_func, (seL4_Word) env, 3, 0, 0);
    start_helper(env, &producer_thread2, (helper_fn_t) producer_func, (seL4_Word) env, 4, 0, 0);

    /* Wait for them to do their thing */
    wait_for_helper(&consumer_thread1);
    wait_for_helper(&consumer_thread2);
    wait_for_helper(&producer_thread1);
    wait_for_helper(&producer_thread2);

    cleanup_helper(env, &consumer_thread1);
    cleanup_helper(env, &consumer_thread2);
    cleanup_helper(env, &producer_thread1);
    cleanup_helper(env, &producer_thread2);

    /* If the monitor worked then this will be 1 */
    test_eq(shared, 1);

    /* Clean up */
    sync_cv_destroy(&(env->vka), &consumer_cv);
    sync_cv_destroy(&(env->vka), &producer_cv);
    sync_bin_sem_destroy(&(env->vka), &monitor_lock);

    return sel4test_get_result();
}
DEFINE_TEST(SYNC003, "libsel4sync Test monitors", test_monitor, true)

static int
broadcaster_func(env_t env, int threadid)
{
    /* Take the monitor */
    sync_bin_sem_wait(&monitor_lock);

    /* Wait for a condition to be true */
    while (shared == 2) {
        sync_cv_wait(&monitor_lock, &broadcaster_cv);
    }

    /* Grab the value of the shared variable */
    int value = shared;

    seL4_Yield();

    /* Write back to the shared variable */
    value += 3;
    shared = value;

    /* Signal the condition variable */
    sync_cv_broadcast_release(&monitor_lock, &consumer_cv);

    return 0;
}

static int
test_monitor_broadcast(struct env *env)
{
    int error;
    helper_thread_t broadcaster_thread;
    helper_thread_t consumer_thread1, consumer_thread2, consumer_thread3;

    /* Create a lock for the monitor */
    error = sync_bin_sem_new(&(env->vka), &monitor_lock, 1);
    test_eq(error, 0);

    /* Create a condition variable for consumers */
    error = sync_cv_new(&(env->vka), &consumer_cv);
    test_eq(error, 0);

    /* Create a condition variable for the broadcaster */
    error = sync_cv_new(&(env->vka), &broadcaster_cv);
    test_eq(error, 0);

    /* Create the broadcaster/consumer threads */
    create_helper_thread(env, &consumer_thread1);
    create_helper_thread(env, &consumer_thread2);
    create_helper_thread(env, &consumer_thread3);
    create_helper_thread(env, &broadcaster_thread);

    shared = 0;

    start_helper(env, &broadcaster_thread, (helper_fn_t) broadcaster_func, (seL4_Word) env, 1, 0, 0);
    start_helper(env, &consumer_thread1, (helper_fn_t) consumer_func, (seL4_Word) env, 2, 0, 0);
    start_helper(env, &consumer_thread2, (helper_fn_t) consumer_func, (seL4_Word) env, 3, 0, 0);
    start_helper(env, &consumer_thread3, (helper_fn_t) consumer_func, (seL4_Word) env, 4, 0, 0);

    /* Wait for them to do their thing */
    wait_for_helper(&broadcaster_thread);
    wait_for_helper(&consumer_thread1);
    wait_for_helper(&consumer_thread2);
    wait_for_helper(&consumer_thread3);

    cleanup_helper(env, &consumer_thread1);
    cleanup_helper(env, &consumer_thread2);
    cleanup_helper(env, &consumer_thread3);
    cleanup_helper(env, &broadcaster_thread);

    /* If the monitor worked then this will be 0 */
    test_eq(shared, 0);

    /* Check that the broadcast is over */
    test_assert(!consumer_cv.broadcasting);

    /* Clean up */
    sync_cv_destroy(&(env->vka), &consumer_cv);
    sync_cv_destroy(&(env->vka), &broadcaster_cv);
    sync_bin_sem_destroy(&(env->vka), &monitor_lock);

    return sel4test_get_result();
}
DEFINE_TEST(SYNC004, "libsel4sync Test monitors - broadcast", test_monitor_broadcast, true)
