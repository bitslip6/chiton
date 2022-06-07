.DEFAULT_GOAL := chiton

# Get the platform architecture if not already defined
ARCH ?= $(shell uname -m)

#-------------------------------------------------------------
# Directories
#-------------------------------------------------------------
ROOT_DIR=.
SRC_DIR=$(ROOT_DIR)/src
API_SRC_DIR=$(ROOT_DIR)/api
TOOLS_DIR=$(ROOT_DIR)/tools
INC_ROOT=$(ROOT_DIR)/include
BUILD_DIR=$(ROOT_DIR)/_build
OBJ_DIR=$(BUILD_DIR)/obj
API_OBJ_DIR=$(BUILD_DIR)/obj/api
API_DIR=$(ROOT_DIR)/api
BIN_DIR=$(ROOT_DIR)/bin
LIB_DIR=$(ROOT_DIR)/lib

LVGL_DIR=$(ROOT_DIR)/lvgl
LVDRV_DIR=$(ROOT_DIR)/lv_drivers

#-------------------------------------------------------------
# Compiler and Toolchain
#-------------------------------------------------------------


CC ?= gcc
CPP ?= g++

#-------------------------------------------------------------
# Build Flags and Parameters
#-------------------------------------------------------------

# Use a native environment when ARCH=aarch64
ifeq ($(ARCH), aarch64)
	#MACHINE_SPEC=-march=armv8.3-a+fp16 -mabi=lp64 -mtune=cortex-a53 -mlow-precision-sqrt -mlow-precision-div
	#MACHINE_SPEC=-march=armv8.3-a+fp16 -mabi=lp64 -mtune=cortex-a53 -mlow-precision-sqrt -mlow-precision-div
	#MACHINE_SPEC=-march=armv8-a -mlow-precision-div -mtune=cortex-a53
	MACHINE_SPEC=-march=armv8-a+simd+fp16 -mtune=cortex-a53  -ftree-vectorize 
	#echo "SPEC: $MACHINE_SPEC"
else
	MACHINE_SPEC=-mfpmath=sse -mmmx -msse -msse2
endif

#ifeq ($(ARCH), aarch64)
#	CC=aarch64-poky-linux-gcc --sysroot=/opt/poky/3.2.1/sysroots/cortexa57-poky-linux
#	CPP=aarch64-poky-linux-g++  --sysroot=/opt/poky/3.2.1/sysroots/cortexa57-poky-linux
#endif

# Let the platform define its own input device event, or default to some event
INPUT_DEVICE ?= /dev/input/touchscreen0

# DEFINES = -DLV_LVGL_H_INCLUDE_SIMPLE

# Assume a desktop environment is not being used if compiling for aarch64
ifeq ($(ARCH), aarch64)
	DEFINES += \
	-DUSE_EVDEV=1 \
	-DUSE_FBDEV=1 \
	-DINPUT_DEVICE="$(INPUT_DEVICE)"
else
	DEFINES += \
	-DUSE_MONITOR=1 \
	-DUSE_MOUSE=1 \
	-DUSE_MOUSE_WHEEL=1 \
	-DUSE_KEYBOARD=1
endif

# Set DEBUG_MODE=ON to compile with debugging parameters
DEBUG_MODE ?= OFF
ifeq ($(DEBUG_MODE),ON)
	DEFINES += -DDEBUG_MODE
	#PERF=-Og -ggdb -fsanitize=address 
	#PERF=-Og -ggdb
	PERF=-Og -ggdb -DEIGEN_NO_DEBUG -ffast-math -funsafe-math-optimizations -freciprocal-math -fopt-info-vec-optimized
else
	PERF=-O3 -DEIGEN_NO_DEBUG -ffast-math -funsafe-math-optimizations -freciprocal-math -fopt-info-vec-optimized
	# PERF=-O3 -ffast-math -funsafe-math-optimizations -freciprocal-math -fopt-info-vec-optomized
	# PERF=-O3 -ffast-math -funsafe-math-optimizations -freciprocal-math -fopt-info-vec-missed
	# PERF=-O2
endif


WARNINGS=-Wall -Wshadow  -Wuninitialized -Wpedantic -Wextra -Wdouble-promotion -Wno-variadic-macros -Wno-psabi -Wno-narrowing -Wno-unused-variable

WARNINGS_REDUCED=-Wall -Wextra -Wdouble-promotion -Wno-variadic-macros -Wno-psabi

CPPFLAGS=-I$(INC_ROOT) \
-I$(LVGL_DIR) \
-I$(LVDRV_DIR) \
-I$(LVDRV_DIR)/display \
-I$(LVDRV_DIR)/indev \

COMMON_FLAGS= \
$(DEFINES) \
$(MACHINE_SPEC) \
$(PERF) \
-finline-functions \
-funit-at-a-time \
$(CPPFLAGS) \

CFLAGS = -std=gnu99 $(COMMON_FLAGS) $(WARNINGS) 
CXXFLAGS = -std=c++17 $(COMMON_FLAGS) $(WARNINGS)
CXX_NO_WARN_FLAGS = -std=c++17 $(COMMON_FLAGS) $(WARNINGS_REDUCED)

