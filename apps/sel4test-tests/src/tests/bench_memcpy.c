#include <sel4test/test.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

static inline uint64_t nsecs_since(const struct timespec* t0, const struct timespec* t1) {
    return (uint64_t)(t1->tv_sec - t0->tv_sec) * 1000000000ull +
           (uint64_t)(t1->tv_nsec - t0->tv_nsec);
}

static void do_once(size_t n) {
    static uint8_t __attribute__((aligned(64))) src[1<<20];
    static uint8_t __attribute__((aligned(64))) dst[1<<20];
    for (size_t i = 0; i < n; i++) src[i] = (uint8_t)(i * 131u);
    memcpy(dst, src, n);
    volatile uint8_t sink = dst[n - 1];
    (void)sink;
}

static int memcpy_bench_once(env_t env, size_t n, unsigned iters) {
    // warm-up
    for (unsigned i = 0; i < (iters < 10 ? iters : 10); i++) do_once(n);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (unsigned i = 0; i < iters; i++) do_once(n);
    clock_gettime(CLOCK_MONOTONIC, &t1);

    uint64_t ns = nsecs_since(&t0, &t1);
    double bytes = (double)n * (double)iters;
    double mbps = (bytes / (1024.0 * 1024.0)) / ((double)ns / 1e9);

    printf("memcpy: size=%zu, iters=%u, total_ns=%llu, MB/s=%.1f\n",
           n, iters, (unsigned long long)ns, mbps);
    return sel4test_get_result();
}

static int test_memcpy_bench(env_t env)
{
    size_t sizes[] = {64, 128, 256, 512, 1024, 4096, 16384, 65536};
    for (unsigned i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        (void)memcpy_bench_once(env, sizes[i], 500);
    }
    return sel4test_get_result();
}

DEFINE_TEST(MEMCPY_BENCH, "Memcpy micro-benchmark", test_memcpy_bench, true)
