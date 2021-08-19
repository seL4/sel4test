/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/* FIXME: This file is symlinked to RISC-V from ARM because they use the same
 * implementation. This was done because there is a plan to remove this functionality
 * in favour of a vka RPC client that allows the test process to query for hardware
 * resources instead of preallocating them.
 */
#include "../../init.h"
#include <platsupport/plat/serial.h>
#include <sel4platsupport/io.h>
#include <sel4platsupport/timer.h>
#include <vka/capops.h>
#include <sel4rpc/client.h>
#include <rpc.pb.h>

static cspacepath_t serial_frame_path = {0};
static vka_t old_vka;
static sel4rpc_client_t *rpc_client;

static int serial_utspace_alloc_at_fn(void *data, const cspacepath_t *dest, seL4_Word type, seL4_Word size_bits,
                                      uintptr_t paddr, seL4_Word *cookie)
{
    /* first try the local VKA */
    int ret = old_vka.utspace_alloc_at(data, dest, type, size_bits, paddr, cookie);
    if (!ret) {
        return ret;
    }

    /* if that didn't work, try the driver's VKA */
    RpcMessage rpcMsg = {
        .which_msg = RpcMessage_memory_tag,
        .msg.memory = {
            .address = paddr,
            .size_bits = size_bits,
            .type = type,
        },
    };

    sel4rpc_call(rpc_client, &rpcMsg, dest->root, dest->capPtr, dest->capDepth);
    *cookie = rpcMsg.msg.ret.cookie;
    return rpcMsg.msg.ret.errorCode;
}

void arch_init_allocator(env_t env, test_init_data_t *data)
{
    /* Get the endpoint we use for communicating with the test driver,
     * and then set up the proxy vka_utspace_alloc_at()
     * wrapper function.
     */
    rpc_client = &env->rpc_client;
    old_vka = env->vka;
    env->vka.utspace_alloc_at = serial_utspace_alloc_at_fn;
}

void arch_init_simple(env_t env, simple_t *simple)
{
    /* nothing to do */
}
