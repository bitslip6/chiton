cmake_minimum_required(VERSION 3.10)

project (CHITON VERSION 0.0.1)
include(CTest)
enable_testing()
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)

set(CMAKE_C_STANDARD 11)
set(C_STANDARD_REQUIRED ON)
set(C_EXTENSIONS NO)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CXX_EXTENSIONS NO)


message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

add_executable(chiton 
 src/app/app_hal.c
 src/app/assets/mouse_cursor_icon.c
 src/main.cpp
 src/app/app.cpp
 src/app/app_theme.cpp
 src/system/system.cpp
 src/system/webserver.cpp
 src/system/settings.cpp
 src/system/utils.cpp
 src/system/wifi.cpp
 src/graphics/images.cpp
 src/color/hsv.cpp
)
