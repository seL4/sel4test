/*
 * Copyright 2019, Data61
 * Commonwealth Scientific and Industrial Research Organisation (CSIRO)
 * ABN 41 687 119 230.
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(DATA61_BSD)
 */
#include <pb_decode.h>
#include <pb_encode.h>
#include <rpc.pb.h>
#include <sel4rpc/server.h>
#include <vka/object.h>

#include "test.h"
#include "rpc.h"

int sel4test_rpc_recv(sel4rpc_server_env_t *env, void *data, RpcMessage *rpcMsg)
{
    driver_env_t drv_env = (driver_env_t)data;
    seL4_CPtr cap;

    if (rpcMsg->which_msg == RpcMessage_memory_tag) {
#ifndef CONFIG_ARCH_X86
        if (rpcMsg->msg.memory.address == drv_env->serial_objects.arch_serial_objects.serial_frame_paddr) {
            cap = drv_env->serial_objects.arch_serial_objects.serial_frame_obj.cptr;
            seL4_SetCap(0, cap);
            sel4rpc_server_reply(env, 1, 0);
            return 0;
        }
#endif
    } else if (rpcMsg->which_msg == RpcMessage_ioport_tag) {
#ifdef CONFIG_ARCH_X86
        if (rpcMsg->msg.ioport.start == SERIAL_CONSOLE_COM1_PORT) {
            cap = drv_env->serial_objects.arch_serial_objects.serial_io_port_cap;
            seL4_SetCap(0, cap);
            sel4rpc_server_reply(env, 1, 0);
            return 0;
        }
#endif
    }

    return sel4rpc_default_handler(env, NULL, rpcMsg);
}
