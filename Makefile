.DEFAULT_GOAL := test

# Get the platform architecture if not already defined
ARCH ?= $(shell uname -m)

#-------------------------------------------------------------
# Directories
#-------------------------------------------------------------
ROOT_DIR=.
SRC_DIR=$(ROOT_DIR)/src
INC_ROOT=$(ROOT_DIR)/include
BUILD_DIR=$(ROOT_DIR)/_build
OBJ_DIR=$(BUILD_DIR)/obj
BIN_DIR=$(ROOT_DIR)/bin
LIB_DIR=$(ROOT_DIR)/lib

LVGL_DIR=$(ROOT_DIR)/lvgl
LVDRV_DIR=$(ROOT_DIR)/lv_drivers

#-------------------------------------------------------------
# Compiler and Toolchain
#-------------------------------------------------------------

CC=gcc
CPP=g++

#-------------------------------------------------------------
# Build Flags and Parameters
#-------------------------------------------------------------

# Use a native environment when ARCH=aarch64
ifeq ($(ARCH), aarch64)
	MACHINE_SPEC=-march=native
else
	MACHINE_SPEC=-mfpmath=sse -mmmx -msse -msse2
endif

DEFINES = -DLV_LVGL_H_INCLUDE_SIMPLE

# Assume a desktop environment is not being used if compiling for aarch64
ifeq ($(ARCH), aarch64)
	DEFINES += \
	-DUSE_EVDEV=1 \
	-DUSE_FBDEV=1
else
	DEFINES += \
	-DUSE_MONITOR=1 \
	-DUSE_MOUSE=1 \
	-DUSE_MOUSE_WHEEL=1 \
	-DUSE_KEYBOARD=1
endif

# Set DEBUG=ON to compile with debugging parameters
DEBUG ?= ON
ifeq ($(DEBUG),ON)
	DEFINES += -DDEBUG
	PERF=-Og -ggdb
else
	PERF=-O2
endif

WARNINGS=-Wall -Wextra -Wdouble-promotion

CPPFLAGS=-I$(INC_ROOT) \
-I$(LVGL_DIR) \
-I$(LVDRV_DIR) \
-I$(LVDRV_DIR)/display \
-I$(LVDRV_DIR)/indev \

COMMON_FLAGS= \
$(DEFINES) \
$(WARNINGS) \
$(MACHINE_SPEC) \
$(PERF) \
-finline-functions \
-funit-at-a-time \
$(CPPFLAGS) \

CFLAGS = -std=gnu99 $(COMMON_FLAGS)

CXXFLAGS = -std=c++17 $(COMMON_FLAGS)

LDFLAGS = \
$(PERF) \
-fkeep-inline-functions \
-L$(LIB_DIR) \

LIBS = \
-lm \
-lpng \
-lstdc++ \
-llvgl \
-lpthread \

#-------------------------------------------------------------
# Sources
#-------------------------------------------------------------

# C Sources
APP_SRC_C = \
$(SRC_DIR)/app/app_hal.c \
$(SRC_DIR)/app/assets/mouse_cursor_icon.c \

C_SOURCES = \
$(APP_SRC_C) \

# CXX Sources
CORE_SRC_CXX = \
$(SRC_DIR)/main.cpp \

APP_SRC_CXX = \
$(SRC_DIR)/app/app.cpp \
$(SRC_DIR)/app/app_theme.cpp \

SYSTEM_SRC_CXX = \
$(SRC_DIR)/system/system.cpp \
$(SRC_DIR)/system/utils.cpp \
$(SRC_DIR)/system/wifi.cpp \

CXX_SOURCES = \
$(CORE_SRC_CXX) \
$(APP_SRC_CXX) \
$(SYSTEM_SRC_CXX) \

#-------------------------------------------------------------
# Objects
#-------------------------------------------------------------

_C_OBJECTS = $(patsubst %.c,%.o,$(C_SOURCES))
C_OBJECTS = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(_C_OBJECTS))

_CXX_OBJECTS = $(patsubst %.cpp,%.o,$(CXX_SOURCES))
CXX_OBJECTS = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(_CXX_OBJECTS))

CORE_OBJ = $(SRC_DIR)/math/3d.o math/3dshapes.o color/textures.o fxaa.o draw.o system.o lut.o ws2811/dma.o ws2811/mailbox.o ws2811/pcm.o ws2811/pwm.o ws2811/rpihw.o ws2811/ws2811.o color/hsv.o color/blends.o
TEST_OBJ = $(OBJ_DIR)/main.o
DEMO_OBJ = DEMO_OBJ.o retro/blob.o retro/tunnel.o retro/balloons.o retro/particle.o retro/aw.o retro/cube.o

OBJECTS = $(C_OBJECTS) $(CXX_OBJECTS)

#-------------------------------------------------------------
# Targets
#-------------------------------------------------------------

vpath %.c src
vpath %.c src/system
vpath %.h include

#-------------------------------------------------------------
# Recipes
#-------------------------------------------------------------

.PHONY: demo test lvgl clean cleaner print

setup:
	@mkdir -p $(LIB_DIR)
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

demo: $(CORE_OBJ) $(DEMO_OBJ)
	@echo "Building demo binary: $(BIN_DIR)/demo"
	$(CC) $(LDFLAGS) -o $(BIN_DIR)/demo $(CORE_OBJ) $(DEMO_OBJ) $(LIBS)

test: lvgl $(OBJECTS)
	@echo
	@echo "Building test binary: $(BIN_DIR)/test"
	$(CPP) $(LDFLAGS) -o $(BIN_DIR)/test $(OBJECTS) $(LIBS)


# Remove built targets and objects for the core project, leaving submodule
# libraries intact
clean:
	rm -rf $(BIN_DIR)
	rm -rf $(OBJ_DIR)

# Remove everything for a fresh build
cleaner:  clean
	rm -rf $(BUILD_DIR)
	rm -rf $(LIB_DIR)
	find . -name '*.o' -exec rm -f {} \;

lvgl: setup
	$(MAKE) -f Makefile.lvgl DEFINES="$(DEFINES)" PERF="$(PERF)" WARNINGS="$(WARNINGS)" MACHINE_SPEC="$(MACHINE_SPEC)"

print:
	@echo "arch: $(ARCH)"

#-------------------------------------------------------------
# Rules
#-------------------------------------------------------------

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	@echo " CPP $<"
	@$(CPP) -c $(CXXFLAGS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@echo " CC $<"
	@$(CC) -c $(CFLAGS) -o $@ $<
