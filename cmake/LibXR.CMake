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

