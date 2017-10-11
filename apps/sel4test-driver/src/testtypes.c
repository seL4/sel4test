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

/* Include Kconfig variables. */
#include <autoconf.h>

#include <sel4debug/register_dump.h>
#include <vka/capops.h>

#include "test.h"
#include <sel4testsupport/testreporter.h>

/* Bootstrap test type. */
static inline void bootstrap_set_up_test_type(uintptr_t e)
{
    ZF_LOGD("setting up bootstrap test type\n");
}
static inline void bootstrap_tear_down_test_type(uintptr_t e)
{
    ZF_LOGD("tearing down bootstrap test type\n");
}
static inline void bootstrap_set_up(uintptr_t e)
{
    ZF_LOGD("set up bootstrap test\n");
}
static inline void bootstrap_tear_down(uintptr_t e)
{
    ZF_LOGD("tear down bootstrap test\n");
}
static inline test_result_t bootstrap_run_test(struct testcase* test, uintptr_t e)
{
    return test->function(e);
}

static DEFINE_TEST_TYPE(BOOTSTRAP, BOOTSTRAP,
                        bootstrap_set_up_test_type, bootstrap_tear_down_test_type,
                        bootstrap_set_up, bootstrap_tear_down, bootstrap_run_test);

/* Basic test type. Each test is launched as its own process. */
/* copy untyped caps into a processes cspace, return the cap range they can be found in */
static seL4_SlotRegion
copy_untypeds_to_process(sel4utils_process_t *process, vka_object_t *untypeds, int num_untypeds, driver_env_t env)
{
    seL4_SlotRegion range = {0};

    for (int i = 0; i < num_untypeds; i++) {
        seL4_CPtr slot = sel4utils_copy_cap_to_process(process, &env->vka, untypeds[i].cptr);

        /* set up the cap range */
        if (i == 0) {
            range.start = slot;
        }
        range.end = slot;
    }
    assert((range.end - range.start) + 1 == num_untypeds);
    return range;
}

static void
copy_serial_caps(test_init_data_t *init, driver_env_t env, sel4utils_process_t *test_process)
{
    init->serial_irq_cap = sel4utils_copy_cap_to_process(test_process, &env->vka,
                                                         env->serial_objects.serial_irq_path.capPtr);
    ZF_LOGF_IF(init->serial_irq_cap == 0,
               "Failed to copy PS default serial IRQ cap to test child "
               "process.");

    arch_copy_serial_caps(init, env, test_process);
}

