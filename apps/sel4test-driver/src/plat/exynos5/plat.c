/*
 * Copyright 2018, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include "../../test.h"

#include <platsupport/plat/clock.h>
#include <sel4platsupport/io.h>
#include <utils/zf_log.h>
#include <platsupport/plat/serial.h>

#if defined(CONFIG_PLAT_EXYNOS5422)
void plat_init(driver_env_t env)
{
    int error;
    clock_sys_t clock = {};
    clk_t *clk;

    error = clock_sys_init(&env->ops, &clock);
    if (error != 0) {
        ZF_LOGF("Failed to initalise clock");
    }

    clk = clk_get_clock(&clock, CLK_UART2);
    if (clk == NULL) {
        ZF_LOGF("Failed to get clock");
    }

    clk_set_freq(clk, UART_DEFAULT_FIN);
}
#endif
