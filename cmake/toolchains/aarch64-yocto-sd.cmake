# Modern AMD-hosted AArch64 GCC frontend with the development runtime recovered
# from this project's Yocto 2.5 SD image. No target programs are executed.
# Supply -DCMAKE_SYSROOT=/absolute/path/to/the/copied/sysroot and use a fresh
# build directory. This image contains GCC 7.3.0 headers/startup/runtime files.
if(NOT CMAKE_SYSROOT OR NOT IS_DIRECTORY "${CMAKE_SYSROOT}")
  message(FATAL_ERROR "Set CMAKE_SYSROOT to the copied Yocto SD development sysroot")
endif()
set(CHITON_YOCTO_GCC_VERSION "7.3.0" CACHE STRING "Recovered Yocto GCC runtime version")
set(CHITON_YOCTO_TRIPLE "aarch64-poky-linux" CACHE STRING "Recovered Yocto compiler triple")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
  CHITON_YOCTO_GCC_VERSION CHITON_YOCTO_TRIPLE)
include("${CMAKE_CURRENT_LIST_DIR}/aarch64-linux-gnu.cmake")

set(_yocto_cxx "${CMAKE_SYSROOT}/usr/include/c++/${CHITON_YOCTO_GCC_VERSION}")
set(_yocto_gcc "${CMAKE_SYSROOT}/usr/lib/${CHITON_YOCTO_TRIPLE}/${CHITON_YOCTO_GCC_VERSION}")
foreach(_required
    "${_yocto_cxx}/${CHITON_YOCTO_TRIPLE}/bits/c++config.h"
    "${_yocto_gcc}/crtbeginS.o" "${_yocto_gcc}/crtendS.o" "${_yocto_gcc}/libgcc.a"
    "${CMAKE_SYSROOT}/usr/include/features.h"
    "${CMAKE_SYSROOT}/usr/lib/Scrt1.o" "${CMAKE_SYSROOT}/usr/lib/libstdc++.so")
  if(NOT EXISTS "${_required}")
    message(FATAL_ERROR "Incomplete Yocto development sysroot: missing ${_required}")
  endif()
endforeach()
execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-file-name=include
  OUTPUT_VARIABLE _frontend_include OUTPUT_STRIP_TRAILING_WHITESPACE
  RESULT_VARIABLE _frontend_include_status)
if(NOT _frontend_include_status EQUAL 0 OR NOT EXISTS "${_frontend_include}/arm_neon.h")
  message(FATAL_ERROR "Cannot locate the modern frontend's ARM intrinsic headers")
endif()

# This distribution's compiler normally searches /usr/aarch64-linux-gnu/include
# even when --sysroot is changed. Suppress all implicit headers, then explicitly
# choose modern compiler intrinsics and the SD image's libc/libstdc++ headers.
set(_headers "-nostdinc -isystem \"${_frontend_include}\"")
set(_runtime "-B\"${_yocto_gcc}/\" -B\"${CMAKE_SYSROOT}/usr/lib/\" -L\"${_yocto_gcc}\" -L\"${CMAKE_SYSROOT}/usr/lib\" -L\"${CMAKE_SYSROOT}/lib\"")
# GCC 7's libgcc has no newer outlined AArch64 atomic helpers. Emit baseline
# inline atomics instead; do not add the modern distribution libgcc to fix this.
string(APPEND CMAKE_C_FLAGS_INIT " -mno-outline-atomics ${_headers} -isystem \"${CMAKE_SYSROOT}/usr/include\" ${_runtime}")
string(APPEND CMAKE_CXX_FLAGS_INIT " -mno-outline-atomics ${_headers} -nostdinc++ -isystem \"${_yocto_cxx}\" -isystem \"${_yocto_cxx}/${CHITON_YOCTO_TRIPLE}\" -isystem \"${_yocto_cxx}/backward\" -isystem \"${CMAKE_SYSROOT}/usr/include\" ${_runtime}")
string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -Wl,-rpath-link,${CMAKE_SYSROOT}/usr/lib -Wl,-rpath-link,${CMAKE_SYSROOT}/lib")
