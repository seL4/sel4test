#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

cmake_minimum_required(VERSION 3.7.2)

project(sel4test-tests C CXX)

# Use arm arch source files on RISCV
set(arch ${KernelArch})
if(KernelArchRiscV)
    set(arch arm)
endif()

file(
    GLOB
        deps
        src/*.c
        src/tests/*.c
        src/arch/${arch}/*.c
        src/arch/${KernelArch}/tests/*.c
        src/*.cxx
        src/tests/*.cxx
        src/arch/${KernelArch}/tests/*.S
)

add_executable(sel4test-tests EXCLUDE_FROM_ALL ${deps})
target_include_directories(
    sel4test-tests
    PRIVATE include arch/${KernelArch} sel4_arch/${KernelSel4Arch}
)

target_link_libraries(
    sel4test-tests
    PUBLIC
        sel4_autoconf
        muslc
        sel4
        sel4runtime
        sel4allocman
        sel4vka
        sel4utils
        sel4rpc
        sel4test
        sel4sync
        sel4muslcsys
        sel4testsupport
        sel4serialserver_tests
    PRIVATE sel4test-driver_Config
)
