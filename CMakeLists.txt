
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


################################################
##### JPEG #####################################
################################################

find_package(PkgConfig REQUIRED)
pkg_search_module(JPEG REQUIRED libjpeg)
link_directories(${JPEG_LIBDIR})

#################################################
########## ADITOF ###############################
#################################################

find_package(aditof REQUIRED)
set(ADITOF_LIBRARIES aditof::aditof)
set(ADITOF_INCLUDE_DIRS  aditof::aditof)


#####################################################
######### LANGUAGE OPTIONS ##########################
####################################################

set(CMAKE_C_STANDARD 11)
set(C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g3 -O3 ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g3 -O3")

#################################################
########### LVGL ################################
#################################################


file(GLOB_RECURSE LVGL_SRC 
  lvgl/src/lv_core/*.c
  lvgl/src/lv_draw/*.c
  lvgl/src/lv_font/*.c
  lvgl/src/lv_gpu/*.c
  lvgl/src/lv_hal/*.c
  lvgl/src/lv_misc/*.c
  lvgl/src/lv_themes/*.c
  lvgl/src/lv_widgets/*.c
  lv_drivers/*.c
  lv_drivers/display/*.c
  lv_drivers/indev/*.c
)

file(GLOB_RECURSE SOURCES_LVGL
lvgl/lv_core/*.c
lvgl/lv_draw/*.c
lvgl/lv_font/*.c
lvgl/lv_gpu/*.c
lvgl/lv_hal/*.c
lvgl/lv_misc/*.c
lvgl/lv_themes/*.c
lvgl/lv_widgets/*.c
)

file(GLOB_RECURSE SOURCES_LV_DRIVERS
lv_drivers/*.c
lv_drivers/display/*.c
lv_drivers/indev/*.c
)

add_library(lvgl SHARED
 ${SOURCES_LVGL}
 ${SOURCES_LV_DRIVERS}
)

target_include_directories(lvgl PUBLIC
lvgl 
lvgl/lv_core 
lvgl/lv_draw 
lvgl/lv_font 
lvgl/lv_gpu 
lvgl/lv_hal 
lvgl/lv_misc 
lvgl/lv_themes 
lvgl/lv_widget 
lv_drivers 
lv_drivers/display 
lv_drivers/indev 
)

target_compile_definitions(lvgl PUBLIC
 LV_LVGL_H_INCLUDE_SIMPLE
 USE_MONITOR=0
 USE_MOUSE=0
 USE_MOUSE_WHEEL=0
 USE_KEYBOARD=0
 USE_EVDEV=1
 USE_FBDEV=1
 INPUT_DEVICE=/dev/input/touchscreen0
)

install(TARGETS lvgl DESTINATION lib)



#################################################
########### CHITON ##############################
#################################################


set(APP_SRC_C 
src/app/app_hal.c
src/app/assets/mouse_cursor_icon.c
)
set(APP_SRC_CXX
src/app/app.cpp
src/app/app_theme.cpp
)

set(SYSTEM_SRC_CXX
src/system/system.cpp
src/system/utils.cpp
src/system/wifi.cpp
)

set(WEB_SRC_CXX
src/webserver/webserver.cpp
src/webserver/software.cpp
src/webserver/settings.cpp
)

set(GRAPHICS_SRC_CXX
src/graphics/images.cpp
src/graphics/shapes.cpp
src/color/hsv.cpp
)

set(CXX_SOURCES 
${APP_SRC_C}
${APP_SRC_CXX}
${SYSTEM_SRC_CXX} 
${WEB_SRC_CXX}
${GRAPHICS_SRC_CXX}
)

#############################
###
#############################

add_executable(chiton 
  ${CXX_SOURCES}
  src/main.cpp
)

target_include_directories(chiton  
#FunctionalPlus/include
  ${ADITOF_INCLUDE_DIRS}
#  ${JPEG_INCLUDE_DIRS}
)
target_link_libraries(chiton  
 ${ADITOF_LIBRARIES}
# ${JPEG_LIBRARIES}
)
#add_dependencies(chiton lvgl)

install(TARGETS chiton DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/release/files DESTINATION bin )
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/release/config.json DESTINATION etc )

#############################
###
#############################

add_executable(test 
  ${CXX_SOURCES}
  src/test.cpp
)

target_include_directories(test  PUBLIC
#  FunctionalPlus/include
  ${ADITOF_INCLUDE_DIRS}
#  ${JPEG_INCLUDE_DIRS}
)
target_link_libraries(test  
 ${ADITOF_LIBRARIES}
# ${JPEG_LIBRARIES}
)
#add_dependencies(test lvgl)

#############################
###
#############################

add_executable(update_json 
  tools/update_json/update_json.cpp
)




