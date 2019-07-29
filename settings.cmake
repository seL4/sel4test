#
# Copyright 2019, Data61
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

set(project_dir "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(resolved_path ${CMAKE_CURRENT_LIST_FILE} REALPATH)
# repo_dir is distinct from project_dir as this file is symlinked.
# project_dir corresponds to the top level project directory, and
# repo_dir is the absolute path after following the symlink.
get_filename_component(repo_dir ${resolved_path} DIRECTORY)

include(${project_dir}/tools/seL4/cmake-tool/helpers/application_settings.cmake)
# Set our custom domain schedule
set(KernelDomainSchedule "${repo_dir}/domain_schedule.c" CACHE INTERNAL "")

# Declare a cache variable that enables/disablings the forcing of cache variables to
# the specific test values. By default it is disabled
set(Sel4testAllowSettingsOverride OFF CACHE BOOL "Allow user to override configuration settings")

include(${repo_dir}/easy-settings.cmake)

correct_platform_strings()

include(${project_dir}/kernel/configs/seL4Config.cmake)

set(
    valid_platforms
    ${KernelPlatform_all_strings}
    sabre
    wandq
    kzm
    rpi3
    exynos5250
    exynos5410
    exynos5422
    am335x-boneblack
    am335x-boneblue
    x86_64
    ia32
)
set_property(CACHE PLATFORM PROPERTY STRINGS ${valid_platforms})
list(FIND valid_platforms "${PLATFORM}" index)
if("${index}" STREQUAL "-1")
    message(FATAL_ERROR "Invalid PLATFORM selected: \"${PLATFORM}\"
Valid platforms are: \"${valid_platforms}\"")
endif()

mark_as_advanced(
    CLEAR
    LibSel4TestPrinterRegex
    LibSel4TestPrinterHaltOnTestFailure
    LibSel4TestPrintXML
)

set(LibNanopb ON CACHE BOOL "" FORCE)

# We use 'FORCE' when settings these values instead of 'INTERNAL' so that they still appear
# in the cmake-gui to prevent excessively confusing users
if(NOT Sel4testAllowSettingsOverride)
    if(ARM_HYP)
        set(KernelArmHypervisorSupport ON CACHE BOOL "" FORCE)
    endif()
    if(KernelArchARM OR KernelArchRiscV)
        # Elfloader settings that correspond to how Data61 sets its boards up.
        ApplyData61ElfLoaderSettings(${KernelPlatform} ${KernelSel4Arch})
    endif()

    if(SIMULATION)
        ApplyCommonSimulationSettings(${KernelArch})
    else()
        if(KernelArchX86)
            set(KernelIOMMU ON CACHE BOOL "" FORCE)
        endif()
    endif()

    # sel4test specific config settings.

    # sel4test creates processes from libsel4utils that require relatively large cspaces
    set(LibSel4UtilsCSpaceSizeBits 17 CACHE STRING "" FORCE)

    if(SIMULATION)
        set(Sel4testHaveCache OFF CACHE BOOL "" FORCE)
    else()
        set(Sel4testHaveCache ON CACHE BOOL "" FORCE)
    endif()
    if(KernelPlatformZynqmp OR (SIMULATION AND (KernelArchRiscV OR KernelArchARM)))
        set(Sel4testHaveTimer OFF CACHE BOOL "" FORCE)
    else()
        set(Sel4testHaveTimer ON CACHE BOOL "" FORCE)
    endif()

    # Check the hardware debug API non simulated (except for ia32, which can be simulated),
    # skipping any aarch64 platform, as this does not yet support the debug API, and a
    # few other miscelaneous platforms that do not support it
    if(
        ((NOT SIMULATION) OR KernelSel4ArchIA32)
        AND (NOT KernelSel4ArchAarch64)
        AND (NOT KernelArchRiscV)
        AND (NOT KernelPlatformExynos5250)
        AND (NOT KernelPlatformAM335X)
        AND (NOT KernelPlatformOMAP3)
        AND (NOT KernelPlatformKZM)
        AND (NOT KernelPlatformExynos5410)
    )
        set(HardwareDebugAPI ON CACHE BOOL "" FORCE)
    else()
        set(HardwareDebugAPI OFF CACHE BOOL "" FORCE)
    endif()

    ApplyCommonReleaseVerificationSettings(${RELEASE} ${VERIFICATION})

    # Need to disable GC sections as it causes our tests to be stripped sometimes
    set(UserLinkerGCSections OFF CACHE BOOL "" FORCE)

    if(BAMBOO)
        set(LibSel4TestPrintXML ON CACHE BOOL "" FORCE)
        set(LibSel4BufferOutput ON CACHE BOOL "" FORCE)
        set(KernelIRQReporting OFF CACHE BOOL "" FORCE)
    else()
        set(LibSel4TestPrintXML OFF CACHE BOOL "" FORCE)
        set(LibSel4BufferOutput OFF CACHE BOOL "" FORCE)
        set(KernelIRQReporting ON CACHE BOOL "" FORCE)
    endif()

    if(DOMAINS)
        set(KernelNumDomains 16 CACHE STRING "" FORCE)
    else()
        set(KernelNumDomains 1 CACHE STRING "" FORCE)
    endif()

    if(SMP)
        if(NUM_NODES MATCHES "^[0-9]+$")
            set(KernelMaxNumNodes ${NUM_NODES} CACHE STRING "" FORCE)
        else()
            set(KernelMaxNumNodes 4 CACHE STRING "" FORCE)
        endif()
    else()
        set(KernelMaxNumNodes 1 CACHE STRING "" FORCE)
    endif()
endif()
