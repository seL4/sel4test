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

# Declare a cache variable that enables/disablings the forcing of cache variables to
# the specific test values. By default it is disabled
set(Sel4testAllowSettingsOverride OFF CACHE BOOL "Allow user to override configuration settings")

# Define our top level settings. Whilst they have doc strings for readability here
# they are hiddein the cmake-gui as they cannot be reliably changed after the initial
# configuration, but enterprising users can still change them if they know what they
# are doing through advanced mode
# Users should initialize a build directory by doing something like
# mkdir build_sabre
# cd build_sabre
# ../init-build.sh -DARM=TRUE -DPLATFORM=sabre -DSIMULATION=TRUE
set(SIMULATION OFF CACHE BOOLL "Include only simulation compatible tests")
set(RELEASE OFF CACHE BOOL "Performance optimized build")
set(VERIFICATION OFF CACHE BOOL "Only verification friendly kernel features")
set(BAMBOO OFF CACHE BOOL "Enable machine parseable output")
set(DOMAINS OFF CACHE BOOL "Test multiple domains")
set(SMP OFF CACHE BOOL "(if supported) Test SMP kernel")
set(PLATFORM "x86_64" CACHE STRING "Platform to test")
set_property(CACHE PLATFORM PROPERTY STRINGS "x86_64;ia32;sabre;jetson;jetson_hyp;arndale;bbone;bbone_black;beagle;hikey;hikey64;inforce;kzm;odroidx;odroidxu;odroidxu3;rpi3;wandq;zynq")
mark_as_advanced(SIMULATION RELEASE VERIFICAITON BAMBOO DOMAINS SMP PLATFORM)

