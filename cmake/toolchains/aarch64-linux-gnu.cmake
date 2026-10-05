# AMD/x86 Linux host -> AArch64 Linux. This selects tools, not a deployment ABI.
# The distribution sysroot may be newer than a Yocto target: audit the output's
# GLIBC/GLIBCXX requirements before deployment. Prefer the target's Yocto SDK
# toolchain file when available.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CHITON_CROSS_PREFIX "aarch64-linux-gnu-" CACHE STRING "Cross compiler prefix, optionally including its directory")
set(CMAKE_C_COMPILER "${CHITON_CROSS_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${CHITON_CROSS_PREFIX}g++")

execute_process(COMMAND "${CMAKE_C_COMPILER}" -dumpmachine
  OUTPUT_VARIABLE _chiton_target OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE _chiton_target_status)
if(NOT _chiton_target_status EQUAL 0 OR NOT _chiton_target MATCHES "^aarch64(-|$)")
  message(FATAL_ERROR "Expected an AArch64 cross compiler, got '${_chiton_target}'")
endif()

# Let callers explicitly select a sysroot; otherwise use the compiler's own.
if(NOT CMAKE_SYSROOT)
  execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-sysroot
    OUTPUT_VARIABLE _chiton_sysroot OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _chiton_sysroot_status)
  if(NOT _chiton_sysroot_status EQUAL 0 OR NOT IS_DIRECTORY "${_chiton_sysroot}")
    message(FATAL_ERROR "Cannot determine cross sysroot; set CMAKE_SYSROOT explicitly")
  endif()
  set(CMAKE_SYSROOT "${_chiton_sysroot}" CACHE PATH "Target sysroot")
endif()

set(CHITON_TARGET_PREFIX "" CACHE PATH "Additional staging prefix containing AArch64 dependency headers/libraries")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
if(CHITON_TARGET_PREFIX)
  list(INSERT CMAKE_FIND_ROOT_PATH 0 "${CHITON_TARGET_PREFIX}")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# No assumption of FP16 or a specific i.MX8 CPU beyond baseline ARMv8-A.
set(CMAKE_C_FLAGS_INIT "-march=armv8-a")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
  CHITON_CROSS_PREFIX CHITON_TARGET_PREFIX)
