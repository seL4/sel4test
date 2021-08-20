#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.7.2)

set(project_dir "${CMAKE_CURRENT_LIST_DIR}/../../")
file(GLOB project_modules ${project_dir}/projects/*)
list(
    APPEND
        CMAKE_MODULE_PATH
        ${project_dir}/kernel
        ${project_dir}/tools/seL4/cmake-tool/helpers/
        ${project_dir}/tools/seL4/elfloader-tool/
        ${project_modules}
)

set(NANOPB_SRC_ROOT_FOLDER "${project_dir}/tools/nanopb" CACHE INTERNAL "")
set(OPENSBI_PATH "${project_dir}/tools/opensbi" CACHE STRING "OpenSBI Folder location")

set(SEL4_CONFIG_DEFAULT_ADVANCED ON)

include(application_settings)

include(${CMAKE_CURRENT_LIST_DIR}/easy-settings.cmake)

correct_platform_strings()

find_package(seL4 REQUIRED)
sel4_configure_platform_settings()

set(valid_platforms ${KernelPlatform_all_strings} ${correct_platform_strings_platform_aliases})
set_property(CACHE PLATFORM PROPERTY STRINGS ${valid_platforms})
if(NOT "${PLATFORM}" IN_LIST valid_platforms)
    message(FATAL_ERROR "Invalid PLATFORM selected: \"${PLATFORM}\"
Valid platforms are: \"${valid_platforms}\"")
endif()

# Declare a cache variable that enables/disablings the forcing of cache variables to
# the specific test values. By default it is disabled
set(Sel4testAllowSettingsOverride OFF CACHE BOOL "Allow user to override configuration settings")
if(NOT Sel4testAllowSettingsOverride)
    # We use 'FORCE' when settings these values instead of 'INTERNAL' so that they still appear
    # in the cmake-gui to prevent excessively confusing users
    if(ARM_HYP)
        set(KernelArmHypervisorSupport ON CACHE BOOL "" FORCE)
    endif()

    if(KernelPlatformQEMUArmVirt)
        set(SIMULATION ON CACHE BOOL "" FORCE)
    endif()

    if(SIMULATION)
        ApplyCommonSimulationSettings(${KernelSel4Arch})
    else()
        if(KernelArchX86)
            set(KernelIOMMU ON CACHE BOOL "" FORCE)
        endif()
    endif()

    # sel4test specific config settings.

    if(SIMULATION)
        set(Sel4testSimulation ON CACHE BOOL "" FORCE)
        set(Sel4testHaveCache OFF CACHE BOOL "" FORCE)
    else()
        set(Sel4testSimulation OFF CACHE BOOL "" FORCE)
        set(Sel4testHaveCache ON CACHE BOOL "" FORCE)
    endif()

    if(KernelPlatformQEMUArmVirt)
        if(KernelArmExportPCNTUser AND KernelArmExportPTMRUser)
            set(Sel4testHaveTimer ON CACHE BOOL "" FORCE)
        else()
            set(Sel4testHaveTimer OFF CACHE BOOL "" FORCE)
        endif()
    elseif(
        KernelPlatformZynqmp
        OR KernelPlatformPolarfire
        OR (SIMULATION AND (KernelArchRiscV OR KernelArchARM))
    )
        # Frequency settings of the ZynqMP make the ltimer tests problematic
        # Polarfire does not have a complete ltimer implementation
        set(Sel4testHaveTimer OFF CACHE BOOL "" FORCE)
    else()
        set(Sel4testHaveTimer ON CACHE BOOL "" FORCE)
    endif()

    # Check the hardware debug API non simulated (except for ia32, which can be simulated),
    # or platforms that don't support it.
    if(((NOT SIMULATION) OR KernelSel4ArchIA32) AND NOT KernelHardwareDebugAPIUnsupported)
        set(HardwareDebugAPI ON CACHE BOOL "" FORCE)
    else()
        set(HardwareDebugAPI OFF CACHE BOOL "" FORCE)
    endif()

    ApplyCommonReleaseVerificationSettings(${RELEASE} ${VERIFICATION})

    if(BAMBOO)
        set(LibSel4TestPrintXML ON CACHE BOOL "" FORCE)
    else()
        set(LibSel4TestPrintXML OFF CACHE BOOL "" FORCE)
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

    if(MCS)
        set(KernelIsMCS ON CACHE BOOL "" FORCE)
    else()
        set(KernelIsMCS OFF CACHE BOOL "" FORCE)
    endif()

endif()
