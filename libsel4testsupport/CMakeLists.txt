#
# Copyright 2017, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.16.0)

project(libsel4testsupport C)

file(GLOB deps src/*.c)

list(SORT deps)

add_library(sel4testsupport STATIC EXCLUDE_FROM_ALL ${deps})
target_include_directories(sel4testsupport PUBLIC include)
target_link_libraries(sel4testsupport muslc sel4 utils sel4test sel4serialserver)
