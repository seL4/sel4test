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
#include <string.h>
#include <stdio.h>

#include <sel4/sel4.h>
#include <vka/capops.h>
#include <allocman/allocman.h>
#include <allocman/vka.h>
#include <allocman/bootstrap.h>
#include <sel4utils/thread.h>
#include <serial_server/parent.h>
#include <serial_server/client.h>

#include "../test.h"
#include "../helpers.h"

#define SERSERV_TEST_PRIO_SERVER    (seL4_MaxPrio - 1)
#define SERSERV_TEST_PRIO_CLIENT    (seL4_MinPrio)

#define SERSERV_TEST_N_CLIENTS      (1)

#define SERSERV_TEST_ALLOCMAN_PREALLOCATED_MEMSIZE (64 * 1024)
#define SERSERV_TEST_UT_SIZE        (512 * 1024)

static const char *test_str = "Hello, world!\n";


/* These next few tests test the same things from client threads.
 *
 * We spawn a bunch of helper threads and then ask those helper threads to
 * run these same tests, and then have the helpers return the values they
 * got when they called the Server.
 *
 * We keep it simple as well by using some global state to pass data to the
 * client threads so they can do their work. Then, so that each client knows
 * which one it is, we pass the clients a numeric ID invented by the test
 * to identify that client.
 */
typedef struct _client_test_data
{
    helper_thread_t thread;
    /* "badged_server_ep_cspath2" is used in the disconnect/reconnect tests:
     * the client can't reconnect using the same badged ep as the first
     * connection.
     *
     * We could make each client seL4_Call() the main thread and ask
     * it to re-mint the ep-cap to each client, but that imposes extra
     * complexity for a simple test. So for that particular test we just mint 2
     * ep caps from the start.
     */
    volatile void **parent_used_pages_list, *client_used_pages_list;
    int n_used_pages;
    cspacepath_t badged_server_ep_cspath, badged_server_ep_cspath2, ut_512k_cspath;
    serial_client_context_t conn;
} client_test_data_t;

typedef int (client_test_fn)(client_test_data_t *, vka_t *, vspace_t *);

struct
{
    vka_t *vka;
    vspace_t *vspace;
    simple_t *simple;
    client_test_data_t client_test_data[SERSERV_TEST_N_CLIENTS];
    uint8_t allocman_mem[SERSERV_TEST_N_CLIENTS][SERSERV_TEST_ALLOCMAN_PREALLOCATED_MEMSIZE];
} static client_globals;

/* These next few functions (client_*_main())are the actual tests that are run
 * by the client threads and processes for the concurrency tests.
 */

/** This function will guide a client thread to connect to the server and return the
 * result.
 */
static int
client_connect_main(client_test_data_t *self, vka_t *vka, vspace_t *vspace)
{
    int error = 0;
    serial_client_context_t conn;

    error = serial_server_client_connect(self->badged_server_ep_cspath.capPtr,
                                         vka, vspace,
                                         &conn);
    return error;
}

/** This function will guide a client thread to connect to the server, send a
 * printf() request, and return the result.
 */
static int
client_printf_main(client_test_data_t *self, vka_t *vka, vspace_t *vspace)
{
    int error;
    serial_client_context_t conn;

    error = serial_server_client_connect(self->badged_server_ep_cspath.capPtr,
                                         vka, vspace,
                                         &conn);
    if (error != 0) {
        return error;
    }

    error = serial_server_printf(&conn, test_str);
    if (error != strlen(test_str)) {
        return -1;
    }
    return 0;
}

/** This function will guide a client thread to connect to the server, send a
 * write() request, and return the result.
 */
static int
client_write_main(client_test_data_t *self, vka_t *vka, vspace_t *vspace)
{
    int error;
    serial_client_context_t conn;

    error = serial_server_client_connect(self->badged_server_ep_cspath.capPtr,
                                         vka, vspace,
                                         &conn);
    if (error != 0) {
        return error;
    }

    error = serial_server_write(&conn, test_str, strlen(test_str));
    if (error != strlen(test_str)) {
            return -1;
    }
    return 0;
}

