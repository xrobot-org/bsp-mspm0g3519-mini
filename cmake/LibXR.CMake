# LibXR integration of the BSP: platform None (no operating system, the timers run from the
# idle loop) with the MSPM0 drivers.

# Path to the LibXR source tree (the libxr submodule).
set(LIBXR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libxr" CACHE PATH "Path to LibXR source tree")

if(NOT EXISTS "${LIBXR_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "LibXR not found. Please set LIBXR_DIR correctly: ${LIBXR_DIR}")
endif()

# LibXR platform/driver selection
set(LIBXR_SYSTEM None)
set(LIBXR_DRIVER mspm0)
set(LIBXR_NO_EIGEN True)

# XRobot modules (resolved by `xrobot setup`)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Modules")
  set(XROBOT_MODULES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Modules")
endif()

# Import LibXR as a subproject
add_subdirectory("${LIBXR_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libxr")

# LibXR compiles with the options and the headers of the SDK build (see CMakeLists.txt).
target_link_libraries(xr PUBLIC mspm0_sdk)

# The application and the LibXR drivers include LibXR headers.
target_link_libraries(${PROJECT_NAME} PRIVATE xr)

# Treat warnings in LibXR and in the application as errors; the SDK code keeps the warning
# level of the SDK makefile.
target_compile_options(xr PRIVATE -Werror)

# Warnings that GCC 14 reports in the LibXR MSPM0 driver sources at -O2 (LibXR 5d442b9b), which
# -Werror turns into errors. They are suppressed for these sources only; remove a line when
# LibXR fixes the source it names.
set_source_files_properties("${LIBXR_DIR}/driver/mspm0/mspm0_atomic_shim.c"
  TARGET_DIRECTORY xr PROPERTIES COMPILE_OPTIONS -Wno-builtin-declaration-mismatch)
set_source_files_properties("${LIBXR_DIR}/driver/mspm0/mspm0_gpio.cpp"
  TARGET_DIRECTORY xr PROPERTIES COMPILE_OPTIONS -Wno-array-bounds)
set_source_files_properties("${LIBXR_DIR}/driver/mspm0/mspm0_timebase.cpp"
  TARGET_DIRECTORY xr PROPERTIES COMPILE_OPTIONS -Wno-volatile)
# system/none/thread.cpp:8: Thread::Current() returns a Thread whose handle is never set;
# GCC reports it at -Og.
set_source_files_properties("${LIBXR_DIR}/system/none/thread.cpp"
  TARGET_DIRECTORY xr PROPERTIES COMPILE_OPTIONS -Wno-uninitialized)
