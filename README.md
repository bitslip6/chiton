# Chiton Project

**NOTE:** This is ported from older code bases and could use some code cleanup.

* [Dependencies]
    - [Submodules]
* [Building the Project and Application]
    - [LVGL and Drivers]
    - [Build Output Structure]
    - [Binary Execution]
* [Docker]
    - [Installing Docker]
    - [Build the Chiton Image]
    - [Run and Compile]
        + [Startup Errors]
    - [Detached Containers]
* [Compilation Notes]

---

## Dependencies
[Dependencies]: #dependencies

* [docker](https://docs.docker.com/get-docker/)
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

## Docker
[Docker]: #docker

Docker is used as an easy-to-use alternative to QEMU, running an instance of
an `arm64v8a` Linux operating system that compiles and runs the project code
as if it was on the associated operating system.

### Installing Docker
[Installing Docker]: #installing-docker

Docker supports [all modern operating systems][get-docker].
It is strongly recommended to use a Linux distribution instead of
MacOS or Windows, especially as MacOS
[has trouble supporting USB device access][docker-usb-macos].

**Disclaimer:** this README assumes _Ubuntu 18.04_ because that is what it
has been developed on.
Support for running the containers in the same way on MacOS is not tested.

A quick overview guide for installing Docker on Ubuntu Linux, as taken from
their [official walkthrough][install-docker-ubuntu].

Before installing, make sure the core dependencies are present on your system.
With basic Ubuntu, they most likely already are.

```
sudo apt-get update
sudo apt-get install \
    apt-transport-https \
    ca-certificates \
    curl \
    gnupg-agent \
    software-properties-common
```

Add their repository:

```
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -
sudo add-apt-repository \
   "deb [arch=amd64] https://download.docker.com/linux/ubuntu \
   $(lsb_release -cs) \
   stable"
```

Install the packages:

```
sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io
```

You can verify the install with their `hello-world` image:

```
sudo docker run hello-world
```

It is recommended to add your user to the `docker` group so that you don't
have to run sudo every time you need to use Docker:

```
sudo usermod -aG docker $USER
```

**You will need to log out and back in** before the group change takes effect.

[get-docker]: https://docs.docker.com/get-docker/
[docker-usb-macos]: https://github.com/docker/for-mac/issues/900
[install-docker-ubuntu]: https://docs.docker.com/engine/install/ubuntu/

### Build the Chiton Image
[Build the Chiton Image]: #build-the-chiton-image

The image that will be used is defined in the `Dockerfile` located in the root
directory of this repository.
In order to derive usable **containers** _(instances of the image)_ from it,
first build the image by pointing it at the `Dockerfile` and giving it a
`<name>:<tag>` _(both don't really matter as long as they don't conflict with
other images in your client; we'll assume `chiton:latest`)_:

```
# From the repository root
docker build -t chiton:latest .
```

Grab a coffee.
After it has completed, you can see the built image in your client's list of
tracked images:

```
$ docker images
REPOSITORY                   TAG                 IMAGE ID            CREATED             SIZE
chiton                       stable              6d94e947b416        46 hours ago        1.22GB
chiton                       latest              3258beef0709        6 days ago          1.14GB
arm64v8/ubuntu               18.04               53ffe7187a86        3 weeks ago         57.7MB
```

In the example above, there are multiple `chiton` images with different tags.
If the image needs to be tweaked or experimented with, it's easy to do by
changing the `Dockerfile` accordingly and then building it again with a
a different tag.

Specifying an existing tag will overwrite that image in the client.
Separate tags are useful for not rebuilding older, stable images.

### Run and Compile
[Run and Compile]: #run-and-compile

Docker containers are used via the `docker run` command.
For this project, we pass it additional options for supporting the ability to
launch the GUI app:

```
$ docker run -ti \
-v /tmp/.X11-unix:/tmp/.X11-unix \
-e DISPLAY=unix$DISPLAY \
--device /dev/dri:/dev/dri:rw \
--network host \
-v $PWD:/workdir \
-u $UID:$GID \
chiton:<appropriate tag>
```

Explained, in order:

* `-ti`: Use an interactive terminal
* Mount the host's `X11` socket as a volume with the same path location
* Define the `DISPLAY` to use in the container environment
* Give the `/dev/dri` device to the container with read/write permissions
* Use the host's network
* Mount the current working directory as `/workdir` in the container
* Do not execute as root but as the host `user:group`
    - This is **VERY IMPORTANT** so that built files do not get placed into the
project directory as `root:root`, requiring `sudo` for any further interaction
by the host.

Use `docker run --help` or their [official run reference manual][docker-run]
for more information on the available options.

If the container starts up correctly, a prompt will appear denoting the Docker
user and the host's hostname, `user@<your hostname>:/workdir$`.
From here, the container will behave like any Linux terminal, allowing you to
build the project:

```
user@<your hostname>:/workdir$ make -j8 DESKTOP=ON DEBUG=ON && ./bin/test
```

**NOTE:** The amount of commands available in the terminal are _quite_ limited,
due to only necessary packages getting installed in the container to minimize
its size. See `Dockerfile` for the list of installed packages.

To quit back to the host terminal, use `exit`.

#### Startup Errors
[Startup Errors]: #startup-errors

After calling the above `docker run` command, if you see this:

```
standard_init_linux.go:211: exec user process caused "exec format error"
```

then Docker is upset that a container with a different architecture than the
host is trying to execute.
There is a [very long, detailed walkthrough][docker-arm-linux] by Docker on
getting a Linux host set up to run ARM architecture containers, but the easier
way is to simply run the following command:

```
docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
```

This setups up QEMU architectures to be called by `binfmt` in the system, which
then enables the project container to be run _(the walkthrough eventually gets
to this point after many other steps)_.

The downside is that this will have to be executed upon every login/restart
of the host machine.
There are probably ways around this that enable it permanently _(such as the
walkthrough, perhaps)_, but they haven't been fully explored at this time.

[docker-arm-linux]: https://www.docker.com/blog/getting-started-with-docker-for-arm-on-linux/

### Detached Containers
[Detached Containers]: #detached-containers

Sometimes it's useful to keep a container running in the background instead of
booting one up and down on every recompile:

```
$ docker run -dti \
-v /tmp/.X11-unix:/tmp/.X11-unix \
-e DISPLAY=unix$DISPLAY \
--device /dev/dri:/dev/dri:rw \
--network host \
-v $PWD:/workdir \
-u $UID:$GID \
chiton:latest

de14a7ae87dcca506ae2cd129051eea2cfec818f5a0edeea905f519274d44ed0
```

Adding the `-d` option launches the container in a _detached_ state in the
background client.

From here, you can _attach_ to it:

```
$ docker attach de14a7ae87dc

user@<your hostname>:/workdir$
```

Or execute a command in it while keeping it in the background:

```
$ docker exec de14a7ae87dc ls -lhX --group-directories-first
total 6.0M
drwxrwxr-x 11 user user 4.0K Jun 18 19:33 FunctionalPlus
drwxr-xr-x  5 user user 4.0K Jul  8 13:45 _build
drwxr-xr-x  2 user user 4.0K Jul  8 15:19 bin
drwxrwxr-x  5 user user 4.0K Jul  8 19:56 include
drwxr-xr-x  2 user user 4.0K Jul  6 21:10 lib
drwxrwxr-x  4 user user 4.0K Jun 24 22:49 linux
drwxrwxr-x  6 user user 4.0K Jun 15 20:33 lv_drivers
drwxrwxr-x  8 user user 4.0K Jun 16 13:25 lvgl
drwxrwxr-x  2 user user 4.0K Jun 12 19:13 poc_src
drwxrwxr-x  7 user user 4.0K Jul  8 19:56 src
drwxrwxr-x  5 user user 4.0K Jun 12 19:13 tools
-rw-rw-r--  1 user user 2.5K Jul  6 21:19 Dockerfile
-rw-rw-r--  1 user user   88 Jun 12 19:13 LICENSE
-rw-rw-r--  1 user user 5.9K Jul  8 14:42 Makefile
-rw-rw-r--  1 user user  23K Jun 29 20:41 lv_conf.h
-rw-rw-r--  1 user user  11K Jun 23 15:23 lv_drv_conf.h
-rw-rw-r--  1 user user 4.0K Jun 22 12:51 Makefile.lvgl
-rw-rw-r--  1 user user 9.4K Jul  8 21:00 README.md
-rw-rw-r--  1 user user  20K Jun 12 19:13 consolas.png
-rw-rw-r--  1 user user  20K Jun 12 19:13 large_font.png
-rw-rw-r--  1 user user 6.6K Jun 12 19:13 small_font.png
-rw-rw-r--  1 user user 1.1K Jun 12 19:13 tiny.png
-rw-rw-r--  1 user user 1.1K Jun 12 19:13 tiny_font.png
```

Use `sh -c "..."` to execute multiple commands in a single execute call:

```
$ docker exec de14a7ae87dc sh -c "make -j8 DESKTOP=ON DEBUG=ON && ./bin/test"
 CC src/app/mouse_cursor_icon.c
 CPP src/main.cpp
 CPP src/app/app.cpp
/usr/bin/make -f Makefile.lvgl DEFINES="-DLV_LVGL_H_INCLUDE_SIMPLE -DUSE_MONITOR=1 -DUSE_MOUSE=1 -DUSE_MOUSE_WHEEL=1
-DUSE_KEYBOARD=1 -DDEBUG" PERF="-Og -ggdb" WARNINGS="-Wall -Wextra -Wdouble-promotion" MACHINE_SPEC="-march=native"
make[1]: Entering directory '/workdir'
Linking liblvgl.so into project release directory
make[1]: Leaving directory '/workdir'

Building test binary: ./bin/test
g++ -Og -ggdb -fkeep-inline-functions -L./lib -L/opt/ComputeLibrary/lib/linux-arm64-v8a-neon -o ./bin/test
./_build/obj/app/mouse_cursor_icon.o ./_build/obj/app/app_hal.o ./_build/obj/app/assets/warehouse.o ./_build/obj/color/hsv.o
./_build/obj/color/blends.o ./_build/obj/color/textures.o ./_build/obj/color/image.o ./_build/obj/graphics/shader.o
./_build/obj/graphics/fxaa.o ./_build/obj/math/3dshapes.o ./_build/obj/math/mathcore.o ./_build/obj/math/lut.o
./_build/obj/math/3d.o ./_build/obj/system/system.o ./_build/obj/system/util.o ./_build/obj/main.o ./_build/obj/app/wifi.o
./_build/obj/app/app_utils.o ./_build/obj/app/app_theme.o ./_build/obj/app/app.o ./_build/obj/utils/Utils.o -lm  -lpng
-lSDL2 -lstdc++ -llvgl -lpthread -larm_compute -larm_compute_core
libGL error: failed to get driver name for fd 4
libGL error: MESA-LOADER: failed to open i965 (search paths /usr/lib/aarch64-linux-gnu/dri:\$${ORIGIN}/dri:/usr/lib/dri)
libGL error: failed to load driver: i965
libGL error: failed to open drm device: Permission denied
libGL error: failed to load driver: i965

# The app should be visible on the desktop now
```

[docker-run]: https://docs.docker.com/engine/reference/commandline/run/

## Compilation Notes
[Compilation Notes]: #compilation-notes

To allow simultaneous compilation between desktop builds and actual hardware
builds, a few switch statements exist in the `Makefile` to toggle compilation
flags:

Switch  | ON                            | OFF                   | Default
:----:  | :---:                         | :---:                 | :-----:
DESKTOP | Compile for desktop use       | Compile for hardware  | ON
DEBUG   | Build image with debug flags  | Build optimized image | ON

Switches don't need to be specified _(see default values)_, but each switch can
be passed into the build via the command line:

```
make

# OR

make -j8 DESKTOP=OFF DEBUG=ON

# etc.
```