# We use 'FORCE' when settings these values instead of 'INTERNAL' so that they still appear
# in the cmake-gui to prevent excessively confusing users
if (NOT Sel4testAllowSettingsOverride)
    # sel4test creates processes from libsel4utils that require relatively large cspaces
    set(LibSel4UtilsCSpaceSizeBits 17 CACHE STRING "" FORCE)

    # Determine the platform/architecture
    if ((${PLATFORM} STREQUAL "x86_64") OR (${PLATFORM} STREQUAL "ia32"))
        set(KernelArch x86 CACHE STRING "" FORCE)
        set(KernelX86Sel4Arch ${PLATFORM} CACHE STRING "" FORCE)
        # Generally we cannot simulate some more recent features
        if (SIMULATION)
            set(KernelSupportPCID OFF CACHE BOOL "" FORCE)
            set(KernelFSGSBase msr CACHE STRING "" FORCE)
            set(KernelIOMMU OFF CACHE BOOL "" FORCE)
            set(KernelFPU FXSAVE CACHE STRING "" FORCE)
        else()
            set(KernelIOMMU ON CACHE BOOL "" FORCE)
            # If we switch from simulation to non simulation builds we don't really care about
            # the other options we changed as they do not change what tests are run
        endif()
    elseif(
            (${PLATFORM} STREQUAL "sabre")
            OR (${PLATFORM} STREQUAL "jetson")
            OR (${PLATFORM} STREQUAL "hikey")
            OR (${PLATFORM} STREQUAL "kzm")
            OR (${PLATFORM} STREQUAL "rpi3")
            OR (${PLATFORM} STREQUAL "wandq")
            OR (${PLATFORM} STREQUAL "arndale")
            OR (${PLATFORM} STREQUAL "bbone")
            OR (${PLATFORM} STREQUAL "bbone_black")
            OR (${PLATFORM} STREQUAL "beagle")
            OR (${PLATFORM} STREQUAL "inforce")
            OR (${PLATFORM} STREQUAL "odroid")
            OR (${PLATFORM} STREQUAL "odroidxu")
            OR (${PLATFORM} STREQUAL "odroidxu3")
            OR (${PLATFORM} STREQUAL "zynq")
            )
        set(KernelArch arm CACHE STRING "" FORCE)
        set(KernelArmSel4Arch "aarch32" CACHE STRING "" FORCE)
        # Some ugly name (un)mangling
        if (${PLATFORM} STREQUAL "arndale")
            set(KernelARMPlatform exynos5250 CACHE STRING "" FROCE)
        elseif((${PLATFORM} STREQUAL "bbone_black") OR (${PLATFORM} STREQUAL "bbone"))
            set(KernelARMPlatform am335x CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "beagle")
            set(KernelARMPlatform omap3 CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "inforce")
            set(KernelARMPlatform apq8064 CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "odroid")
            set(KernelARMPlatform exynos4 CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "odroidxu")
            set(KernelARMPlatform exynos5410 CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "odroidxu3")
            set(KernelARMPlatform exynos5422 CACHE STRING "" FORCE)
        elseif(${PLATFORM} STREQUAL "zynq")
            set(KernelARMPlatform zynq7000 CACHE STRING "" FORCE)
        else()
            set(KernelARMPlatform ${PLATFORM} CACHE STRING "" FORCE)
        endif()
        if ((${PLATFORM} STREQUAL "hikey") OR (${PLATFORM} STREQUAL "bbone_black"))
            set(ElfloaderImage "binary" CACHE STRING "" FORCE)
        endif()
    elseif(${PLATFORM} STREQUAL "tx1")
        set(KernelArch arm CACHE STRING "" FORCE)
        set(KernelArmSel4Arch "aarch64" CACHE STRING "" FORCE)
        set(KernelARMPlatform ${PLATFORM} CACHE STRING "" FORCE)
        set(ElfloaderImage "binary" CACHE STRING "" FORCE)
    elseif(${PLATFORM} STREQUAL "hikey64")
        set(KernelArch arm CACHE STRING "" FORCE)
        set(KernelArmSel4Arch "aarch64" CACHE STRING "" FORCE)
        set(KernelARMPlatform hikey CACHE STRING "" FORCE)
        set(ElfloaderImage "efi" CACHE STRING "" FORCE)
    elseif(${PLATFORM} STREQUAL "jetson_hyp")
        set(KernelArch arm CACHE STRING "" FORCE)
        set(KernelArmSel4Arch "arm_hyp" CACHE STRING "" FORCE)
        set(KernelARMPlatform jetson CACHE STRING "" FORCE)
        set(ElfloaderMode "hypervisor" CACHE STRING "" FORCE)
        set(ElfloaderMonitorHook ON CACHE BOOL "" FORCE)
        set(KernelArmHypervisorSupport ON CACHE BOOL "" FORCE)
    else()
        message(FATAL_ERROR "Unknown PLATFORM. Initial configuration may not work")
    endif()

    if (SIMULATION)
        set(Sel4testHaveCache OFF CACHE BOOL "" FORCE)
    else()
        set(Sel4testHaveCache ON CACHE BOOL "" FORCE)
    endif()
    if (SIMULATION AND KernelArchARM)
        set(Sel4testHaveTimer OFF CACHE BOOL "" FORCE)
    else()
        set(Sel4testHaveTimer ON CACHE BOOL "" FORCE)
    endif()

    # Check the hardware debug API non simulated (except for ia32, which can be simulated),
    # skipping any aarch64 platform, as this does not yet support the debug API, and a
    # few other miscelaneous platforms that do not support it
    if (((NOT SIMULATION) OR KernelSel4ArchIA32) AND (NOT KernelSel4ArchAarch64)
            AND (NOT ("${KernelARMPlatform}" STREQUAL "exynos5250"))
            AND (NOT ("${KernelARMPlatform}" STREQUAL "am335x"))
            AND (NOT ("${KernelARMPlatform}" STREQUAL "omap3"))
            AND (NOT ("${KernelARMPlatform}" STREQUAL "kzm"))
            AND (NOT ("${KernelARMPlatform}" STREQUAL "exynos5410"))
            )
        set(HardwareDebugAPI ON CACHE BOOL "" FORCE)
    else()
        set(HardwareDebugAPI OFF CACHE BOOL "" FORCE)
    endif()

    # Setup flags for different combinations of RELEASE (performance optimized builds) and
    # VERIFICATION (verification friendly features) builds
    if (RELEASE)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    else()
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
    endif()
    if (VERIFICATION)
        set(KernelVerificationBuild ON CACHE BOOL "" FORCE)
    else()
        set(KernelVerificationBuild OFF CACHE BOOL "" FORCE)
        set(KernelPrinting ON CACHE BOOL "" FORCE)
    endif()
    # If neither release nor verification then enable debug facilities, otherwise turn them off
    if ((NOT RELEASE) AND (NOT VERIFICATION))
        set(KernelDebugBuild ON CACHE BOOL "" FORCE)
    else()
        set(KernelDebugBuild OFF CACHE BOOL "" FORCE)
    endif()

    if (BAMBOO)
        set(LibSel4TestPrintXML ON CACHE BOOL "" FORCE)
        set(LibSel4BufferOutput ON CACHE BOOL "" FORCE)
    else()
        set(LibSel4TestPrintXML OFF CACHE BOOL "" FORCE)
        set(LibSel4BufferOutput OFF CACHE BOOL "" FORCE)
    endif()

    if (DOMAINS)
        set(KernelNumDomains 16 CACHE STRING "" FORCE)
    else()
        set(KernelNumDomains 1 CACHE STRING "" FORCE)
    endif()

    if (SMP)
        set(KernelMaxNumNodes 4 CACHE STRING "" FORCE)
    else()
        set(KernelMaxNumNodes 1 CACHE STRING "" FORCE)
    endif()
endif()
