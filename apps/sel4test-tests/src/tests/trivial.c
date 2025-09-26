/*
 * Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sel4test/test.h>
#include "../test.h"
#include "../helpers.h"

#define MIN_EXPECTED_ALLOCATIONS 100

int test_trivial(env_t env)
{
    test_geq(2, 1);
    return sel4test_get_result();
}
DEFINE_TEST(TRIVIAL0000, "Ensure the test framework functions", test_trivial, true)

int test_allocator(env_t env)
{
    /* Perform a bunch of allocations and frees */
    vka_object_t endpoint;
    int error;

    for (int i = 0; i < MIN_EXPECTED_ALLOCATIONS; i++) {
        error = vka_alloc_endpoint(&env->vka, &endpoint);
        test_error_eq(error, 0);
        test_assert(endpoint.cptr != 0);
        vka_free_object(&env->vka, &endpoint);
    }

    return sel4test_get_result();
}
DEFINE_TEST(TRIVIAL0001, "Ensure the allocator works", test_allocator, true)
DEFINE_TEST(TRIVIAL0002, "Ensure the allocator works more than once", test_allocator, true)




/* ==== BEGIN: MEMCPY_BENCH_GUARD ==== */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <sel4test/test.h>   /* for env_t and DEFINE_TEST */

typedef struct {
    uint8_t *base;    /* pointer returned by malloc (for free) */
    uint8_t *aligned; /* 64B-aligned pointer within the allocation */
    size_t   size;    /* usable payload size from aligned */
} bench_buf;

static bench_buf bench_alloc64(size_t payload) {
    size_t need = payload + 128;              /* headroom for align/misalign */
    uint8_t *base = (uint8_t*)malloc(need);
    assert(base);
    uintptr_t p = (uintptr_t)base;
    uintptr_t aligned = (p + 63u) & ~(uintptr_t)63u;
    size_t head = (size_t)(aligned - p);
    size_t usable = (head <= need) ? (need - head) : 0;
    bench_buf b = { .base = base, .aligned = (uint8_t*)aligned, .size = usable };
    return b;
}

static void bench_free(bench_buf *b) {
    if (b && b->base) free(b->base);
    if (b) *b = (bench_buf){0};
}

#if defined(__x86_64__)
static inline uint64_t rdtsc_serialized(void) {
    unsigned int lo, hi;
    asm volatile("cpuid" : : "a"(0) : "rbx","rcx","rdx");
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t t0 = ((uint64_t)hi << 32) | lo;
    asm volatile("cpuid" : : "a"(0) : "rbx","rcx","rdx");
    return t0;
}
#else
# error "This microbench currently targets x86_64 only."
#endif

static void memcpy_touch(void *p, size_t n) {
    volatile uint8_t *q = (volatile uint8_t*)p;
    for (size_t i = 0; i < n; i += 64) q[i] ^= 0;
    if (n) q[n-1] ^= 0;
}

static double memcpy_bench_once(uint8_t *dst, uint8_t *src, size_t n, int iters) {
    memcpy_touch(src, n); memcpy_touch(dst, n);
    (void)memcpy(dst, src, n); /* warm */

    uint64_t t0 = rdtsc_serialized();
    for (int i = 0; i < iters; i++) {
        memcpy(dst, src, n);
    }
    uint64_t t1 = rdtsc_serialized();

    uint64_t cycles = (t1 - t0);
    return (double)cycles / (double)(n * (size_t)iters);
}

/* NOTE: correct sel4test signature */
static int bench_memcpy(env_t *env) {
    (void)env;
    const size_t sizes[] = {64, 256, 1024, 4096, 65536, 1048576};
    const int iters[]    = {512, 256, 128, 64, 16, 4};

    for (size_t si = 0; si < sizeof(sizes)/sizeof(sizes[0]); si++) {
        size_t n = sizes[si];

        bench_buf a = bench_alloc64(n + 1); /* +1 for misalign+1 case */
        bench_buf b = bench_alloc64(n + 1);
        assert(a.aligned && b.aligned && a.size >= n+1 && b.size >= n+1);

        for (size_t i = 0; i < n+1; i++) a.aligned[i] = (uint8_t)(i * 131u);
        memset(b.aligned, 0, n+1);

        struct {
            const char *label;
            uint8_t *src;
            uint8_t *dst;
        } cases[2] = {
            { "aligned",    a.aligned,   b.aligned   },
            { "misalign+1", a.aligned+1, b.aligned+1 },
        };

        for (int ci = 0; ci < 2; ci++) {
            double cpb = memcpy_bench_once(cases[ci].dst, cases[ci].src, n, iters[si]);
            printf("memcpy: size=%7zu  %-10s  cpb=%.3f\n", n, cases[ci].label, cpb);
        }

        bench_free(&b);
        bench_free(&a);
    }
    return 0;
}

/* Always-on registration */
/* ==== END: MEMCPY_BENCH_GUARD ==== */