LDFLAGS = \
$(PERF) \
-fkeep-inline-functions \
-L$(LIB_DIR) \

LIBS = \
-lm \
-lz \
-lpng \
-ljpeg \
-levdev \
-lstdc++ \
-lpthread \
-linput \
-ldl \


#-lglog \
#

#-------------------------------------------------------------
# Sources
#-------------------------------------------------------------

# C Sources
#APP_SRC_C = \
$(SRC_DIR)/app/app_hal.c \
$(SRC_DIR)/app/assets/mouse_cursor_icon.c \

C_SOURCES = \
$(APP_SRC_C) \

# CXX Sources
CORE_SRC_CXX = \
$(SRC_DIR)/main.cpp \
$(SRC_DIR)/data.cpp \
$(SRC_DIR)/test.cpp \

API_SRC_CXX = \
$(SRC_DIR)/api/api.cpp \
$(SRC_DIR)/api/utils.cpp \

#APP_SRC_CXX = \
#$(SRC_DIR)/app/app.cpp \
#$(SRC_DIR)/app/app_theme.cpp \

SYSTEM_SRC_CXX = \
$(SRC_DIR)/system/system.cpp \
$(SRC_DIR)/system/utils.cpp \
$(SRC_DIR)/system/core.cpp \
$(SRC_DIR)/system/wifi.cpp \
$(SRC_DIR)/system/camera.cpp \
$(SRC_DIR)/system/rover.cpp \

#/usr/local/chiton/current/libaditof.so 

WEB_SRC_CXX = \
$(SRC_DIR)/webserver/webserver.cpp \
$(SRC_DIR)/webserver/software.cpp \
$(SRC_DIR)/webserver/settings.cpp \
$(SRC_DIR)/webserver/rover.cpp \

GRAPHICS_SRC_CXX = \
$(SRC_DIR)/graphics/images.cpp \
$(SRC_DIR)/graphics/shapes.cpp \
$(SRC_DIR)/graphics/calibration.cpp \
$(SRC_DIR)/graphics/opencl.cpp \
$(SRC_DIR)/color/hsv.cpp \

CXX_SOURCES = \
$(CORE_SRC_CXX) \
$(SYSTEM_SRC_CXX) \
$(WEB_SRC_CXX) \
$(GRAPHICS_SRC_CXX) \
$(APP_SRC_CXX) \


#-------------------------------------------------------------
# Objects
#-------------------------------------------------------------

_C_OBJECTS = $(patsubst %.c,%.o,$(C_SOURCES))
C_OBJECTS = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(_C_OBJECTS))

_CXX_OBJECTS = $(patsubst %.cpp,%.o,$(CXX_SOURCES))
CXX_OBJECTS = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(_CXX_OBJECTS))

CORE_OBJ = $(SRC_DIR)/math/3d.o math/3dshapes.o color/textures.o fxaa.o draw.o system.o lut.o ws2811/dma.o ws2811/mailbox.o ws2811/pcm.o ws2811/pwm.o ws2811/rpihw.o ws2811/ws2811.o color/hsv.o color/blends.o
CHITON_OBJ = $(OBJ_DIR)/chiton.o
API_OBJECTS = $(API_OBJ_DIR)/api.o $(API_OBJ_DIR)/utils.o
MAIN_OBJ = $(OBJ_DIR)/main.o
SCRATCH_OBJ = $(OBJ_DIR)/scratch.o
TEST_OBJ = $(OBJ_DIR)/test.o 
LVGL_OBJ = $(OBJ_DIR)/app/app.o $(OBJ_DIR)/app/app_hal.o $(OBJ_DIR)/app/app_theme.o
DEMO_OBJ = DEMO_OBJ.o retro/blob.o retro/tunnel.o retro/balloons.o retro/particle.o retro/aw.o retro/cube.o 

OBJECTS = $(CXX_OBJECTS)
MAIN_OBJECTS := $(filter-out $(TEST_OBJ),$(OBJECTS))
TEST_OBJECTS1 := $(filter-out $(MAIN_OBJ),$(OBJECTS))
TEST_OBJECTS := $(filter-out $(LVGL_OBJ),$(TEST_OBJECTS1))
SCRATCH_OBJECTS := $(OBJ_DIR)/system/utils.o $(OBJ_DIR)/scratch.o $(OBJ_DIR)/graphics/calibration.o $(OBJ_DIR)/graphics/shapes.o $(OBJ_DIR)/system/core.o $(OBJ_DIR)/data.o $(OBJ_DIR)/color/hsv.o $(OBJ_DIR)/graphics/images.o


#-------------------------------------------------------------
# Targets
#-------------------------------------------------------------

vpath %.c src
vpath %.c src/system
vpath %.h include

#-------------------------------------------------------------
# Recipes
#-------------------------------------------------------------

.PHONY: demo chiton test lvgl clean cleaner print run

