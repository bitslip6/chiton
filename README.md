# Chiton · Wide-Angle 940 nm Time-of-Flight Sensor for Autonomous Mobile Robots

[![Web release gates](https://github.com/bitslip6/chiton/actions/workflows/ubuntu1804_gcc_7.5.0_intel.yml/badge.svg)](https://github.com/bitslip6/chiton/actions/workflows/ubuntu1804_gcc_7.5.0_intel.yml)

[See the application](#see-the-application) · [Engineering highlights](#engineering-highlights) · [Run it locally](#run-on-your-desktop)

Chiton controls and processes data from the **Chiton 8-vixel, 940 nm
Time-of-Flight camera**: **270° coverage, VGA (640 × 480), and 30 fps**.
The application combines sensor control, on-device processing, live visualization,
measurements, and data export. The camera serves the browser interface directly.

Chiton runs on embedded Linux and uses C++ for camera SDK integration, image
processing, and network services. Build tools, automated tests, and deployment
bundles support development from the desktop to the camera.

[![Chiton browser interface displaying a color-mapped depth image with measurement and export controls](images/chiton-color-dog.png)](images/chiton-color-dog.png)

*Color-mapped depth view. Colors represent distance across the scene.
Select any screenshot to view it at full size.*

## Camera platform

The camera platform combines an **i.MX8 system-on-module**, a **Sony CMOS image sensor**, and an **Analog Devices (ADI)
Time-of-Flight ASIC**. ToF imaging measures distance using emitted light and its
return, producing depth data as well as infrared intensity images.

| | Camera platform |
| --- | --- |
| Sensing | 8-vixel, 940 nm Time-of-Flight; depth and infrared intensity |
| Field of view | 270 degrees |
| Acquisition | VGA, 640 × 480 pixels at 30 frames per second |
| Compute | i.MX8 SOM running embedded Linux |
| Connectivity | Gigabit Ethernet and/or 2.4 GHz wireless |
| Extensibility | Customer C++ processing code runs on the camera through a native plugin |
| Client access | Browser UI, HTTP data exports, and persistent JPEG WebSocket streaming |

The platform supports hardware encode/decode and 30-fps data export in a variety
of formats. Processing workload, format, codec path, and network conditions
determine the delivered frame rate. The application encodes JPEGs with
**libjpeg**. Validate hardware codec operation and throughput on the target camera.

## Explore and process sensor data

Use Chiton to:

1. **Explore the scene.** Switch between depth and IR, adjust visualization, and
   inspect the camera's wide-angle view from a browser.
2. **Understand the measurements.** Select pixels or regions and inspect values,
   statistics, units, and the frame's processing state.
3. **Develop on the device.** Add customer processing through `api.so`, keeping
   frame processing close to the sensor.
4. **Integrate with other tools.** Export processed 16-bit images, JPEGs, binary
   point clouds, and OBJ vertices through documented interfaces.
5. **Develop on your desktop.** Use file-backed camera simulation and automated
   tests on a Linux development machine.

## See the application

| Grayscale view | 3D point-cloud view |
| --- | --- |
| [![Grayscale wide-angle camera view with pixel measurements and display controls](images/greyscale.png)](images/greyscale.png) | [![Chiton point-cloud visualization with measurement and export controls](images/chiton-point-cloud.png)](images/chiton-point-cloud.png) |
| Inspect the scene and selected-pixel values. | Explore the scene as spatial points and export its geometry. |

The application also supports an **unwrapped planar view**, presenting the
curved camera image as a horizontal strip. See the
[planar-view diagnostic capture](images/chiton-unwrap-dog.png).

The screenshots show the application interface and its visualization modes.
The planar capture includes development telemetry. Use controlled performance
tests to measure throughput and latency.

## Engineering highlights

| Area | Implementation |
| --- | --- |
| **Embedded camera integration** | Integrates the ADI SDK, manages capture and device-global settings, and supports framebuffer and headless operation on Linux |
| **Image processing and geometry** | Calibrates and smooths samples, renders grayscale/color images, remaps planar views, and generates point clouds |
| **On-device extensibility** | Runs customer frame-processing code through a native C++ plugin interface |
| **Networked visualization** | Serves the browser UI, streams JPEGs with bounded per-viewer buffering, and provides snapshot fallback |
| **Data integrity** | Binds metadata to snapshots, reports units and frame IDs, isolates measurement histories, and checks calibration storage |
| **Developer experience** | Provides file-backed simulation, portable ARM/x86 SIMD support, CMake/Make builds, and documented data contracts |
| **Validation and deployment** | Runs sanitizer and browser CI gates, retains failure logs, and packages versioned AArch64 bundles for operator-controlled deployment and rollback |

### How the pieces fit together

```text
ToF camera + ADI SDK          File-backed simulator
             \                         /
              +---- Capture pipeline --+
                            |
                  On-device processing
             calibration / customer plugin / rendering
                            |
              +-------------+--------------+
              |                            |
     Processed data + metadata      JPEG encoding + publication
              |                            |
     HTTP measurements/exports      WebSocket / HTTP preview
              |                            |
              +------ Browser / client ----+
```

The camera application manages the SDK and processes frames on the device.
Clients consume published data through HTTP and WebSocket interfaces. Raw data
and JPEGs retain their source-frame metadata as each processing stage completes.
Slow JPEG viewers skip intermediate frames to keep their buffers bounded.

For implementation details, see [image streaming](docs/image-streaming.md),
[measurement semantics](docs/measurements.md), and
[HTTP contracts](docs/http-contracts.md).

## Run on your desktop

Build Chiton on **Linux with a C++17 compiler**. Follow this Ubuntu/Debian
example to run the application with file-backed depth and IR inputs.

### 1. Get the source and dependencies

```sh
git clone https://github.com/bitslip6/chiton.git
cd chiton
git submodule update --init -- FunctionalPlus

sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config python3 \
  libpng-dev libjpeg-dev zlib1g-dev libi2c-dev libsimde-dev
```

Initialize the named build dependency and use CMake 3.10 or newer. You can also
supply SIMDe through a [source checkout](https://github.com/simd-everywhere/simde)
by adding `-DSIMDE_INCLUDE_DIR=/path/to/simde` to the configuration command.

### 2. Build and run

```sh
cmake -S . -B _build/desktop \
  -DUSE_ADITOF_STUB=ON \
  -DUSE_SDL2=OFF \
  -DCHITON_DEFAULT_DISPLAY=headless
cmake --build _build/desktop -j2

python3 tools/generate_stub_images.py --pattern gradient --output-dir /tmp/chiton-images
ADITOF_STUB_IR=/tmp/chiton-images/aditof-ir.bin \
ADITOF_STUB_DEPTH=/tmp/chiton-images/aditof-depth.bin \
  ./_build/desktop/chiton --display headless
```

Open **`http://localhost/`** or **`http://localhost:8080/`**, using the port that
the application reports in its startup log. The app tries port 80, then 8080;
allow it to bind to at least one available port. Stop it with **Ctrl+C**.

The generator creates a gradient for repeatable testing. It preserves existing
images by default; add `--force` to replace them.

<details>
<summary><strong>Simulator inputs, desktop preview, and Make builds</strong></summary>

Each simulator input contains exactly 640 × 480 little-endian unsigned 16-bit
samples (614,400 bytes) in headerless, row-major order. Camera samples use the
12-bit range 0–4095. The simulator repeats these images on every frame and
rejects explicitly selected files that are missing or have an incorrect size.
Measure sensor performance separately on the camera.

By default, the simulator uses `aditof-ir.bin` and `aditof-depth.bin` in the
working directory. It creates missing default files with every pixel set to 320.

For a local preview window, install `libsdl2-dev` and build separately:

```sh
cmake -S . -B _build/desktop-sdl -DUSE_ADITOF_STUB=ON -DUSE_SDL2=ON
cmake --build _build/desktop-sdl -j2
./_build/desktop-sdl/chiton --display desktop
```

Choose `auto`, `desktop`, `framebuffer`, or `headless` for the display backend.
Headless mode runs capture and network services independently of a local screen.

Alternatively, use Make:

```sh
make USE_ADITOF_STUB=1 USE_SDL2=0
./bin/chiton --display headless
```

Make outputs `bin/chiton` and isolates build variants under `_build/`.
Use `make print` to inspect configuration. Make selects native compilers by
default; pass compiler and architecture overrides explicitly.

</details>

## Connect a client or add your own processing

On a running camera, open `http://<camera-address>/`. Add `:8080` when the
startup log reports that port. The device address and wireless provisioning depend on its
installed image and configuration.

| Output | Route | Format |
| --- | --- | --- |
| Frame metadata | `/frame/camera` | JSON mode, type, dimensions, frame ID, age, and processing state |
| Processed depth or IR | `/raw/camera` | 640 × 480 unsigned 16-bit samples; response headers identify units and byte order |
| Preview image | `/image/camera` | JPEG; the selected mode controls image dimensions |
| Binary point cloud | `/pointcloud/camera` | Signed 16-bit X/Y/Z/distance records |
| OBJ point cloud | `/pointcloud_obj/camera` | ASCII vertices; coordinates divided by 6000 |

**Settings apply to every connected client.** Select cloud mode (4) before
point-cloud exports. Frame reads preserve the selected mode. The `/raw/camera`
endpoint returns processed samples that may include calibration, smoothing, and
plugin edits. Consult the [data contracts](docs/http-contracts.md) for processing
semantics, sample layouts, and units.

To wait for a newer snapshot, use `?after=<frame_id>&timeout_ms=1000` on a frame
endpoint. Ordinary reads may return 503 during a transition; newer-frame waits
return 504 on expiry, and incompatible formats return 409. Wait on the JPEG
endpoint itself when you need an encoded image—raw metadata can publish first.
Timestamps record when the host receives each frame.

### Customer processing plugins

[`api/api.cpp`](api/api.cpp) and [`api/api.h`](api/api.h) provide the example
`process_standard_frame` callback and optional `set_runlines` mapping callback.
Build the example from the repository root with **`make api`**. For the camera,
compile against its architecture and ABI and include `api.so` in the release's
resource location. Build a separate plugin binary for each target architecture.

Plugins execute trusted native code with the application's process privileges.
Their execution time contributes to the frame-processing budget. Review the
[plugin compatibility contract](docs/measurements.md), including private-copy
ownership and the legacy final-element counter, before integrating your code.

## Build and deploy to the camera

Install the **ADI SDK**, an ARM/AArch64 toolchain, and libraries compatible
with the device's Linux image before building for hardware. Select
`USE_ADITOF_STUB=OFF` in CMake or `USE_ADITOF_STUB=0` in Make.

- [Cross-build and target-library compatibility](docs/aarch64-cross-build.md)
- [Versioned application bundles, configuration, and rollback](docs/deployment-bundle.md)
- [Release process and hardware qualification checklist](docs/release-gates.md)

Deploy assets and private libraries with the executable. Preserve device
configuration/calibration and a known-good release, and stop the existing
camera owner before starting another. Run one camera-owning process at a time.

**Network boundary:** restrict access to a trusted network or protect remote
access with an authenticated TLS reverse proxy. The application relies on that
external boundary for authentication and transport security. Device-management
routes can change device state.

## Quality and validation

The live badge at the top links to the **Web release gates** workflow. GitHub
Actions runs on pushes and pull requests, with separate checks for:

- **Normal and sanitizer builds:** AddressSanitizer, UndefinedBehaviorSanitizer,
  and ThreadSanitizer headless simulator variants on Ubuntu 22.04.
- **Application contracts:** native tests and simulator HTTP checks for frames,
  measurements, exports, settings, and lifecycle behavior.
- **Browser behavior:** Chromium recovery/download checks for both shipped UI
  copies, alongside deterministic UI tests.

Run the same desktop gates locally with build dependencies, Node.js, and Chromium:

```sh
python3 tests/release_gate.py --all --output /tmp/chiton-release-checks --jobs 2
```

Use a **new output directory**. The gate stops on failed checks and retains logs.
See [focused validation commands](tests/SAFETY.md) and the
[release-gate scope](docs/release-gates.md).

**Validation scope:** desktop CI exercises software behavior with simulated
inputs. Qualify optical performance, illumination safety, hardware codecs, and
sustained network throughput on the target camera. The legacy Yocto workflows
perform checkout when you trigger them manually. The server handles ordinary
HTTP requests sequentially and runs persistent JPEG streaming through a separate
bounded reactor. The [web-server review](docs/web-server-review.md) tracks
remaining engineering work.

## Explore the repository

| Location | Contents |
| --- | --- |
| `src/`, `include/` | Camera integration, processing, display, and network services |
| `api/` | Customer frame-processing plugin example |
| `release/` | Browser assets and application resources |
| `images/` | Application screenshots |
| `tests/` | Native, HTTP, browser, and release-gate checks |
| `tools/` | Fixture generation, packaging, and development utilities |
| `docs/` | Architecture details, data contracts, deployment, and engineering notes |
| `linux/`, `devops/`, `deprecated/` | Hardware support and legacy platform material |

For display work, see [TFT refresh qualification](docs/display-refresh-benchmark.md)
and [SPI driver diagnostics](docs/spi-display-diagnostics.md).
