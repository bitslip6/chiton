
cmake_minimum_required(VERSION 3.5.1)
project (chiton)

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

#################################################
########## SDL2 ###############################
#################################################

#find_package(SDL2 REQUIRED)
#include_directories(${SDL2_INCLUDE_DIRS})

#####################################################
######### LANGUAGE OPTIONS ##########################
####################################################

set(CMAKE_C_STANDARD 11)
set(C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -g3 -O3 ")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g3 -O3")

#################################################
########### LVGL ################################
#################################################


file(GLOB_RECURSE SOURCES_LVGL 
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

message("SOURCES_LVGL: ${SOURCES_LVGL}")

add_library(lvgl STATIC
 ${SOURCES_LVGL}
)

set(LVGL_INCLUDES
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

target_include_directories(lvgl PUBLIC
${LVGL_INCLUDES}
)

set(LVGL_FLAGS 
LV_LVGL_H_INCLUDE_SIMPLE
USE_MONITOR=0
USE_MOUSE=0
USE_MOUSE_WHEEL=0
USE_KEYBOARD=0
USE_EVDEV=1
USE_FBDEV=1
INPUT_DEVICE=/dev/input/touchscreen0
)

target_compile_definitions(lvgl PUBLIC
   ${LVGL_FLAGS}
)

target_link_libraries(lvgl  
 input
)

install(TARGETS lvgl DESTINATION lib)


#############################
### JSON UPDATE
#############################

add_executable(update_json 
  tools/update_json/update_json.cpp
)
target_include_directories(update_json  PUBLIC
  include
)
install(TARGETS update_json DESTINATION bin)

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
### CHITON LIBRARY
#############################

message("SOURCES_CHITON: ${CXX_SOURCES}")

add_library(chitonlib STATIC
   ${CXX_SOURCES}
)

target_include_directories(chitonlib PRIVATE
  include
  ${LVGL_INCLUDES}
  ${ADITOF_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIRS}
)
target_link_libraries(chitonlib PRIVATE
 ${ADITOF_LIBRARIES}
 ${JPEG_LIBRARIES}
 evdev
 lvgl
)

target_compile_definitions(chitonlib PUBLIC
 ${LVGL_FLAGS}
)
install(TARGETS chitonlib DESTINATION lib)

add_dependencies(chitonlib lvgl)


##################################
## CHITON MAIN
##################################

add_executable(chiton
  src/main.cpp
)
target_include_directories(chiton PRIVATE
  include
  ${LVGL_INCLUDES}
  ${ADITOF_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIRS}
)
target_link_libraries(chiton PRIVATE
 chitonlib
 pthread
 ${ADITOF_LIBRARIES}
)
install(TARGETS chiton DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/release/files DESTINATION bin )
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/release/config.json DESTINATION bin )

add_dependencies(chiton chitonlib)


##################################
## CHITON TEST
##################################

add_executable(chiontest 
  src/test.cpp
)
target_include_directories(chiontest PRIVATE
  include
  ${LVGL_INCLUDES}
  ${ADITOF_INCLUDE_DIRS}
  ${JPEG_INCLUDE_DIRS}
)
target_link_libraries(chiontest PRIVATE
 chitonlib
)
install(TARGETS chiontest DESTINATION bin)

add_dependencies(chiontest chitonlib)