setup:
	@mkdir -p $(LIB_DIR)
	@mkdir -p $(OBJ_DIR)
	@mkdir -p $(BIN_DIR)

demo: $(CORE_OBJ) $(DEMO_OBJ)
	@echo "Building demo binary: $(BIN_DIR)/demo"
	$(CC) $(LDFLAGS) -o $(BIN_DIR)/demo $(CORE_OBJ) $(DEMO_OBJ) $(LIBS) -llvgl -laditof  -lopencl

chiton: $(MAIN_OBJECTS) 
	@echo
	@echo "Building test binary: $(BIN_DIR)/chiton"
	@echo "CMD: " $(CPP) $(LDFLAGS) -o $(BIN_DIR)/chiton $(MAIN_OBJECTS) $(LIBS)  -li2c -laditof -lopencl
	$(CPP) $(LDFLAGS) -o $(BIN_DIR)/chiton $(MAIN_OBJECTS) $(LIBS) -li2c -laditof  -lopencl

scratch: $(SCRATCH_OBJECTS) 
	@echo
	@echo "Building test binary: $(BIN_DIR)/scratch"
	@echo "CMD: " $(CPP) $(LDFLAGS) -o $(BIN_DIR)/scratch $(SCRATCH_OBJECTS) $(LIBS)  -li2c -laditof -lopencl
	$(CPP) $(LDFLAGS) -o $(BIN_DIR)/scratch $(SCRATCH_OBJECTS) $(LIBS) -li2c -laditof  -lopencl

	
api: $(API_OBJECTS)
	@echo
	@echo "Building api library: $(BIN_DIR)/api.so"
	$(CPP) $(LDFLAGS) -shared -o $(BIN_DIR)/api.so $(API_OBJECTS) -lm


test: $(TEST_OBJECTS)
	@echo "test _ objects"
	@echo "$(TEST_OBJECTS)"
	@echo "objects"
	@echo "$(OBJECTS)"
	@echo "Building test binary: $(BIN_DIR)/test"
	$(CPP) $(LDFLAGS) -o $(BIN_DIR)/test $(TEST_OBJECTS) $(LIBS) -li2c -laditof  -lopencl


install: installchiton installweb

installscratch: scratch
	scp bin/scratch root@chiton.lan:/home/root/scratch/scratch

installapi: api 
	scp bin/api.so root@chiton.lan:/usr/local/chiton/current/api.so

installchiton: chiton
	scp bin/chiton root@chiton.lan:/usr/local/chiton/current/chiton2
	scp src/calibrate.cl root@chiton.lan:/usr/local/chiton/current/calibrate.cl

installweb:
	scp release/app.html root@chiton.lan:/usr/local/chiton/current/files/app.html

# Remove built targets and objects for the core project, leaving submodule
# libraries intact
clean:
	cp -r release/* bin
	rm -f $(BIN_DIR)/chiton
	rm -f $(BIN_DIR)/test
	rm -rf $(LIB_DIR)
	rm -rf $(OBJ_DIR)
	rm -rf _build
	cp release/app.html bin

# Remove everything for a fresh build
cleaner:  clean
	rm -rf $(BUILD_DIR)
	rm -rf $(LIB_DIR)
	find . -name '*.o' -exec rm -f {} \;
	mkdir bin

lvgl: setup
	$(MAKE) -f Makefile.lvgl CC="$(CC)"  CPP="$(CPP)"  ARCH="$(ARCH)" DEFINES="$(DEFINES)" PERF="$(PERF)" WARNINGS="$(WARNINGS_REDUCED)" MACHINE_SPEC="$(MACHINE_SPEC)"

print:
	@echo "arch: $(ARCH)"

#scratch: src/scratch.o src/system/utils.o src/graphics/calibration.o
#	$(CPP) $(LDFLAGS) -o $(BIN_DIR)/scratch src/scratch.o $(LIBS)
#	$(CPP) $(MACHINE_SPEC) -O2 -fopt-info-vec -ftree-vectorize -ffast-math -fassociative-math -I./include -S -fverbose-asm -O3 src/scratch.cpp src/system/utils.cpp src/graphics/calibration.cpp


run: test
	LD_LIBRARY_PATH=$(PWD)/lib:$(LD_LIBRARY_PATH) ./bin/test

#-------------------------------------------------------------
# Rules
#-------------------------------------------------------------

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	@echo " CPP $< $(CXXFLAGS)"
	@$(CPP) -c $(CXXFLAGS) -o $@ $<

$(OBJ_DIR)/app/%.o: $(SRC_DIR)/app/%.cpp
	@mkdir -p $(@D)
	@echo " CPP $<"
	@$(CPP) -c $(CXX_NO_WARN_FLAGS) -o $@ $<


$(API_OBJ_DIR)/%.o: $(API_SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	@echo " CPP $<"
	@$(CPP) -c $(CXXFLAGS) -fpic -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@echo " CC $<"
	@$(CC) -c $(CFLAGS) -o $@ $<
