/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/* Small program entry point. This just defines the
 * _start symbol, does some IPC buffer setup and then
 * jumps to main. Should main return we also call
 * _exit with the return value*/

#include <sel4/arch/functions.h>
#include <stdlib.h>

int main(int argc, char *argv[]);

void __attribute__((externally_visible)) _start(int argc, char *argv[], seL4_IPCBuffer *ipc_buffer) {
    int ret;
    seL4_SetUserData((seL4_Word)ipc_buffer);
    ret = main(argc, argv);
    exit(ret);
    __builtin_unreachable();
}
