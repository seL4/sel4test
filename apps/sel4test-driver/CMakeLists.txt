#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16.0)

project(sel4test-driver C)

set(configure_string "")

find_package(musllibc REQUIRED)
find_package(util_libs REQUIRED)
find_package(seL4_libs REQUIRED)
find_package(sel4_projects_libs REQUIRED)

# Need to disable GC sections as it causes our tests to be stripped sometimes
# This option is tested in the following musllibc_setup_build_environment_with_sel4runtime call.
set(UserLinkerGCSections OFF CACHE BOOL "" FORCE)
# This sets up environment build flags and imports musllibc and runtime libraries.
musllibc_setup_build_environment_with_sel4runtime()
sel4_import_libsel4()
util_libs_import_libraries()
sel4_libs_import_libraries()

# Setting LibNanopb On causes its libraries to be created when sel4_projects_libs_import_libraries
# is called.
set(LibNanopb ON CACHE BOOL "" FORCE)
sel4_projects_libs_import_libraries()
add_subdirectory(../../libsel4testsupport libsel4testsupport)

if(
    # Platform does not support timers
    (NOT LibPlatSupportHaveTimer)
    # Frequency settings of the ZynqMP make the ltimer tests problematic
    OR
        KernelPlatformZynqmp
        # Polarfire does not have a complete ltimer implementation
    OR
        KernelPlatformPolarfire
        # Quartz64 does not have a complete ltimer implementation
    OR KernelPlatformQuartz64
)
    set(Sel4testHaveTimer OFF CACHE BOOL "" FORCE)
else()
    set(Sel4testHaveTimer ON CACHE BOOL "" FORCE)
endif()

config_option(Sel4testHaveTimer HAVE_TIMER "Enable tests that require a timer driver" DEFAULT ON)

config_option(Sel4testSimulation SIMULATION "Disable tests not suitable for simulation" DEFAULT OFF)

config_option(
    Sel4testHaveCache
    HAVE_CACHE
    "Enable tests that require a functioning cache"
    DEFAULT
    ON
)
if(Sel4testAllowSettingsOverride)
    mark_as_advanced(CLEAR Sel4testHaveTimer Sel4testHaveCache)
else()
    mark_as_advanced(FORCE Sel4testHaveTimer Sel4testHaveCache)
endif()

add_config_library(sel4test-driver "${configure_string}")

file(
    GLOB
        static
        src/*.c
        src/tests/*.c
        src/arch/${KernelArch}/*.c
        src/plat/${KernelPlatform}/*.c
        src/arch/${KernelArch}/*.S
        src/plat/${KernelPlatform}/*.S
)

# Import build rules for test app
add_subdirectory(../sel4test-tests sel4test-tests)
include(cpio)
MakeCPIO(archive.o "$<TARGET_FILE:sel4test-tests>")

add_executable(sel4test-driver EXCLUDE_FROM_ALL ${static} archive.o)
target_include_directories(sel4test-driver PRIVATE "include")
target_link_libraries(
    sel4test-driver
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
        sel4platsupport
        sel4muslcsys
        sel4testsupport
    PRIVATE sel4test-driver_Config
)
target_compile_options(sel4test-driver PRIVATE -Werror -g)

# Set this image as the rootserver
include(rootserver)
DeclareRootserver(sel4test-driver)
