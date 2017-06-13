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

project(sel4test-driver C)

set(configure_string "")

config_option(Sel4testHaveTimer HAVE_TIMER
    "Enable tests that require a timer driver"
    DEFAULT ON
)

config_option(Sel4testHaveCache HAVE_CACHE
    "Enable tests that require a functioning cache"
    DEFAULT ON
)

add_config_library(sel4test-driver "${configure_string}")

file(GLOB static
    src/*.c src/arch/${KernelArch}/*.c src/plat/${KernelPlatform}/*.c
    src/arch/${KernelArch}/*.S src/plat/${KernelPlatform}/*.S
)

add_custom_command(OUTPUT test_names.c
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract-test-names.sh
        ${CROSS_COMPILER_PREFIX}objdump $<TARGET_FILE:sel4test-tests> > test_names.c
    DEPENDS $<TARGET_FILE:sel4test-tests> ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract-test-names.sh
    VERBATIM
    COMMENT "Extracting unit test list from sel4test-tests"
)

add_custom_target(test_names_gen DEPENDS test_names.c)

MakeCPIO(archive.o "$<TARGET_FILE:sel4test-tests>")

add_executable(sel4test-driver EXCLUDE_FROM_ALL ${static} test_names.c archive.o)
target_include_directories(sel4test-driver PRIVATE "include")
target_link_libraries(sel4test-driver Configuration muslc sel4 sel4allocman sel4vka sel4utils sel4test sel4platsupport sel4muslcsys)
target_compile_options(sel4test-driver PRIVATE -Werror -g)

# Set this image as the rootserver
DeclareRootserver(sel4test-driver)
