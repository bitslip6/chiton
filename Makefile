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

#-------------------------------------------------------------
# Compiler and Toolchain
#-------------------------------------------------------------

CC=gcc
CPP=g++

#-------------------------------------------------------------
# Build Flags and Parameters
#-------------------------------------------------------------

# prod or debug build
#PERF=-O2
PERF=-Og -ggdb

WARNINGS=-Wall -Wextra -Wdouble-promotion
MACHINE_SPEC=-mfpmath=sse -mmmx -msse -msse2

DEFINES=-DLV_LVGL_H_INCLUDE_SIMPLE

# Uncomment these for compiling on embedded platform
# DEFINES = -DUSE_MONITOR=0 \
# _DUSE_MOUSE=0 \
# _DUSE_MOUSE_WHEEL=0 \
# _DUSE_KEYBOARD=0 \

# Uncomment this when compiling for desktop app development
DEFINES +=-DUSE_MONITOR=1 \
-DUSE_MOUSE=1 \
-DUSE_MOUSE_WHEEL=1 \
-DUSE_KEYBOARD=1 \

# Enable debug logging if enabled from command line
ifeq ($(DEBUG), ON)
	DEFINES += -DDEBUG
endif

CPPFLAGS=-I$(INC_ROOT) \
-I$(ROOT_DIR)/lvgl \
-I$(ROOT_DIR)/lv_drivers \
-I$(ROOT_DIR)/lv_drivers/display \
-I$(ROOT_DIR)/lv_drivers/indev

COMMON_FLAGS= $(DEFINES) $(WARNINGS) $(MACHINE_SPEC) $(PERF) -finline-functions -funit-at-a-time $(CPPFLAGS)
CFLAGS = -std=gnu99 $(COMMON_FLAGS)
CXXFLAGS = -std=c++17 $(COMMON_FLAGS)
LDFLAGS = $(PERF) -fkeep-inline-functions -L$(LIB_DIR)
LIBS = -lm  -lpng  -lSDL2 -lstdc++ -llvgl -lpthread

#-------------------------------------------------------------
# Sources
#-------------------------------------------------------------

CORE_SRC_C = $(wildcard $(SRC_DIR)/*.c)
APP_SRC_C = $(wildcard $(SRC_DIR)/app/*.c)
APP_SRC_C += $(wildcard $(SRC_DIR)/app/images/*.c)
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

# OBJECTS = $(C_OBJECTS) $(CXX_OBJECTS)
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
	@echo ""

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
