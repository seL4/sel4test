# MEMCPY_BENCH micro-benchmark

This adds a simple memcpy micro-benchmark to `sel4test-tests`.

## What’s included
- `apps/sel4test-tests/src/tests/bench_memcpy.c`
- CMakeLists updated to compile and register the test.

## Build & run
```bash
cd ~/sel4-ws/build-x86
../init-build.sh -DPLATFORM=x86_64 -DSIMULATION=TRUE -GNinja
ninja
ninja simulate           # generates the ./simulate script
./simulate | tee run.log # boots QEMU and runs tests

