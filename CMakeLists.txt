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


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g3 -O3 -march=native  -fsanitize=address -fsanitize=undefined -fsanitize=leak ")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -g3 -O3 -march=native  -fsanitize=address -fsanitize=undefined -fsanitize=leak ")

message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message("CMAKE_CXX_COMPILER_ID: ${CMAKE_CXX_COMPILER_ID}")
message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

file(GLOB_RECURSE LVGL_SRC 
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_core/*.c
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_draw/*.c
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_font/*.c
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_gpu/*.c
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_hal/*.c
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_misc/*.c
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_themes/*.c
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_widgets/*.c
 ${PROJECT_SOURCE_DIR}/lv_drivers/*.c
 ${PROJECT_SOURCE_DIR}/lv_drivers/display/*.c
 ${PROJECT_SOURCE_DIR}/lv_drivers/indev/*.c
)

add_library(lvgl STATIC
 ${LVGL_SRC}
)
target_include_directories(lvgl PRIVATE 
 ${PROJECT_SOURCE_DIR}/lvgl
 ${PROJECT_SOURCE_DIR}/lvgl/src
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_draw
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_font
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_gpu
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_hal
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_misc
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_themes
 ${PROJECT_SOURCE_DIR}/lvgl/src/lv_widgets
 ${PROJECT_SOURCE_DIR}/lv_drivers
 ${PROJECT_SOURCE_DIR}/lv_drivers/display
 ${PROJECT_SOURCE_DIR}/lv_drivers/indev
)

target_compile_definitions(lvgl PRIVATE
 INPUT_DEVICE=/dev/input/touchscreen0
 LV_LVGL_H_INCLUDE_SIMPLE
 USE_EVDEV=1
 USE_FBDEV=1

# LV_LVGL_H_INCLUDE_SIMPLE
# USE_MONITOR=0
# USE_MOUSE=0
# USE_MOUSE_WHEEL=0
# USE_KEYBOARD=0
)



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

target_include_directories(chiton PRIVATE 
 include
 lvgl
)

target_link_libraries(chiton PRIVATE 
 m
 z
 png
 jpeg
 evdev
 stdc++
 lvgl
 pthread
 glog
 lvgl
 aditof
)

target_compile_definitions(chiton PRIVATE
 INPUT_DEVICE=/dev/input/touchscreen0
 LV_LVGL_H_INCLUDE_SIMPLE
 USE_EVDEV=1
 USE_FBDEV=1
)
