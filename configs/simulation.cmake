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

set(Sel4testHaveCache OFF CACHE BOOL "")
set(Sel4testHaveTimer OFF CACHE BOOL "")

# If we're x86-64 then we need to force these
set(KernelSupportPCID OFF CACHE BOOL "")
set(KernelFSGSBase msr CACHE STRING "")

# General x86 needs to set these
set(KernelFPU FXSAVE CACHE STRING "")
set(KernelIOMMU OFF CACHE BOOL "")
