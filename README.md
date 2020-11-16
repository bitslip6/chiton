# Chiton Project

Toolchain | GCC | Devops Status
-- | -- | --
Yocto 2.5 | 7.3.0 |![.github/workflows/ubuntu1804_gcc_7.3.0_yocto_2.5.yml](https://github.com/bitslip6/chiton/workflows/.github/workflows/ubuntu1804_gcc_7.3.0_yocto_2.5.yml/badge.svg)
Yocto 3.1 | 9.3.0 |![.github/workflows/ubuntu1804_gcc_9.3.0_yocto_3.1.yml](https://github.com/bitslip6/chiton/workflows/.github/workflows/ubuntu1804_gcc_9.3.0_yocto_3.1.yml/badge.svg)
Ubuntu 18.04 | 7.5.0 | ![.github/workflows/ubuntu1804_gcc_7.5.0_intel.yml](https://github.com/bitslip6/chiton/workflows/.github/workflows/ubuntu1804_gcc_7.5.0_intel.yml/badge.svg)



**NOTE:** This is ported from older code bases and could use some code cleanup.

* [Dependencies]
    - [Submodules]
* [Building the Project and Application]
    - [LVGL and Drivers]
    - [Build Output Structure]
    - [Binary Execution]

<br>
---
<br>

### Compile
* git submodule init ; git submodule update
* git fetch -r
* make lvgl
* make

## Dependencies
[Dependencies]: #dependencies

* `make`
* `libpng-dev`

Install them through the package manager.

### Submodules
[Submodules]: #submodules

This project uses submodules for bringing in some 3rd party source code:

* [FunctionalPlus](https://github.com/Dobiasd/FunctionalPlus)
* [lvgl](https://github.com/lvgl/lvgl)
* [lv_drivers](https://github.com/lvgl/lv_drivers)

Make sure that submodules are up to date before attempting to compile any code:

```
git submodule init
git submodule update
```

**NOTE:** If there are strange build errors about header files not being found,
update the submodules again to make sure they were checked out after switching
branches, etc.

## Building the Project and Application
[Building the Project and Application]: #building-the-project-and-application

Building the project and application binaries can be run with a simple `make`
call.
A `make clean` call will wipe the `chiton` source code object files while
keeping built dependency libraries around for faster recompile.

## LVGL and Drivers
[LVGL and Drivers]: #lvgl-and-drivers

LVGL source code is compiled into a shared library that is ultimately linked
into `chiton` source code.
This library is a prerequisite to the `chiton` code and is always built ahead
of it when calling `make`.

To explicitly build the LVGL library:

```
make lvgl
```

Similarly, to clean up the built library for a recompile:

```
make cleaner
```

This will also remove built `chiton` source objects so that they will be
rebuilt and linked with the new library code.

### Build Output Structure
[Build Output Structure]: #build-output-structure

Build outputs for all dependency libraries and core project files under kept
under the same build directory, `_build`, for tidiness and ease of cleaning:

```
chiton
    |
    +--> _build/
    |       |
    |       +--> lvgl/              (1)
    |       |       |
    |       |       +-->lib/        (2)
    |       |       +-->obj/        (3)
    |       +--> obj/               (4)
    |       +--> ...
    |
    +--> bin/                       (5)
    +--> lib/                       (6)
```

As referenced above:


1. Build folder for a dependency library, such as `lvgl`.
2. Subdirectory containing built `.so` files for the library
3. Built object files for the library
4. Built object files for the core project, such as those in `chiton/src`
5. Binary outputs from `Makefile`
6. Common library directory. Dependency libraries, such as `liblvgl.so` are
    symbolically linked here to the library built by their associated
    `Makefile.<library>`. The main `Makfile` then only has to link against a
    single folder for linking all library files into the project build.

### Binary Execution
[Binary Execution]: #binary-execution

Currently, executing a built binary like `./bin/test` will fail because it
can't see the `liblvgl.so` library.
A present workaround is adding the output path to the `LD_LINKER_PATH`
environment variable:

```
# From the repo's root directory
export LD_LIBRARY_PATH=$PWD/lib:$LD_LIBRARY_PATH
```
