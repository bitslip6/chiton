# Chiton Project

**NOTE:** This is ported from older code bases and could use some code cleanup.

## Dependencies

* `make`
* `libpng-dev`

Install them through the package manager.

## Build Output Structure

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
