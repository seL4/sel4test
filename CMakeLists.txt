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

include(${KERNEL_HELPERS_PATH})

add_subdirectory(apps/sel4test-tests)
add_subdirectory(apps/sel4test-driver)
add_subdirectory(libsel4testsupport)

# Set our custom domain schedule
RequireFile(SEL4TEST_DOMAIN_SCHEDULE domain_schedule.c CMAKE_FIND_ROOT_PATH_BOTH)
set(KernelDomainSchedule "${SEL4TEST_DOMAIN_SCHEDULE}" CACHE INTERNAL "")

GenerateSimulateScript()
