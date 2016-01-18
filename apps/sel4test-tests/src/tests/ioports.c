/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include <autoconf.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sel4/sel4.h>
#include <sel4/messages.h>
#include <sel4utils/arch/util.h>

#include <vka/object.h>

#include "../test.h"
#include "../helpers.h"

#ifdef CONFIG_X86

#define START_PORT 0
#define END_PORT BIT(16)
#define PORT_STRIDE 256
#define EXPECTED_FAULTS ((END_PORT - START_PORT) / PORT_STRIDE)

static volatile int total_faults = 0;

static void
increment_pc(seL4_CPtr tcb, seL4_Word inc)
{
    int error;
    seL4_UserContext ctx;
    error = seL4_TCB_ReadRegisters(tcb,
                                   false,
                                   0,
                                   sizeof(ctx) / sizeof(seL4_Word),
                                   &ctx);
#ifdef CONFIG_ARCH_X86_64
    ctx.rax = 1;
    ctx.rip += inc;
#else
    ctx.eax = 1;
    ctx.eip += inc;
#endif
    error = seL4_TCB_WriteRegisters(tcb,
                                    true,
                                    0,
                                    sizeof(ctx) / sizeof(seL4_Word),
                                    &ctx);
    test_assert_fatal(!error);
}


static int
handle_fault(seL4_CPtr fault_ep, seL4_CPtr tcb, seL4_Word expected_fault, void *unused)
{
    seL4_MessageInfo_t tag;
    seL4_Word sender_badge = 0;

    while(1) {
        tag = seL4_Recv(fault_ep, &sender_badge);

        test_check(seL4_MessageInfo_get_label(tag) == SEL4_USER_EXCEPTION_LABEL);

        total_faults++;
        increment_pc(tcb, 1);
    }

    return 0;
}

static int
do_ioports(int arg1, int arg2, int arg3, int arg4)
{
    unsigned int i;
    for (i = START_PORT; i < END_PORT; i += PORT_STRIDE) {
        volatile unsigned char dummy = 0;
        asm volatile("inb %1,%0"
            : "=a"(dummy)
            : "dN"((uint16_t)i)
        );
        test_check(dummy == 1);
    }
    return 0;
}

static int
test_native_ioports(env_t env)
{
    helper_thread_t handler_thread;
    helper_thread_t faulter_thread;
    int error;
    seL4_Word handler_arg0, handler_arg1;
    /* The endpoint on which faults are received. */
    seL4_CPtr fault_ep = vka_alloc_endpoint_leaky(&env->vka);
    seL4_CPtr faulter_vspace, faulter_cspace;

    create_helper_thread(env, &faulter_thread);
    create_helper_thread(env, &handler_thread);
    faulter_cspace = env->cspace_root;
    faulter_vspace = env->page_directory;
    handler_arg0 = fault_ep;
    handler_arg1 = faulter_thread.thread.tcb.cptr;
    set_helper_priority(&handler_thread, 100);

    error = seL4_TCB_Configure(faulter_thread.thread.tcb.cptr,
                               fault_ep, seL4_CapNull,
                               seL4_Prio_new(100, 100),
                               faulter_thread.thread.sched_context.cptr,
                               faulter_cspace,
                               seL4_CapData_Guard_new(0, seL4_WordBits - env->cspace_size_bits),
                               faulter_vspace, seL4_NilData,
                               faulter_thread.thread.ipc_buffer_addr,
                               faulter_thread.thread.ipc_buffer);
    test_assert(!error);

    /* clear the faults */
    total_faults = 0;

    start_helper(env, &handler_thread, (helper_fn_t) handle_fault,
                 handler_arg0, handler_arg1, 0, 0);
    start_helper(env, &faulter_thread, (helper_fn_t) do_ioports,
                 0, 0, 0, 0);

    wait_for_helper(&faulter_thread);

    test_check(total_faults == EXPECTED_FAULTS);

    cleanup_helper(env, &handler_thread);
    cleanup_helper(env, &faulter_thread);

    return (total_faults == EXPECTED_FAULTS) ? SUCCESS : FAILURE;
}
DEFINE_TEST(IOPORTS1000, "Test fault if directly using I/O ports", test_native_ioports)

#endif
