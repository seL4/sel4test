/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "../../test.h"

#include <sel4utils/arch/tsc.h>

void plat_init(driver_env_t env)
{
    env->init->tsc_freq = x86_get_tsc_freq_from_simple(&env->simple);
    ZF_LOGF_IF(env->init->tsc_freq == 0, "Failed get TSC frequency");
}
