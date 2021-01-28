.DEFAULT_GOAL := api

# Get the platform architecture if not already defined
ARCH ?= $(shell uname -m)

#-------------------------------------------------------------
# Directories
#-------------------------------------------------------------
ROOT_DIR=.
OBJ_DIR=.
SRC_DIR=.
BIN_DIR=.

#-------------------------------------------------------------
# Compiler and Toolchain
#-------------------------------------------------------------

CPP=g++

#-------------------------------------------------------------
# Build Flags and Parameters
#-------------------------------------------------------------

# Use a native environment when ARCH=aarch64
ifeq ($(ARCH), aarch64)
	MACHINE_SPEC=-march=armv8-a -mlow-precision-div -mtune=cortex-a53 -ftree-vectorizer-verbose=2
else
	MACHINE_SPEC=-mfpmath=sse -mmmx -msse -msse2
endif

# Set DEBUG_MODE=ON to compile with debugging parameters
DEBUG_MODE ?= OFF
ifeq ($(DEBUG_MODE),ON)
	DEFINES += -DDEBUG_MODE
	PERF=-Og -ggdb
else
	PERF=-O3 -ffast-math
endif

WARNINGS=-Wall -Wpedantic -Wextra -Wdouble-promotion -Wno-variadic-macros

CPPFLAGS=-I. -I../include

COMMON_FLAGS= \
$(DEFINES) \
$(WARNINGS) \
$(MACHINE_SPEC) \
$(PERF) \
-finline-functions \
-funit-at-a-time \
$(CPPFLAGS) \

CXXFLAGS = -std=c++17 $(COMMON_FLAGS)

LDFLAGS = \
$(PERF) \
-fkeep-inline-functions \


#LIBS = \
#-lm \
#-lz \
#-lpng \
#-ljpeg \
#-levdev \
#-lstdc++ \
#-llvgl \
#-lpthread \
#-linput \
#-ldl \
#-lglog \


#-------------------------------------------------------------
# Sources
#-------------------------------------------------------------

# CXX Sources
CORE_SRC_CXX = \
$(SRC_DIR)/main.cpp \
$(SRC_DIR)/data.cpp \
$(SRC_DIR)/test.cpp \

APP_SRC_CXX = \
$(SRC_DIR)/app/app.cpp \
$(SRC_DIR)/app/app_theme.cpp \

API_SRC_CXX = \
$(API_DIR)/api.cpp \
$(API_DIR)/../src/system/utils.cpp \

SYSTEM_SRC_CXX = \
$(SRC_DIR)/system/system.cpp \
$(SRC_DIR)/system/utils.cpp \
$(SRC_DIR)/system/core.cpp \
$(SRC_DIR)/system/wifi.cpp \
/usr/local/chiton/current/libaditof.so 

WEB_SRC_CXX = \
$(SRC_DIR)/webserver/webserver.cpp \
$(SRC_DIR)/webserver/software.cpp \
$(SRC_DIR)/webserver/settings.cpp \

GRAPHICS_SRC_CXX = \
$(SRC_DIR)/graphics/images.cpp \
$(SRC_DIR)/graphics/shapes.cpp \
$(SRC_DIR)/color/hsv.cpp \

CXX_SOURCES = \
$(API_SRC_CXX) \

#-------------------------------------------------------------
# Objects
#-------------------------------------------------------------

_CXX_OBJECTS = $(patsubst %.cpp,%.o,$(CXX_SOURCES))
CXX_OBJECTS = $(patsubst $(SRC_DIR)/%,$(OBJ_DIR)/%,$(_CXX_OBJECTS))

API_OBJECTS = $(OBJ_DIR)/api.o $(OBJ_DIR)/utils.o


#-------------------------------------------------------------
# Targets
#-------------------------------------------------------------

vpath %.c src
vpath %.c src/system
vpath %.h include

#-------------------------------------------------------------
# Recipes
#-------------------------------------------------------------

.PHONY: api

	
api: $(API_OBJECTS)
	@echo
	@echo "Building api library: $(BIN_DIR)/api.so"
	$(CPP) $(LDFLAGS) -shared -o $(BIN_DIR)/api.so $(API_OBJECTS) $(LIBS)
	
install: api
	cp api.so /usr/local/chiton/current/api.so



# Remove built targets and objects for the core project, leaving submodule
# libraries intact
clean:
	rm -f $(OBJ_DIR)/*.o
	rm -f $(OBJ_DIR)/*.so

#-------------------------------------------------------------
# Rules
#-------------------------------------------------------------

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	@echo " CPP $<"
	@$(CPP) -c $(CXXFLAGS) -fpic -o $@ $<

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@echo " CC $<"
	@$(CC) -c $(CFLAGS) -o $@ $<
