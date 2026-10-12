# TFT refresh qualification

## Build and run

`./cross-build.sh --name rel_display_test` builds and audits
`benchmarks/display_refresh_benchmark` and includes its launcher, sweep script,
and this guide in the ordinary AArch64 bundle. It does **not** execute ARM code,
deploy, touch a framebuffer, or alter driver settings on the build host.
The benchmark has no camera SDK or SDL dependencies.

On the compatible target, stop Chiton and any other framebuffer writers first.
Use the new release's launcher without activating it as `current`:

```sh
./benchmarks/run.sh display --help
./benchmarks/run.sh display --device /dev/fb1 --seconds 15 --delay-ms 50
./benchmarks/run-display-sweep.sh --device /dev/fb1 --seconds 15 \
  > /tmp/display-baseline.jsonl 2> /tmp/display-baseline.log
```

The default pattern encodes a submission number modulo 65536 in the top 24 rows:
16 black/white cells left to right, **least significant bit first**. Below it are
a large white **six-digit decimal submission number** on a black backdrop,
an alternating dark background and moving white bar. Decimal numbers start at
`000000` for each test and show the full submission number through `999999`.
The decimal overlay scales to fit full-frame and half-frame regions; the 24-row
partial test retains only the binary strip. Very small regions may also omit the
decimal overlay. Start each recording before the test. The binary counter wraps
at 65536; the decimal counter does not wrap within the one-million-frame limit.
The other patterns are
`alternate` (black/white full frames) and `bars` (moving red/green bars).
Counters are not an atomic framebuffer update: mixed cells/tearing are possible.

The sweep runs full-frame delays 110, 50, 33, 16, and 0 milliseconds, then unpaced
320x120 and 320x24 partial writes. Delay is **after** rendering/submission, not an
exact target interval. Each run has its own duration limit, starts its counter at
zero, and returns one JSON line. At most 1,000,000 frames are submitted; the first
of `--seconds` or `--frames` limits terminates the run. There is no warm-up excluded
from the report. Repeat tests at least three times after the device is warm.
SIGINT/SIGTERM stops after the current driver operation; a blocked driver ioctl
or write cannot be guaranteed to exit promptly. Pixels are not restored on exit.

## What the measurements mean

**`submission_fps` is NOT panel refresh FPS.** mmap writes can complete at thousands
of FPS while flexfb's deferred worker only transfers a few frames per second.
Writes may be coalesced or dropped. The report deliberately leaves `panel_fps`
null. `submitted_MB_s` counts bytes copied/submitted, not bytes actually sent on
the wire. The time includes pattern generation, writes, and requested delay;
render/present p50/p95 measure those individual CPU operations (present includes
vsync waiting when requested). JSON also records method, pattern, update size,
delay, requested controls, duration, count, and interruption status. Framebuffer
geometry and flags go to stderr. Keep both output files with your results.

To measure visible throughput, record at 120/240 FPS or faster and count distinct
complete counter images per second; read the large decimal number for easier
manual counting, reject torn/mixed counters and account for binary counter wrap.
Count distinct visible numbers, not the numerical difference between the first
and last: submissions may be skipped. Capture several seconds in the middle of each test. A camera's
rolling shutter, exposure and capture rate can limit the measurement. Alternating
black/white alone aliases badly if frames are skipped, so use the counter for FPS
qualification. For definitive bus throughput, capture write transactions with a
logic analyzer: measure full-frame transfer duration, inter-transfer gaps, clock,
and whether updates are actually partial. Report physical scanout, completed bus
updates and unique visible frames separately where available.

## Driver settings and transport

First record the actual board/driver wiring; **flexfb does not imply SPI**:

```sh
fbset -fb /dev/fb1 -i
readlink -f /sys/class/graphics/fb1/device
ls /sys/module/flexfb/parameters /sys/module/fbtft_device/parameters
cat /sys/module/fbtft_device/parameters/fps
cat /sys/module/fbtft_device/parameters/speed
```

Paths/parameters vary and may not exist. Also record the deployed module-load
command or device-tree node, kernel version, display/controller model, CPU load,
and temperatures. The repository's `tools/display-driver/driver_load.sh` is a
**parallel GPIO** configuration (`buswidth=8`, `wr`, `dc`, `db*` pins), not proof
that the deployed TFT is SPI. On that transport an SPI speed parameter will not
increase the data-bus rate. Verify the deployed setup instead of copying that
script or changing its pins.

There is no generic fbdev ioctl to set SPI speed or deferred-update FPS. If your
kernel exposes a documented **writable, live** numeric control, the benchmark
can change it explicitly, verify readback, and restore the original value:

