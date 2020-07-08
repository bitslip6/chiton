.DEFAULT_GOAL := test

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

ifeq ($(ARCH), aarch64)
	ARM_COMPUTE_DIR ?= /opt/ComputeLibrary
else
	ARM_COMPUTE_DIR ?= $(ROOT_DIR)/ComputeLibrary
endif

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

DEFINES=-DLV_LVGL_H_INCLUDE_SIMPLE

# Set DESKTOP=ON when compiling the app to show on the desktop's monitor
DESKTOP ?= ON
ifeq ($(DESKTOP),ON)
	DEFINES +=-DUSE_MONITOR=1 \
	-DUSE_MOUSE=1 \
	-DUSE_MOUSE_WHEEL=1 \
	-DUSE_KEYBOARD=1
else
	DEFINES += -DUSE_MONITOR=0 \
	-DUSE_MOUSE=0 \
	-DUSE_MOUSE_WHEEL=0 \
	-DUSE_KEYBOARD=0
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
-I$(ARM_COMPUTE_DIR) \
-I$(ARM_COMPUTE_DIR)/include \
-I$(ARM_COMPUTE_DIR)/utils \

COMMON_FLAGS= $(DEFINES) $(WARNINGS) $(MACHINE_SPEC) $(PERF) -finline-functions -funit-at-a-time $(CPPFLAGS)
CFLAGS = -std=gnu99 $(COMMON_FLAGS)
CXXFLAGS = -std=c++17 $(COMMON_FLAGS)
LDFLAGS = $(PERF) -fkeep-inline-functions -L$(LIB_DIR) -L$(ARM_COMPUTE_DIR)/lib/linux-arm64-v8a-neon
LIBS = -lm  -lpng  -lSDL2 -lstdc++ -llvgl -lpthread -larm_compute -larm_compute_core

# List of warnings to disable when compiling ARM Compute Library source code
ARM_COMPUTE_WARNINGS = -Wno-double-promotion

#-------------------------------------------------------------
# Sources
#-------------------------------------------------------------

CORE_SRC_C = $(wildcard $(SRC_DIR)/*.c)
APP_SRC_C = $(wildcard $(SRC_DIR)/app/*.c)
APP_SRC_C += $(wildcard $(SRC_DIR)/app/assets/*.c)
COLOR_SRC_C = $(wildcard $(SRC_DIR)/color/*.c)
GFX_SRC_C = $(wildcard $(SRC_DIR)/graphics/*.c)
MATH_SRC_C = $(wildcard $(SRC_DIR)/math/*.c)
SYSTEM_SRC_C = $(wildcard $(SRC_DIR)/system/*.c)

C_SOURCES = $(CORE_SRC_C) \
$(APP_SRC_C) \
$(COLOR_SRC_C) \
$(GFX_SRC_C) \
$(MATH_SRC_C) \
$(SYSTEM_SRC_C) \

CORE_SRC_CXX = $(wildcard $(SRC_DIR)/*.cpp)
APP_SRC_CXX = $(wildcard $(SRC_DIR)/app/*.cpp)
COLOR_SRC_CXX = $(wildcard $(SRC_DIR)/color/*.cpp)
GFX_SRC_CXX = $(wildcard $(SRC_DIR)/graphics/*.cpp)
MATH_SRC_CXX = $(wildcard $(SRC_DIR)/math/*.cpp)
SYSTEM_SRC_CXX = $(wildcard $(SRC_DIR)/system/*.cpp)

CXX_SOURCES = $(CORE_SRC_CXX) \
$(APP_SRC_CXX) \
$(COLOR_SRC_CXX) \
$(GFX_SRC_CXX) \
$(MATH_SRC_CXX) \
$(SYSTEM_SRC_CXX) \

ARM_CXX_SOURCES = $(ARM_COMPUTE_DIR)/utils/Utils.cpp

#-------------------------------------------------------------
# Objects
#-------------------------------------------------------------

_C_OBJECTS = $(patsubst %.c,%.o,$(C_SOURCES))
C_OBJECTS = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(_C_OBJECTS))

_CXX_OBJECTS = $(patsubst %.cpp,%.o,$(CXX_SOURCES))
CXX_OBJECTS = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(_CXX_OBJECTS))

_ARM_CXX_OBJECTS = $(patsubst %.cpp,%.o,$(ARM_CXX_SOURCES))
ARM_CXX_OBJECTS = $(patsubst $(ARM_COMPUTE_DIR)/%,$(OBJ_DIR)/%,$(_ARM_CXX_OBJECTS))

CORE_OBJ = $(SRC_DIR)/math/3d.o math/3dshapes.o color/textures.o fxaa.o draw.o system.o lut.o ws2811/dma.o ws2811/mailbox.o ws2811/pcm.o ws2811/pwm.o ws2811/rpihw.o ws2811/ws2811.o color/hsv.o color/blends.o
TEST_OBJ = $(OBJ_DIR)/main.o
DEMO_OBJ = DEMO_OBJ.o retro/blob.o retro/tunnel.o retro/balloons.o retro/particle.o retro/aw.o retro/cube.o

OBJECTS = $(C_OBJECTS) $(CXX_OBJECTS) $(ARM_CXX_OBJECTS)

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
	@echo ""

#-------------------------------------------------------------
# Rules
#-------------------------------------------------------------

$(OBJ_DIR)/%.o: $(ARM_COMPUTE_DIR)/%.cpp
	@mkdir -p $(@D)
	@echo " CPP $<"
	@$(CPP) -c $(CXXFLAGS) $(ARM_COMPUTE_WARNINGS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	@echo " CPP $<"
	@$(CPP) -c $(CXXFLAGS) -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@echo " CC $<"
	@$(CC) -c $(CFLAGS) -o $@ $<
