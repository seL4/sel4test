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
#include <sync/bin_sem.h>

#include "../test.h"
#include "../helpers.h"

static volatile int shared = 0;
sync_bin_sem_t sem = {0};

static int
do_something(env_t env, int procnum)
{
    int error;
    
    /* Take the semaphore */
    printf("Thread %d waits on sem\n", procnum);
    error = sync_bin_sem_wait(&sem);
    printf("Thread %d enters critical region\n", procnum);

    /* Grab the value of the shared variable */
    int value = shared;
    printf("Thread %d read shared = %d\n", procnum, value);

    /* Yield to give the other thread a chance to test the semaphore */
    printf("Thread %d yields\n", procnum);
    seL4_Yield();

    /* Write back to the shared variable */
    value += 1;
    shared = value;
    printf("Thread %d wrote shared = %d\n", procnum, value);

    /* Signal the semaphore */
    printf("Thread %d releases sem\n", procnum);
    error = sync_bin_sem_post(&sem);

    return 0;
}

static int
test_test(struct env *env)
{
    int error;
    helper_thread_t thread1, thread2;
    vka_object_t notification = {0};

    /* allocate a notification to use in the semaphore */
    error = vka_alloc_notification(&env->vka, &notification);
    test_eq(error, 0);

    /* Create a binary semaphore */
    error = sync_bin_sem_init(&sem, notification.cptr);
    test_eq(error, 0);

    /* Create some threads that need mutual exclusion */
    create_helper_thread(env, &thread1);
    create_helper_thread(env, &thread2);
    start_helper(env, &thread1, (helper_fn_t) do_something, (seL4_Word) env, 1, 0, 0);
    start_helper(env, &thread2, (helper_fn_t) do_something, (seL4_Word) env, 2, 0, 0);

    /* Wait for them to do their thing */
    wait_for_helper(&thread1);
    wait_for_helper(&thread2);
    cleanup_helper(env, &thread1);
    cleanup_helper(env, &thread2);

    /* If the semaphore worked then this will be 4 */
    test_eq(shared, 2);

    /* Clean up */
    sync_bin_sem_destroy(&sem);
    vka_free_object(&env->vka, &notification);

    return sel4test_get_result();
}
DEFINE_TEST(SYNC001, "libsel4sync test", test_test)
