
# Ubuntu 16.04 cmake is 3.5.1

cmake_minimum_required(VERSION 3.5.1)
project (chiton)


### SOURCE
message("CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
### BUILD
message("CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")
### SYSROOT
message("CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}" )
### DEPLOY


message("CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}" )


message("CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message("CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}")
message("CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}")
message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

######################################################
##### USE CCACHE TO SPEEDUP BUILD ####################
######################################################

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    message("CCACHE FOUND!!!!")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${CCACHE_PROGRAM}")
else()
    message("CCACHE NOT FOUND!!!!")
endif()

#####################################################
######### LANGUAGE OPTIONS ##########################
####################################################

set(CMAKE_C_STANDARD 11)
set(C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g3 -O3 --coverage -pg ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g3 -O3 --coverage -pg ")

#################################################
########## ADITOF ###############################
#################################################

#if ( "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64" )
########## NATIVE BUILD ON DEVICE ###########
find_package(aditof REQUIRED)
set(ADITOF_LIBRARIES aditof::aditof)
set(ADITOF_INCLUDE_DIRS  aditof::aditof)
#else()
########## CROSS COMPILER ENVIRONMENT ###########
#set(ADITOF_LIBRARIES  aditof pthread)
#set(ADITOF_INCLUDE_DIRS  aditof pthread)
#endif()


#################################################
########### LVGL ################################
#################################################


file(GLOB_RECURSE LVGL_SRC 
  ../lvgl/*.c
  ../lvgl/*.cpp
  ../lv_drivers/*.c
  ../lv_drivers/*.cpp
)
message("LVGL_SRC: ${LVGL_SRC}")

add_library(lvgl STATIC
 ${LVGL_SRC}
)
target_include_directories(lvgl PUBLIC
 ../lvgl
 ../lvgl/src
 ../lvgl/src/lv_draw
 ../lvgl/src/lv_font
 ../lvgl/src/lv_gpu
 ../lvgl/src/lv_hal
 ../lvgl/src/lv_misc
 ../lvgl/src/lv_themes
 ../lvgl/src/lv_widgets
 ../lv_drivers
 ../lv_drivers/display
 ../lv_drivers/indev
)

target_compile_definitions(lvgl PUBLIC
 INPUT_DEVICE=/dev/input/touchscreen0
 LV_LVGL_H_INCLUDE_SIMPLE
 USE_EVDEV=1
 USE_FBDEV=1
)


################################################
##### JPEG #####################################
################################################

find_package(PkgConfig REQUIRED)
pkg_search_module(JPEG REQUIRED libjpeg)
link_directories(${JPEG_LIBDIR})

#################################################
########### CHITON ##############################
#################################################

file(GLOB_RECURSE CHITON_SRC 
  ../src/*.c
  ../src/*.cpp
)

# filter is added in Cmake 3.6.0
if(${CMAKE_VERSION} VERSION_LESS "3.6.0") 
############### UBUNTU 16.04 and YOCTO 2.5 SUMO ###################
set(CHITON_SRC 
../src/app/app.cpp
../src/app/app_hal.c
../src/app/app_theme.cpp
../src/app/assets/mouse_cursor_icon.c
../src/color/hsv.cpp
../src/color/textures.cpp
../src/graphics/images.cpp
../src/system/settings.cpp
../src/system/software.cpp
../src/system/system.cpp
../src/system/utils.cpp
../src/system/webserver.cpp
../src/system/wifi.cpp
)
else()
#################### UBUNTU 20.04 and YOCTO 3.1 DUNFELL ###################
# exclude old  files
list(FILTER CHITON_SRC EXCLUDE REGEX "src/.app/*")
list(FILTER CHITON_SRC EXCLUDE REGEX "src/system/camera.cpp")
list(FILTER CHITON_SRC EXCLUDE REGEX "src/color/image.c")
list(FILTER CHITON_SRC EXCLUDE REGEX "src/color/blends.c")
# exclude main files
list(FILTER CHITON_SRC EXCLUDE REGEX "src/system/i2c-addi9036.cpp")
list(FILTER CHITON_SRC EXCLUDE REGEX "src/main.cpp")
list(FILTER CHITON_SRC EXCLUDE REGEX "src/test.cpp")
########################################################
endif()

message("CHITON_SRC: ${CHITON_SRC}")





add_executable(chiton 
 ${CHITON_SRC}
 ../src/main.cpp
)
install(TARGETS chiton DESTINATION bin)

target_include_directories(chiton  PUBLIC
  ../include
  ../lvgl
  ${ADITOF_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIRS}
)
target_link_libraries(chiton  
 lvgl
 ${ADITOF_LIBRARIES}
 ${JPEG_LIBRARIES}
)

target_compile_definitions(chiton PUBLIC
 INPUT_DEVICE=/dev/input/touchscreen0
 LV_LVGL_H_INCLUDE_SIMPLE
 USE_EVDEV=1
 USE_FBDEV=1
)



if(0)

add_executable(chiton_test
 ${CHITON_SRC}
 src/test.cpp
)
install(TARGETS chiton_test DESTINATION bin)
target_include_directories(chiton_test  PUBLIC
  include
  lvgl
  ${ADITOF_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIRS}
)
target_link_libraries(chiton_test
 lvgl
 ${ADITOF_LIBRARIES}
 ${JPEG_LIBRARIES}
)
target_compile_definitions(chiton_test PUBLIC
 INPUT_DEVICE=/dev/input/touchscreen0
 LV_LVGL_H_INCLUDE_SIMPLE
 USE_EVDEV=1
 USE_FBDEV=1
)
endif()


#################################################
########### INSTALL  ############################
#################################################

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../release/files DESTINATION bin )
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../release/config.json DESTINATION chiton/current )
