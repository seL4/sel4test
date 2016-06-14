/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */
#include "../../test.h"

#include <sel4platsupport/timer.h>
#include <sel4platsupport/plat/timer.h>
#include <platsupport/timer.h>

#include <platsupport/plat/serial.h>
#include <platsupport/plat/pit.h>

static seL4_CPtr
get_IOPort_cap(void *data, uint16_t start_port, uint16_t end_port)
{
    test_init_data_t *init = (test_init_data_t *) data;

    assert((start_port >= PIT_IO_PORT_MIN && end_port <= PIT_IO_PORT_MAX)
            || (start_port >= SERIAL_CONSOLE_COM1_PORT && start_port <= SERIAL_CONSOLE_COM1_PORT_END));

    if (start_port >= PIT_IO_PORT_MIN && end_port <= PIT_IO_PORT_MAX) {
        return init->io_port;
    }

    if (start_port >= SERIAL_CONSOLE_COM1_PORT && start_port <= SERIAL_CONSOLE_COM1_PORT_END) {
        return init->serial_io_port1;
    }
    /* Return invalid cap */
    return 0;
}

void
arch_init_simple(simple_t *simple) {
    simple->arch_simple.IOPort_cap = get_IOPort_cap;
}