```sh
# Replace both placeholder paths with verified controls from YOUR driver.
./benchmarks/run-display-sweep.sh --seconds 15 \
  --spi-hz 20000000 --spi-path /sys/PATH/TO/LIVE_SPI_CONTROL \
  --driver-fps 30 --fps-path /sys/PATH/TO/LIVE_FPS_CONTROL \
  > /tmp/display-20mhz-30fps.jsonl 2> /tmp/display-20mhz-30fps.log
```

Both value and path must be supplied together. Only paths resolving under `/sys/`
are accepted. Changes are opt-in; no privileged escalation, module reload, device
rebind, or persistent configuration edit is performed. Unsupported/read-only
controls or mismatched readback fail the run. Normal exits, exceptions and
SIGINT/SIGTERM attempt restoration; restoration failure is reported and returns
nonzero. SIGKILL, power loss, and kernel failure cannot restore controls: record
original settings before testing. A successful readback proves the parameter
value, **not that the driver applied it to existing hardware**. Some module `fps`
parameters only initialize deferred I/O at device registration. Verify behavior
on the wire or screen.

If controls are load-time only, keep the program read-only with respect to them.
An operator must stop users of the device, change the documented module-load or
device-tree setting, reload/reboot as appropriate, confirm effective settings,
and rerun the same sweep. Do not blindly unload shared display drivers or change
SPI speed using spidev on a device already owned by flexfb. Preserve rotation,
GPIO mapping, bus width and all other working parameters. Increase speeds in
small supported steps and stop at corruption, unstable colors, or driver errors;
maximum stable throughput matters more than an advertised clock rate.

## Additional comparisons

```sh
# Compare syscall path with mmap; pwrite issues one write per changed row.
./benchmarks/run-display-sweep.sh --seconds 15 --method pwrite \
  > /tmp/display-pwrite.jsonl 2> /tmp/display-pwrite.log

# Optional only: fail explicitly if the framebuffer lacks the ioctl.
./benchmarks/run.sh display --seconds 15 --delay-ms 0 --vsync

# A small update to test driver dirty-region behavior.
./benchmarks/run.sh display --seconds 15 --region 320x24 --delay-ms 0
```

`--vsync` uses `FBIO_WAITFORVSYNC`; flexfb may not support it. Even success does not
prove every submitted frame was displayed. mmap dirties pages, not exact rectangles;
pwrite may mark more memory dirty than requested. A partial-region test demonstrates
a bandwidth improvement only when panel/bus measurements improve. No `msync` is
used: it is not a generic fbdev flush or display-completion fence.

## Decide the application changes from the results

Maintain a results table with transport, effective clock, effective driver FPS,
method, region, delay, submission FPS, present p95, **measured visible FPS**, and
corruption/tearing observations. Change one driver variable at a time.

| Observation | Likely action |
| --- | --- |
| Stable visible FPS rises as delay falls below 110 ms | Lower Chiton's current 110 ms preview interval; choose a configurable rate at or below the qualified stable panel rate. |
| Submission is very fast but visible FPS plateaus | Investigate driver deferred-update cadence and bus throughput, not memcpy optimization. |
| Raising driver FPS improves visible rate with the same clock | Increase the qualified driver update cadence; avoid CPU-wasting over-submission. |
| Raising effective SPI clock improves visible rate | Qualify a higher clock within controller, wiring and board limits. |
| Partial writes improve actual transfers/visible rate | Implement changed-row/region writes and skip unchanged frames; preserve driver-compatible dirty tracking. |
| Partial writes do not improve actual transfers | Driver sends full frames or dirty pages cover too much; investigate driver behavior before application dirty rectangles. |
| Rendering or present latency dominates | Profile CPU rendering and framebuffer copy/write path; compare mmap and pwrite under identical conditions. |
| Higher rates cause corruption or tearing | Back off to the highest repeatably stable setting; consider synchronization/double buffering only if supported by the driver. |

For **SPI**, 320x240 RGB565 requires 1,228,800 pixel bits per full frame. The
ideal upper bound at 16 MHz is 13.0 FPS, 20 MHz is 16.3 FPS, and 40 MHz is 32.6 FPS;
command overhead, scheduling and gaps lower it. 30 FPS needs at least 36.864 Mbit/s
before overhead. These calculations do not apply directly to a parallel bus.
The benchmark does not automatically modify Chiton's pacing or claim a maximum
panel refresh rate without physical measurement.

For slow transfers on the Variscite i.MX8 kernel, see
[SPI driver diagnostics](spi-display-diagnostics.md) for an opt-in kernel patch
that separates DMA/PIO selection, clock selection, preparation, waits and gaps.
This kernel diagnostic is separate from the application cross-build.

Without rebuilding the kernel, the [direct SPI benchmark](direct-spi-benchmark.md)
can compare spidev transfer word sizes and chunking after an operator temporarily
detaches flexfb. It is included in the application cross-build but never run automatically.