/** This function will guide a client thread to connect to the server, send a
 * printf() request, then a write() request, then disconnect(), and then
 * reconnect to the Server and send another printf() and another write(),
 * and return the result.
 */
static int
client_disconnect_reconnect_printf_write_main(client_test_data_t *self, vka_t *vka, vspace_t *vspace)
{
    int error;
    serial_client_context_t conn;

    error = serial_server_client_connect(self->badged_server_ep_cspath.capPtr,
                                         vka, vspace,
                                         &conn);
    if (error != 0) {
        return error;
    }
    error = serial_server_printf(&conn, test_str);
    if (error != strlen(test_str)) {
        return -1;
    }
    error = serial_server_write(&conn, test_str, strlen(test_str));
    if (error != strlen(test_str)) {
        return error;
    }

    serial_server_disconnect(&conn);

    error = serial_server_client_connect(self->badged_server_ep_cspath2.capPtr,
                                         vka, vspace,
                                         &conn);
    if (error != 0) {
        return error;
    }
    error = serial_server_printf(&conn, test_str);
    if (error != strlen(test_str)) {
        return -1;
    }
    error = serial_server_write(&conn, test_str, strlen(test_str));
    if (error != strlen(test_str)) {
        return -1;
    }
    return 0;
}

/** This function will guide a client thread to connect to the server, and then
 * send a kill() request.
 */
static int
client_disconnect_main(client_test_data_t *self, vka_t *vka, vspace_t *vspace)
{
    int error;
    serial_client_context_t conn;

    error = serial_server_client_connect(self->badged_server_ep_cspath.capPtr,
                                         vka, vspace,
                                         &conn);
    if (error != 0) {
        return error;
    }

    serial_server_disconnect(&conn);
    return 0;
}

static void
init_file_globals(struct env *env)
{
    memset(client_globals.client_test_data, 0,
           sizeof(client_globals.client_test_data));

    memset(client_globals.allocman_mem, 0, sizeof(client_globals.allocman_mem));

    client_globals.vka = &env->vka;
    client_globals.vspace = &env->vspace;
    client_globals.simple = &env->simple;
}

