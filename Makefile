.DEFAULT_GOAL := test

# prod or debug build
#PERF=-O2
PERF=-Og -ggdb

IDIR = include
ODIR = obj


MKDIR_P = mkdir -p
CC=gcc
CPP=g++
WARNINGS=-Wall -Wextra -Wdouble-promotion -pedantic
MACHINE_SPEC=-mfpmath=sse -mmmx -msse -msse2
CFLAGS = $(WARNINGS) $(MACHIINE_SPEC) $(PERF) -std=gnu99 -finline-functions -funit-at-a-time -I$(IDIR)
CPPFLAGS = $(WARNINGS) $(MACHIINE_SPEC) $(PERF) -std=c++17 -finline-functions -funit-at-a-time -I$(IDIR)
LDFLAGS = $(PERF) -fkeep-inline-functions
LIBS = -lm  -lpng  -lSDL2 -lstdc++

CORE = src/math/3d.o math/3dshapes.o color/textures.o fxaa.o draw.o system.o lut.o ws2811/dma.o ws2811/mailbox.o ws2811/pcm.o ws2811/pwm.o ws2811/rpihw.o ws2811/ws2811.o color/hsv.o color/blends.o
TEST = obj/test.o
DEMOS = demos.o retro/blob.o retro/tunnel.o retro/balloons.o retro/particle.o retro/aw.o retro/cube.o
CORE_SRC = $(wildcard src/*.c)
CORE_OBJ = $(patsubst %.c,%.o,$(wildcard src/*.c))

DIRS = color graphics math retro system ws2811
DEPS = $(wildcard include/*.h)


SYSTEM_SRC = $(wildcard src/system/*.c)
_SYSTEM_OBJ = $(patsubst %.c,%.o,$(SYSTEM_SRC))
SYSTEM_OBJ = $(patsubst src/%,obj/%,$(_SYSTEM_OBJ))

MATH_SRC = $(wildcard src/math/*.c)
_MATH_OBJ = $(patsubst %.c,%.o,$(MATH_SRC))
MATH_OBJ = $(patsubst src/%,obj/%,$(_MATH_OBJ))

GFX_SRC = $(wildcard src/graphics/*.c)
_GFX_OBJ = $(patsubst %.c,%.o,$(GFX_SRC))
GFX_OBJ = $(patsubst src/%,obj/%,$(_GFX_OBJ))

COLOR_SRC = $(wildcard src/color/*.c)
_COLOR_OBJ = $(patsubst %.c,%.o,$(COLOR_SRC))
COLOR_OBJ = $(patsubst src/%,obj/%,$(_COLOR_OBJ))

ALL_SRC = $(wildcard src/*/*.c)
_ALL_OBJ = $(patsubst %.c,%.o,$(ALL_SRC))
ALL_OBJ = $(patsubst src/%,obj/%,$(_ALL_OBJ))

vpath %.c src
vpath %.c src/system
vpath %.h include



obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CPP) -c $(CPPFLAGS) -o $@ $<

obj/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) -o $@ $<

demo: $(CORE) $(DEMOS)
	$(CC) $(LDFLAGS) -o demo $(CORE) $(DEMOS) $(LIBS)
	mv demo bin

test: $(TEST) $(SYSTEM_OBJ) $(MATH_OBJ) $(GFX_OBJ) $(COLOR_OBJ)
	$(CPP) $(LDFLAGS) -o test $(SYSTEM_OBJ) $(MATH_OBJ) $(GFX_OBJ) $(COLOR_OBJ) $(TEST) $(LIBS)
	mv test bin

.PHONY: lvgl clean

clean:
	rm -rf bin
	rm -rf _build
	find . -name '*.o' -exec rm -f {} \;

lvgl:
	$(MAKE) -f Makefile.lvgl
