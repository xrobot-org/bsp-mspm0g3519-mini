# XRobot BSP for the TI MSPM0 MINI-D.B.48 board.
#
# TI ships no CMake flow; the SDK examples build with CCS projects or with one makefile per
# compiler. This file does what the gcc makefile of the SDK examples does (for example
# examples/nortos/LP_MSPM0G3519/driverlib/empty/gcc/makefile):
#
#   - SysConfig runs on the project's .syscfg and generates ti_msp_dl_config.c/.h, device.opt,
#     the linker script and device.lds.genlibs (cmake/MSPM0SysConfig.cmake)
#   - the generated sources and the startup file named by SysConfig are compiled with the
#     options of the makefile and the options file device.opt
#   - the link uses the generated linker script and device.lds.genlibs
#
# Everything that depends on the device (MSPM0G3507, MSPM0G3519, ...) comes from SysConfig,
# so another device needs another .syscfg only.
#
# Inputs, in the cache or in the environment (the names of the SDK imports.mak):
#   MSPM0_SDK_INSTALL_DIR  MSPM0 SDK root
#   SYSCONFIG_TOOL         sysconfig_cli.sh / sysconfig_cli.bat
cmake_minimum_required(VERSION 3.20)

project(mspm0_minidb48 LANGUAGES C CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type: Debug or Release" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# ---- SysConfig ----------------------------------------------------------------------------
include(cmake/MSPM0SysConfig.cmake)

file(GLOB _syscfg_files CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*.syscfg")
set(MSPM0_SYSCFG_FILE "${_syscfg_files}" CACHE FILEPATH "The SysConfig file of the board")
list(LENGTH MSPM0_SYSCFG_FILE _syscfg_count)
if(NOT _syscfg_count EQUAL 1)
  message(FATAL_ERROR "Expected one .syscfg file in ${CMAKE_SOURCE_DIR}, or set MSPM0_SYSCFG_FILE")
endif()
mspm0_sysconfig("${MSPM0_SYSCFG_FILE}")

# ---- Options of the SDK example makefile ------------------------------------------------
# CFLAGS of the makefile: -I<syscfg dir> @device.opt -O2 -I<sdk>/source/third_party/CMSIS/Core/Include
# -I<sdk>/source (here -isystem) -mcpu=cortex-m0plus -march=armv6-m -mthumb -std=c99 -mfloat-abi=soft
# -ffunction-sections -fdata-sections -g -Wall, and -gstrict-dwarf unless CONFIG=RELEASE.
# C++ gets the same options and -std=gnu++20 from CMAKE_CXX_STANDARD. Debug builds use -Og
# instead of the makefile's -O2 so that code can be stepped through.
add_library(mspm0_sdk INTERFACE)
target_include_directories(mspm0_sdk INTERFACE "${MSPM0_SYSCFG_OUTPUT_DIR}")
# The SDK headers are C code that the C++20 warnings of -Wall flag (deprecated volatile and enum
# arithmetic in dl_opa.h, dl_adc12.h), so the directories are system directories (-isystem).
target_include_directories(mspm0_sdk SYSTEM INTERFACE
  "${MSPM0_SDK_INSTALL_DIR}/source/third_party/CMSIS/Core/Include"
  "${MSPM0_SDK_INSTALL_DIR}/source")
target_compile_options(mspm0_sdk INTERFACE
  "@${MSPM0_SYSCFG_OPTIONS}"
  $<IF:$<CONFIG:Debug>,-Og,-O2>
  -mcpu=cortex-m0plus
  -march=armv6-m
  -mthumb
  -mfloat-abi=soft
  -ffunction-sections
  -fdata-sections
  -g
  $<$<CONFIG:Debug>:-gstrict-dwarf>
  -Wall
  $<$<COMPILE_LANGUAGE:C>:-std=c99>
  # No C++ runtime features that cost size and need an operating system.
  $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
  $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>)
# driverlib comes from device.lds.genlibs. The linker script names it, and the archive has to
# follow the objects that use it, so link it explicitly as well, as the makefile does with
# -l:driverlib.a.
target_link_libraries(mspm0_sdk INTERFACE ${MSPM0_SYSCFG_LIBRARIES})

# The device startup file and the SysConfig output: SDK code, built with the makefile's
# warning level.
add_library(mspm0_device OBJECT "${MSPM0_STARTUP_FILE}" ${MSPM0_SYSCFG_SOURCES})
target_link_libraries(mspm0_device PUBLIC mspm0_sdk)

# ---- Application --------------------------------------------------------------------------
add_executable(${PROJECT_NAME} main.c User/app_main.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE User)
target_link_libraries(${PROJECT_NAME} PRIVATE mspm0_device)
target_compile_options(${PROJECT_NAME} PRIVATE -Werror)

# LFLAGS of the makefile: -nostartfiles -Tdevice.lds.genlibs -Wl,-T,device_linker.lds
# -Wl,-Map,<name>.map -L<sdk>/source -march=armv6-m -mthumb -static -Wl,--gc-sections
# --specs=nano.specs --specs=nosys.specs, with the libraries -lgcc -lc -lm (the compiler driver
# links them). -Wl,--no-warn-rwx-segment: the SRAM region of the SysConfig linker script is
# RWX, which binutils 2.39 and later warn about.
target_link_options(${PROJECT_NAME} PRIVATE
  -mcpu=cortex-m0plus
  -march=armv6-m
  -mthumb
  -mfloat-abi=soft
  -nostartfiles
  "-T${MSPM0_SYSCFG_GENLIBS}"
  "-Wl,-T,${MSPM0_SYSCFG_LINKER}"
  "-Wl,-Map,${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map"
  "-L${MSPM0_SDK_INSTALL_DIR}/source"
  -static
  -Wl,--gc-sections
  -Wl,--no-warn-rwx-segment
  --specs=nano.specs
  --specs=nosys.specs)
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS
  "${MSPM0_SYSCFG_LINKER}" "${MSPM0_SYSCFG_GENLIBS}")

# ---- LibXR and the XRobot modules ---------------------------------------------------------
include(cmake/LibXR.CMake)

# ---- Firmware images ----------------------------------------------------------------------
# Each link also writes <project>.hex and <project>.bin next to the ELF.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  WORKING_DIRECTORY $<TARGET_FILE_DIR:${PROJECT_NAME}>
  COMMAND ${CMAKE_OBJCOPY} --output-target ihex
    $<TARGET_FILE_NAME:${PROJECT_NAME}> ${PROJECT_NAME}.hex
  COMMAND ${CMAKE_OBJCOPY} --output-target binary
    $<TARGET_FILE_NAME:${PROJECT_NAME}> ${PROJECT_NAME}.bin
  VERBATIM)
