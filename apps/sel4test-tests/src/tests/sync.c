/*
 * Copyright 2016, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <stdio.h>
#include <sync/sem.h>
#include <sync/bin_sem.h>

#include "../test.h"
#include "../helpers.h"

static volatile int shared = 0;
sync_bin_sem_t bin_sem = {0};

sync_sem_t sem = {0};

static int
bin_sem_func(env_t env, int threadid)
{
    int error;
    
    /* Take the semaphore */
    printf("Thread %d waits on bin_sem\n", threadid);
    error = sync_bin_sem_wait(&bin_sem);
    printf("Thread %d enters critical region\n", threadid);

    /* Grab the value of the shared variable */
    int value = shared;
    printf("Thread %d read shared = %d\n", threadid, value);

    /* Yield to give the other thread a chance to test the semaphore */
    printf("Thread %d yields\n", threadid);
    seL4_Yield();

    /* Write back to the shared variable */
    value += 1;
    shared = value;
    printf("Thread %d wrote shared = %d\n", threadid, value);

    /* Signal the semaphore */
    printf("Thread %d releases bin_sem\n", threadid);
    error = sync_bin_sem_post(&bin_sem);

    return 0;
}

static int
test_bin_sem(struct env *env)
{
    int error;
    helper_thread_t thread1, thread2;
    vka_object_t notification = {0};

    /* allocate a notification to use in the semaphore */
    error = vka_alloc_notification(&env->vka, &notification);
    test_eq(error, 0);

    /* Create a binary semaphore */
    error = sync_bin_sem_init(&bin_sem, notification.cptr);
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

    /* If the semaphore worked then this will be 4 */
    test_eq(shared, 2);

    /* Clean up */
    sync_bin_sem_destroy(&bin_sem);
    vka_free_object(&env->vka, &notification);

    return sel4test_get_result();
}
DEFINE_TEST(SYNC001, "libsel4sync Test binary semaphores", test_bin_sem)

static int
sem_func(env_t env, int threadid)
{
    int error;
    
    /* Take the semaphore */
    printf("Thread %d waits on sem\n", threadid);
    error = sync_sem_wait(&sem);
    printf("Thread %d enters critical region\n", threadid);

    /* Grab the value of the shared variable */
    int value = shared;
    printf("Thread %d read shared = %d\n", threadid, value);

    /* Yield to give the other thread a chance to test the semaphore */
    printf("Thread %d yields\n", threadid);
    seL4_Yield();

    /* Write back to the shared variable */
    value += 1;
    shared = value;
    printf("Thread %d wrote shared = %d\n", threadid, value);

    /* Signal the semaphore */
    printf("Thread %d releases sem\n", threadid);
    error = sync_sem_post(&sem);

    return 0;
}

static int
test_sem(struct env *env)
{
    int error;
    helper_thread_t thread1, thread2;
    vka_object_t endpoint = {0};

    /* allocate a notification to use in the semaphore */
    error = vka_alloc_endpoint(&env->vka, &endpoint);
    test_eq(error, 0);

    /* Reset the shared variable */
    shared = 0;

    /* Create a binary semaphore */
    error = sync_sem_init(&sem, endpoint.cptr, 1);
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

    /* If the semaphore worked then this will be 4 */
    test_eq(shared, 2);

    /* Clean up */
    sync_sem_destroy(&sem);
    vka_free_object(&env->vka, &endpoint);

    return sel4test_get_result();
}
DEFINE_TEST(SYNC002, "libsel4sync Test semaphores", test_sem)