static int
mint_server_ep_to_clients(struct env *env, bool mint_2nd_ep)
{
    int error;

    for (int i = 0; i < SERSERV_TEST_N_CLIENTS; i++) {
        /* Ask the serserv library to mint the Server's EP to all the new clients. */
        client_test_data_t *curr_client = &client_globals.client_test_data[i];

        if (curr_client->thread.is_process) {
            curr_client->badged_server_ep_cspath.capPtr = serial_server_parent_mint_endpoint_to_process(
                                                &curr_client->thread.process);
            if (curr_client->badged_server_ep_cspath.capPtr == 0) {
                return -1;
            }
        } else {
            error = serial_server_parent_vka_mint_endpoint(&env->vka,
                                                           &curr_client->badged_server_ep_cspath);
            if (error != 0) {
                return -1;
            }
        }

        if (mint_2nd_ep) {
            if (curr_client->thread.is_process) {
                curr_client->badged_server_ep_cspath2.capPtr = serial_server_parent_mint_endpoint_to_process(
                                                    &curr_client->thread.process);
                if (curr_client->badged_server_ep_cspath2.capPtr == 0) {
                    return -1;
                }
            } else {
                error = serial_server_parent_vka_mint_endpoint(&env->vka,
                                                               &curr_client->badged_server_ep_cspath2);
                if (error != 0) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

static void
create_clients(struct env *env, bool is_process)
{
    for (int i = 0; i < SERSERV_TEST_N_CLIENTS; i++) {
        client_test_data_t *curr_client = &client_globals.client_test_data[i];

        if (is_process) {
            create_helper_process(env, &curr_client->thread);
        } else {
            create_helper_thread(env, &curr_client->thread);
        }
    }
}

/** Guides a client "process" to set up a vspace_t instance for its new VSpace.
 *
 * This basically entails setting up an allocman_t instance, followed by a
 * vka_t instance and finally a vspace_t instance. The function requires an
 * untyped of a "reasonable" size (you determine this based on what this new
 * process will be doing), a list of used pages in the new process' VSpace,
 * some allocman initial mem and your pointers.
 *
 * @param ut_cap CPtr to an untyped of "reasonable" size that was copied to the
 *               new process before this function was invoked.
 * @param ut_size_bits The size in bits of the untyped "ut_cap".
 * @param used_pages_list A NULL-terminated list of virtual pages that are
 *                        already occupied within this new process' VSpace.
 * @param allocman_mem Initial memory set aside for bootstrapping an allocman_t.
 * @param allocman_mem_size Size of the initial allocman_t memory.
 * @param allocman[out] Pointer to a pointer to an uninitialized allocman_t
 *                      instance. Will be returned with an initialized
 *                      allocman_t if this function completes successfully.
 * @param vka[out] Pointer to an uninitialized vka_t instance.
 * @param vspace[out] Pointer to an uninitialized vspace_t instance.
 * @param alloc_data[out] Pointer to an uninitialized alloc data structure.
 * @return 0 on success; non-zero on error.
 */
static int
setup_client_process_allocman_vka_and_vspace(seL4_CPtr ut_cap, size_t ut_size_bits,
                                             seL4_CPtr first_free_cptr,
                                             volatile void **used_pages_list,
                                             uint8_t *allocman_mem, size_t allocman_mem_size,
                                             allocman_t **allocman,
                                             vka_t *vka, vspace_t *vspace,
                                             sel4utils_alloc_data_t *alloc_data)
{
    cspacepath_t ut_cspath;
    uintptr_t paddr;
    int error;

    if (used_pages_list == NULL || allocman_mem == NULL
        || allocman == NULL || vspace == NULL || vka == NULL
        || alloc_data == NULL || allocman_mem_size == 0) {
        return seL4_InvalidArgument;
    }
    *allocman = bootstrap_use_current_1level(SEL4UTILS_CNODE_SLOT,
                                             CONFIG_SEL4UTILS_CSPACE_SIZE_BITS,
                                             first_free_cptr,
                                             BIT(CONFIG_SEL4UTILS_CSPACE_SIZE_BITS),
                                             allocman_mem_size,
                                             allocman_mem);
    if (*allocman == NULL) {
        return -1;
    }

    allocman_make_vka(vka, *allocman);
    vka_cspace_make_path(vka, ut_cap, &ut_cspath);
    error = allocman_utspace_add_uts(*allocman, 1, &ut_cspath,
                                     &ut_size_bits,
                                     &paddr, ALLOCMAN_UT_KERNEL);
    if (error != 0) {
        return error;
    }

    /* Pass the shmem page containing the list of already allocated frames, to
     * the vspace bootstrap.
     */
    return sel4utils_bootstrap_vspace_leaky(vspace, alloc_data,
                                      SEL4UTILS_PD_SLOT, vka,
                                      (void **)used_pages_list);
}

/** Common entry point for all client thread and processes alike.
 *
 * This is what is passed to start_helper so that it can do its init work.
 * This function invokes the actual test routine after it finishes working.
 *
 * Determines whether the new client is a thread or process. If it's a process
 * we do some extra processing, vis-a-vis setting up the new process' own
 * vka, vspace and allocman, so it can call the library.
 *
 * @param _test_fn The actual test function that is to be invoked.
 * @param _used_pages_list List of used virtual pages in the VSpace of the new
 *                         client. This is used to initialize the new client's
 *                         vspace_t instance.
 * @param n_used_pages Number of elements in the _used_pages_list.
 * @param thread_num The "ID" of this thread as the parent assigned it.
 * @return 0 on success; non-zero on error.
 */
static int client_common_entry_point(seL4_Word _test_fn, seL4_Word _used_pages_list,
                                  seL4_Word n_used_pages, seL4_Word thread_num)
{
    bool is_process;
    client_test_data_t *client_data;
    volatile void **used_pages_list = (volatile void **)_used_pages_list;
    client_test_fn *test_fn = (client_test_fn *)_test_fn;
    int error;
    vka_t *vka, _vka;
    vspace_t *vspace, _vspace;
    sel4utils_alloc_data_t alloc_data;
    allocman_t *allocman;

    is_process = used_pages_list != NULL;

    if (is_process) {
        /* +1 because the list is NULL terminated and we want to get past the
         * end of the list.
         */
        client_data = (client_test_data_t *)&used_pages_list[n_used_pages + 1];
        vka = &_vka;
        vspace = &_vspace;
        error = setup_client_process_allocman_vka_and_vspace(client_data->ut_512k_cspath.capPtr,
                                                             BYTES_TO_SIZE_BITS(SERSERV_TEST_UT_SIZE),
                                                             client_data->ut_512k_cspath.capPtr + 0x8,
                                                             used_pages_list,
                                                             client_globals.allocman_mem[thread_num],
                                                             SERSERV_TEST_ALLOCMAN_PREALLOCATED_MEMSIZE,
                                                             &allocman,
                                                             vka, vspace,
                                                             &alloc_data);
        test_eq(error, 0);
    } else {
        client_data = &client_globals.client_test_data[thread_num];
        vka = client_globals.vka;
        vspace = client_globals.vspace;
    }
    return test_fn(client_data, vka, vspace);
}

static void
start_clients(struct env *env, client_test_fn *test_fn)
{
    bool is_process = client_globals.client_test_data[0].thread.is_process;

    for (int i = 0; i < SERSERV_TEST_N_CLIENTS; i++) {
        client_test_data_t *curr_client = &client_globals.client_test_data[i];
        /* The client threads don't need a list of used pages. The client
         * processes do need it -- so if the used pages list is NULL, the newly
         * spawned client assumes it's a thread and not a process.
         */
        volatile void **used_pages_list = ((is_process)
                        ? (volatile void **)curr_client->client_used_pages_list
                        : NULL);

        set_helper_priority(env, &curr_client->thread, SERSERV_TEST_PRIO_CLIENT);
        start_helper(env, &curr_client->thread, &client_common_entry_point,
                     (seL4_Word)test_fn,
                     (seL4_Word)used_pages_list,
                     curr_client->n_used_pages,
                     i);
    }
}

static void
cleanup_clients(struct env *env)
{
    for (int i = 0; i < SERSERV_TEST_N_CLIENTS; i++) {
        helper_thread_t *curr_client = &client_globals.client_test_data[i].thread;
        cleanup_helper(env, curr_client);
    }
}

/** Compiles a list of pages that libsel4utils allocated while creating a
 * thread in a new VSpace.
 *
 * This is useful if you want your new process to be able to initialize its own
 * vspace_t, because you will need to have a list of pages that are occupied in
 * the new VSpace, in order to get a usable vspace_t for that VSpace.
 * You need to allocate the memory for page_list yourself.
 *
 * @param page_list The base vaddr of a page in the PARENT VSpace (the thread
 *                  that is spawning the new thread), that will be used as the
 *                  shared mem for telling the new thread which pages in its
 *                  VSpace are occupied.
 * @param p Initialized sel4utils_process_t handle for the new thread.
 * @return 0 on success; Positive integer count of the number of used pages
 *         whose vaddrs were filled into the page_list on success.
 */
static int
fill_sel4utils_used_pages_list(volatile void **page_list, sel4utils_process_t *p)
{
    seL4_Word sel4utils_stack_n_pages, sel4utils_stack_base_vaddr;

    if (page_list == NULL || p == NULL) {
        return -1;
    }

    /* Need to fill in the vaddrs for its stack and IPC buffer.
     */

    /* libsel4utils adds another page for guard */
    sel4utils_stack_n_pages = BYTES_TO_4K_PAGES(CONFIG_SEL4UTILS_STACK_SIZE) + 1;
    sel4utils_stack_base_vaddr = (uintptr_t)p->thread.stack_top - (sel4utils_stack_n_pages * BIT(seL4_PageBits));
    for (int i = 0; i < sel4utils_stack_n_pages; i++) {
        *page_list = (void *)sel4utils_stack_base_vaddr;
        sel4utils_stack_base_vaddr += BIT(seL4_PageBits);
        page_list++;
    }

    /* Next take care of the IPC buffer */
    *page_list = (void *)p->thread.ipc_buffer_addr;
    page_list++;
    /* NULL terminate the list */
    *page_list = NULL;

    /* Stack pages + IPC buffer */
    return sel4utils_stack_n_pages + 1;
}

/** Shares a used page list to a target process that was created by sel4utils.
 *
 * @param page_list Page that has been filled out with a list of pages used by
 *                  sel4utils_configure_process.
 * @param n_entries Number of entries in the page_list.
 * @param from_vspace Initialized vspace_t for the parent process (that is doing
 *                    the spawning).
 * @param p Initialized sel4utils_process_t for the new process.
 * @param target_vaddr[out] The vaddr that the NEW process can access the
 *                          used pages list at, within ITS own VSpace.
 * @return Negative integer on failure. Positive integer on success, which
 *         represents the number of ADDITIONAL used pages filled in by this
 *         function invocation.
 */
static int share_sel4utils_used_pages_list(volatile void **page_list, size_t n_entries,
                                           vspace_t *from_vspace,
                                           sel4utils_process_t *p,
                                           volatile void **target_vaddr)
{
    vspace_t *to_vspace;

    if (page_list == NULL || from_vspace == NULL || p == NULL
        || target_vaddr == NULL) {
        return -1;
    }

    to_vspace = &p->vspace;
    *target_vaddr = vspace_share_mem(from_vspace, to_vspace,
                                     (void *)ALIGN_DOWN((uintptr_t)page_list, BIT(seL4_PageBits)),
                                     1, seL4_PageBits,
                                     seL4_AllRights, true);
    if (*target_vaddr == NULL) {
        return -1;
    }
    /* Don't forget to add the offset back if the page_list vaddr was not
     * page aligned
     */
    *target_vaddr = (void *)((uintptr_t)*target_vaddr
                    + ((uintptr_t)page_list & (BIT(seL4_PageBits) - 1)));
    /* Next, take care of the shmem page we ourselves created in this
     * function
     */
    page_list[n_entries] = *target_vaddr;
    return 1;
}

/** For each new client process, fill out a list of virtual pages that will be
 * occupied in its VSpace before it even begins executing.
 *
 * The client will use this list to initialize a vspace_t instance. This is
 * called in the Parent, before the clients are allowed to execute.
 * @return 0 on success; non-zero on error.
 */
static int
setup_pclient_used_pages_lists(struct env *env)
{
    int error;

    for (int i = 0; i < SERSERV_TEST_N_CLIENTS; i++) {
        client_test_data_t *curr_client = &client_globals.client_test_data[i];

        /* Allocate the page in our VSpace */
        curr_client->parent_used_pages_list = (volatile void **)vspace_new_pages(
                                                            &env->vspace,
                                                            seL4_AllRights,
                                                            1, seL4_PageBits);
        if (curr_client->parent_used_pages_list == NULL) {
            return -1;
        }

        /* Give the page to fill_sel4utils_used_pages and let it fill out the
         * pages it used in the new process' VSpace.
         */
        curr_client->n_used_pages = fill_sel4utils_used_pages_list(
                                               curr_client->parent_used_pages_list,
                                               &curr_client->thread.process);
        if (curr_client->n_used_pages < 0) {
            return -1;
        }

        /* Share the page containing the used pages list with the new process */
        error = share_sel4utils_used_pages_list(curr_client->parent_used_pages_list,
                                                curr_client->n_used_pages,
                                                &env->vspace,
                                                &curr_client->thread.process,
                                                &curr_client->client_used_pages_list);
        if (error < 0) {
            return -1;
        }
        curr_client->n_used_pages += error;
        /* NULL terminate the list since we added to it. */
        curr_client->parent_used_pages_list[curr_client->n_used_pages] = NULL;
    }
    return 0;
}

/** The new client processes each need an untyped of some size so they can
 * allocate memory.
 *
 * 512k seems reasonably large (if not a little too large). Allocate an untyped
 * for each new client process. This is called in the parent, before the test
 * clients are allowed to execute.
 * @return 0 on success; Non-zero on error.
 */
static int
alloc_untypeds_for_clients(struct env *env)
{
    int error;

    for (int i = 0; i < SERSERV_TEST_N_CLIENTS; i++) {
        client_test_data_t *curr_client = &client_globals.client_test_data[i];
        vka_object_t tmp;

        error = vka_alloc_untyped(&env->vka, BYTES_TO_SIZE_BITS(SERSERV_TEST_UT_SIZE), &tmp);
        if (error != 0) {
            return error;
        }

        vka_cspace_make_path(&env->vka, tmp.cptr, &curr_client->ut_512k_cspath);
        curr_client->ut_512k_cspath.capPtr = sel4utils_move_cap_to_process(
                                                &curr_client->thread.process,
                                                curr_client->ut_512k_cspath,
                                                &env->vka);
        if (curr_client->ut_512k_cspath.capPtr == 0) {
            return -1;
        }
    }
    return 0;
}

static void
copy_client_data_to_shmem(struct env *env)
{
    for (int i = 0; i < SERSERV_TEST_N_CLIENTS; i++) {
        client_test_data_t *curr_client = &client_globals.client_test_data[i];
        /* +1 because the list is NULL terminated and we want to get past the
         * end of the list.
         */
        void *client_data_copy = &curr_client->parent_used_pages_list[
                                    curr_client->n_used_pages + 1];

        memcpy(client_data_copy, curr_client, sizeof(*curr_client));
    }
}

static int
concurrency_test_common(struct env *env, bool is_process, client_test_fn *test_fn, bool mint_2nd_ep_cap)
{
    int error;

    error = serial_server_parent_spawn_thread(&env->simple,
                                              &env->vka, &env->vspace,
                                              SERSERV_TEST_PRIO_SERVER);

    test_eq(error, 0);

    init_file_globals(env);
    create_clients(env, is_process);
    error = mint_server_ep_to_clients(env, mint_2nd_ep_cap);
    test_eq(error, 0);

    if (is_process) {
        /* Setup the extra things that a new VSpace/CSpace needs.
         */
        error = setup_pclient_used_pages_lists(env);
        test_eq(error, 0);
        error = alloc_untypeds_for_clients(env);
        test_eq(error, 0);
        copy_client_data_to_shmem(env);
    };

    start_clients(env, test_fn);

    for (int i = 0; i < SERSERV_TEST_N_CLIENTS; i++) {
        error = wait_for_helper(&client_globals.client_test_data[i].thread);
        test_eq(error, 0);
    }

    cleanup_clients(env);
    return 0;
}

static int test_client_connect(struct env *env)
{
    int error;

    error = concurrency_test_common(env, false, &client_connect_main, false);
    test_eq(error, 0);
    return sel4test_get_result();
}
DEFINE_TEST(SERSERV_CLIENT_001, "Connect from client threads",
            test_client_connect, true)

static int
test_client_printf(struct env *env)
{
    int error;

    error = concurrency_test_common(env, false, &client_printf_main, false);
    test_eq(error, 0);
    return sel4test_get_result();
}
DEFINE_TEST(SERSERV_CLIENT_002, "Printf from client threads",
            test_client_printf, true)

static int
test_client_write(struct env *env)
{
    int error;

    error = concurrency_test_common(env, false, &client_write_main, false);
    test_eq(error, 0);
    return sel4test_get_result();
}
DEFINE_TEST(SERSERV_CLIENT_003, "Write from client threads",
            test_client_write, true)

static int
test_client_disconnect_reconnect_printf_write(struct env *env)
{
    int error;

    error = concurrency_test_common(env, false, &client_disconnect_reconnect_printf_write_main, true);
    test_eq(error, 0);
    return sel4test_get_result();
}
DEFINE_TEST(SERSERV_CLIENT_004, "Printf, then write, then reset connection, and "
            "Printf, then write again, from client threads",
            test_client_disconnect_reconnect_printf_write, true)

static int
test_client_kill(struct env *env)
{
    int error;
    cspacepath_t parent_badged_server_ep_cspath;
    serial_client_context_t conn;

    error = concurrency_test_common(env, false, &client_disconnect_main, false);
    test_eq(error, 0);

    error = serial_server_parent_vka_mint_endpoint(&env->vka,
                                                   &parent_badged_server_ep_cspath);
    test_eq(error, 0);

    error = serial_server_client_connect(parent_badged_server_ep_cspath.capPtr,
                                         &env->vka, &env->vspace, &conn);
    test_eq(error, 0);

    error = serial_server_kill(&conn);
    test_eq(error, 0);
    return sel4test_get_result();
}
DEFINE_TEST(SERSERV_CLIENT_005, "Connect, then disconnect from server on all "
            "threads, then kill server from parent thread",
            test_client_kill, true)

static int
test_client_process_connect(struct env *env)
{
    int error;

    error = concurrency_test_common(env, true, &client_connect_main, false);
    test_eq(error, 0);
    return sel4test_get_result();
}
DEFINE_TEST(SERSERV_CLI_PROC_001, "Connect to server from a client in another "
            "VSpace and CSpace", test_client_process_connect, true)

static int
test_client_process_printf(struct env *env)
{
    int error;

    error = concurrency_test_common(env, true, &client_printf_main, false);
    test_eq(error, 0);
    return sel4test_get_result();
}
DEFINE_TEST(SERSERV_CLI_PROC_002, "Connect to server and printf(, true), from a client "
            "in another VSpace and CSpace", test_client_process_printf, true)

static int
test_client_process_write(struct env *env)
{
    int error;

    error = concurrency_test_common(env, true, &client_write_main, false);
    test_eq(error, 0);
    return sel4test_get_result();
}
DEFINE_TEST(SERSERV_CLI_PROC_003, "Connect to server and write(, true), from a client "
            "in another VSpace and CSpace", test_client_process_write, true)

static int
test_client_process_disconnect_reconnect_printf_write(struct env *env)
{
    int error;

    error = concurrency_test_common(env, true,
                                    &client_disconnect_reconnect_printf_write_main,
                                    true);
    test_eq(error, 0);
    return sel4test_get_result();
}
DEFINE_TEST(SERSERV_CLI_PROC_004, "Connect to server, printf(), write(, true), then "
            "disconnect, then reconnect and printf() and write() again, from "
            "clients in other VSpaces and CSpaces",
            test_client_process_disconnect_reconnect_printf_write, true)

static int
test_client_process_kill(struct env *env)
{
    int error;
    serial_client_context_t conn;
    cspacepath_t badged_server_ep_cspath;

    error = concurrency_test_common(env, true, &client_disconnect_main, false);
    test_eq(error, 0);

    error = serial_server_parent_vka_mint_endpoint(&env->vka, &badged_server_ep_cspath);
    test_eq(error, 0);

    error = serial_server_client_connect(badged_server_ep_cspath.capPtr,
                                         &env->vka, &env->vspace, &conn);
    test_eq(error, 0);
    error = serial_server_kill(&conn);
    test_eq(error, 0);
    return sel4test_get_result();
}
DEFINE_TEST(SERSERV_CLI_PROC_005, "Connect to server then disconnect on all "
            "clients (in different VSpace/CSpace containers), and finally kill "
            "the server from the parent",
            test_client_process_kill, true)