void basic_set_up(uintptr_t e)
{
    int error;
    driver_env_t env = (driver_env_t)e;

    sel4utils_process_config_t config = process_config_default_simple(&env->simple, TESTS_APP, env->init->priority);
    config = process_config_mcp(config, seL4_MaxPrio);
    error = sel4utils_configure_process_custom(&(env->test_process), &env->vka, &env->vspace, config);
    assert(error == 0);

    /* set up caps about the process */
    env->init->stack_pages = CONFIG_SEL4UTILS_STACK_SIZE / PAGE_SIZE_4K;
    env->init->stack = env->test_process.thread.stack_top - CONFIG_SEL4UTILS_STACK_SIZE;
    env->init->page_directory = sel4utils_copy_cap_to_process(&(env->test_process), &env->vka, env->test_process.pd.cptr);
    env->init->root_cnode = SEL4UTILS_CNODE_SLOT;
    env->init->tcb = sel4utils_copy_cap_to_process(&(env->test_process), &env->vka, env->test_process.thread.tcb.cptr);
    env->init->domain = sel4utils_copy_cap_to_process(&(env->test_process), &env->vka, simple_get_init_cap(&env->simple, seL4_CapDomain));
    env->init->asid_pool = sel4utils_copy_cap_to_process(&(env->test_process), &env->vka, simple_get_init_cap(&env->simple, seL4_CapInitThreadASIDPool));
    env->init->asid_ctrl = sel4utils_copy_cap_to_process(&(env->test_process), &env->vka, simple_get_init_cap(&env->simple, seL4_CapASIDControl));
#ifdef CONFIG_IOMMU
    env->init->io_space = sel4utils_copy_cap_to_process(&(env->test_process), &env->vka, simple_get_init_cap(&env->simple, seL4_CapIOSpace));
#endif /* CONFIG_IOMMU */
#ifdef CONFIG_ARM_SMMU
    env->init->io_space_caps = arch_copy_iospace_caps_to_process(&(env->test_process), &env);
#endif
#ifdef CONFIG_ARCH_X86
    /* pass the entire io port range for the timer io port for simplicity */
    env->init->timer_io_port_cap = sel4utils_copy_cap_to_process(&(env->test_process), &env->vka, simple_get_init_cap(&env->simple, seL4_CapIOPort));
#endif
    env->init->cores = simple_get_core_count(&env->simple);
    /* copy the sched ctrl caps to the remote process */
    if (config_set(CONFIG_KERNEL_RT)) {
        seL4_CPtr sched_ctrl = simple_get_sched_ctrl(&env->simple, 0);
        env->init->sched_ctrl = sel4utils_copy_cap_to_process(&(env->test_process), &env->vka, sched_ctrl);
        for (int i = 1; i < env->init->cores; i++) {
            sched_ctrl = simple_get_sched_ctrl(&env->simple, i);
            sel4utils_copy_cap_to_process(&(env->test_process), &env->vka, sched_ctrl);
        }
    }
    /* setup data about untypeds */
    env->init->untypeds = copy_untypeds_to_process(&(env->test_process), env->untypeds, env->num_untypeds, env);
    error = sel4utils_copy_timer_caps_to_process(&env->init->to, &env->timer_objects, &env->vka, &(env->test_process));
    ZF_LOGF_IF(error, "Failed to copy timer_objects to test process");
    copy_serial_caps(env->init, env, &(env->test_process));
    /* copy the fault endpoint - we wait on the endpoint for a message
     * or a fault to see when the test finishes */
    env->endpoint = sel4utils_copy_cap_to_process(&(env->test_process), &env->vka, env->test_process.fault_endpoint.cptr);

    /* map the cap into remote vspace */
    env->remote_vaddr = vspace_share_mem(&env->vspace, &(env->test_process).vspace, env->init, 1, PAGE_BITS_4K, seL4_AllRights, 1);
    assert(env->remote_vaddr != 0);

    /* WARNING: DO NOT COPY MORE CAPS TO THE PROCESS BEYOND THIS POINT,
     * AS THE SLOTS WILL BE CONSIDERED FREE AND OVERRIDDEN BY THE TEST PROCESS. */
    /* set up free slot range */
    env->init->cspace_size_bits = CONFIG_SEL4UTILS_CSPACE_SIZE_BITS;
    env->init->free_slots.start = env->endpoint + 1;
    env->init->free_slots.end = (1u << CONFIG_SEL4UTILS_CSPACE_SIZE_BITS);
    assert(env->init->free_slots.start < env->init->free_slots.end);
}

test_result_t
basic_run_test(struct testcase *test, uintptr_t e)
{
    int error;
    driver_env_t env = (driver_env_t)e;

    /* copy test name */
    strncpy(env->init->name, test->name, TEST_NAME_MAX);
    /* ensure string is null terminated */
    env->init->name[TEST_NAME_MAX - 1] = '\0';
#ifdef CONFIG_DEBUG_BUILD
    seL4_DebugNameThread(env->test_process.thread.tcb.cptr, env->init->name);
#endif

    /* set up args for the test process */
    seL4_Word argc = 2;
    char string_args[argc][WORD_STRING_SIZE];
    char *argv[argc];
    sel4utils_create_word_args(string_args, argv, argc, env->endpoint, env->remote_vaddr);

    /* spawn the process */
    error = sel4utils_spawn_process_v(&(env->test_process), &env->vka, &env->vspace,
                                      argc, argv, 1);
    ZF_LOGF_IF(error != 0, "Failed to start test process!");

    /* wait on it to finish or fault, report result */
    seL4_MessageInfo_t info = api_wait(env->test_process.fault_endpoint.cptr, NULL);

    test_result_t result = seL4_GetMR(0);
    if (seL4_MessageInfo_get_label(info) != seL4_Fault_NullFault) {
        sel4utils_print_fault_message(info, test->name);
        printf("Register of root thread in test (may not be the thread that faulted)\n");
        sel4debug_dump_registers(env->test_process.thread.tcb.cptr);
        result = FAILURE;
    }

    return result;
}

void basic_tear_down(uintptr_t e)
{
    driver_env_t env = (driver_env_t)e;
    /* unmap the env->init data frame */
    vspace_unmap_pages(&(env->test_process).vspace, env->remote_vaddr, 1, PAGE_BITS_4K, NULL);

    /* reset all the untypeds for the next test */
    for (int i = 0; i < env->num_untypeds; i++) {
        cspacepath_t path;
        vka_cspace_make_path(&env->vka, env->untypeds[i].cptr, &path);
        vka_cnode_revoke(&path);
    }

    /* destroy the process */
    sel4utils_destroy_process(&(env->test_process), &env->vka);
}

DEFINE_TEST_TYPE(BASIC, BASIC, NULL, NULL, basic_set_up, basic_tear_down, basic_run_test);

