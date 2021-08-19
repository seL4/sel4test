/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../../init.h"

#include <sel4platsupport/io.h>
#include <sel4platsupport/timer.h>
#include <platsupport/plat/timer.h>
#include <platsupport/plat/serial.h>
#include <sel4platsupport/arch/io.h>
#include <sel4utils/sel4_zf_logif.h>
#include <sel4rpc/client.h>
#include <rpc.pb.h>

static sel4rpc_client_t *rpc_client;
static seL4_Error get_IOPort_cap(void *data, uint16_t start_port, uint16_t end_port, seL4_Word root, seL4_Word dest,
                                 seL4_Word depth)
{
    test_init_data_t *init = (test_init_data_t *) data;

    if (start_port < SERIAL_CONSOLE_COM1_PORT ||
        start_port > SERIAL_CONSOLE_COM1_PORT_END) {
        return seL4_RangeError;
    }

    RpcMessage rpcMsg = {
        .which_msg = RpcMessage_ioport_tag,
        .msg.ioport = {
            .start = SERIAL_CONSOLE_COM1_PORT,
            .end = SERIAL_CONSOLE_COM1_PORT_END,
        },
    };

    int ret = sel4rpc_call(rpc_client, &rpcMsg, root, dest, depth);
    if (ret < 0) {
        return seL4_InvalidArgument;
    }

    return rpcMsg.msg.ret.errorCode;
}

static seL4_Error get_msi(void *data, seL4_CNode root, seL4_Word index, uint8_t depth,
                          UNUSED seL4_Word pci_bus, UNUSED seL4_Word pci_dev, UNUSED seL4_Word pci_func,
                          UNUSED seL4_Word handle, seL4_Word vector)
{
    return 0;
}

static seL4_Error get_ioapic(void *data, seL4_CNode root, seL4_Word index, uint8_t depth, seL4_Word ioapic,
                             seL4_Word pin, seL4_Word level, seL4_Word polarity, seL4_Word vector)
{
    return 0;
}

void arch_init_simple(env_t env, simple_t *simple)
{
    rpc_client = &env->rpc_client;
    simple->arch_simple.IOPort_cap = get_IOPort_cap;
    simple->arch_simple.msi = get_msi;
    simple->arch_simple.ioapic = get_ioapic;
}

void arch_init_allocator(env_t env, test_init_data_t *data)
{
}
