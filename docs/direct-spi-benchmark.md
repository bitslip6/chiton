# Direct ST7789VI SPI display benchmark

`spi_display_benchmark` bypasses flexfb/deferred framebuffer I/O and submits pixel
transfers directly through **spidev**. It uses the existing i.MX8 SPI controller
and its existing DMA/PIO decisions; it cannot force DMA or fix SDMA firmware.
No kernel or device-tree rebuild is needed if this kernel already supports
spidev and runtime SPI device registration. The captured boot log shows spidev
on `spi2.0` before flexfb replaces it.

`cross-build.sh` automatically builds/audits/packages the program under
`benchmarks/spi_display_benchmark`. The launcher is:

```sh
./benchmarks/run.sh spi --help
```

Builds never run ARM code, unbind drivers, change GPIOs, or deploy anything. The
ordinary pipeline `run-all.sh` does NOT run this hardware benchmark.

## Safety and temporary device ownership

**Do not run while flexfb owns the panel.** Stop Chiton and any service/script
that can initialize the panel or write its framebuffer, including the duplicate
startup loader. Arrange console access and a known-good restoration procedure.
Only touch the display device; do not unload/rebind the whole SPI controller or
SDMA, which may also serve other devices.

The program requires `--ack-exclusive` and an explicit `--dc-gpio`. It never
unloads modules, rebinds devices, edits DT or persistent configuration, or calls
sudo. It takes an advisory file lock, but non-cooperating writers are NOT blocked:
the operator must ensure exclusive access. Verify bus, CS, GPIOs and init profile
against the **deployed working loader**, not just an example.

For this repository's legacy `fbtft_device` driver, the matching source supports
`custom name=spidev` registration. The following is an **operator-only example**,
not an automatically executed setup script:

```sh
# After stopping Chiton and preventing automatic display reinitialization:
rmmod fbtft_device          # Unregister its display device; fails if in use.
modprobe spidev
modprobe fbtft_device custom name=spidev busnum=2 cs=0 speed=48000000 mode=3
ls -l /dev/spidev2.0
readlink -f /sys/bus/spi/devices/spi2.0/driver
```

Stop if a command fails or the device does not bind to spidev. Do not blindly
change the device tree, bind another SPI peripheral, or copy GPIO mappings from
the unrelated parallel loader. `flexfb` may remain loaded but must have no device
bound to this panel. Other `fbtft_device`-created displays, if any, would also be
affected by removing that module: confirm its scope before doing so.

After testing, close the benchmark, unregister the temporary device by removing
`fbtft_device`, then run the **device's existing working TFT loader once**. Confirm
its usual framebuffer/FPS/buffer/SPI settings in dmesg before restarting Chiton.
Do not install the temporary spidev registration in startup configuration.

## GPIO and panel initialization

The repository's SPI loader uses Linux global GPIO **118 for DC** and **121 for
reset**. These are not header pin numbers. Verify the deployed loader uses them.
The program exports only currently unexported GPIOs and refuses to take over
existing exports. A kernel-owned line fails export. It sets the initial output
high atomically and holds value file descriptors for the run. Normal exits,
exceptions and SIGINT/SIGTERM attempt to unexport its own lines and restore the
original spidev mode/speed/default word-size/LSB settings. Cleanup failure returns
nonzero. SIGKILL, power loss and kernel failures cannot guarantee cleanup; pin
state after unexport is not guaranteed, and the image/register contents are NOT
restored. Re-run the working loader after testing. Blocked SPI ioctls may delay
signal handling; a current frame is finished before stopping.

For a reproducible first test, explicitly reset and initialize the panel:

```sh
./benchmarks/run.sh spi --ack-exclusive --dc-gpio 118 \
  --init-panel --reset-gpio 121 --word-bits 8 --chunk-bytes 4096 \
  --speed-hz 48000000 --seconds 15 --delay-ms 25 \
  > /tmp/spi8.json 2> /tmp/spi8.log
```

`--init-panel` uses the exact command/gamma/power profile in the repository's
`release/tft_device.sh`. It is not a universal ST7789 module profile: verify that
it is appropriate before using it. Initialization is outside measurement. The
program always sets RGB565 and the repository's landscape MADCTL value (`A2h`).
Without `--init-panel`, the operator must preserve a previously initialized,
awake panel; the program does not send sleep-out/display-on or reset in that mode.

The reset GPIO is held for the entire run when initialization is requested. If
unexporting reset changes its electrical state on your board, use initialization
on each subsequent comparison as well. Otherwise subsequent runs can omit both
`--init-panel` and `--reset-gpio`.

## Word-size comparison

Repeat the same test with:

```text
--word-bits 8
--word-bits 16
--word-bits 32
```

Commands and command parameters **always use 8-bit words**. Only RGB565 pixel data
uses the selected size. The program packs native-endian 16/32-bit host words so
MSB-first SPI produces the same RGB565 byte stream in all modes. This is not a
panel interface change. The requested word size may be rejected by a particular
kernel/controller; rejection or a short transfer fails the run instead of
silently falling back. Successful ioctl completion is not proof of correct
colors: inspect the counter and `--pattern bars` (red/green) for word-order errors,
then qualify stability and tearing. The host packing tests verify the intended
wire sequence, not the hardware's electrical output.

Use the same clock, chunk size, delay and pattern for each comparison. Default
`counter` uses the same large decimal counter, binary strip, blue/black background
and moving white bar as the framebuffer benchmark. `alternate` is black/white;
`bars` is red/green. No optional assets or camera SDK are required.

## Chunk-size comparison and spidev limits

Pixel data uses one synchronous `SPI_IOC_MESSAGE(1)` per chunk; CS can deassert
between chunks, with DC remaining high. Each frame starts fresh address-window
commands and RAMWR. This relies on the controller's supported paused serial RAM
write behavior. Default chunks are 4096 bytes. Try 16 KiB, 32 KiB and 150 KiB only
if spidev's buffer limit permits:

```sh
cat /sys/module/spidev/parameters/bufsiz
```

If readable, the program rejects a larger chunk before claiming GPIOs. Otherwise
the ioctl can reject it with `EMSGSIZE`. The final chunk is shorter as needed;
all chunks remain aligned to the SPI word size. `--chunk-bytes` must be a multiple
of word bytes, from 4 through 153600.

If spidev is a module and **no other device/application depends on it**, an
operator may explicitly reload it with a larger `bufsiz` before registering the
test device, for example `modprobe spidev bufsiz=153600`. An already-loaded module
will NOT apply a new value. Do not unload a shared spidev module just for this
test; if built in, use its existing limit. Allocation may fail at large sizes.

Spidev does not eliminate internal DMA descriptor splitting. In the referenced
SPI driver, the maximum SDMA segment size is 32 KiB, even if a single ioctl sends
150 KiB. The test distinguishes ioctl/message overhead from controller behavior,
not necessarily every internal DMA burst.

## Clock comparison

With fixed word/chunk settings, compare requested 16, 24, 32 and 48 MHz first:

```text
--speed-hz 16000000
--speed-hz 24000000
--speed-hz 32000000
--speed-hz 48000000
```

The ST7789VI write-timing-derived ceiling is 62.5 MHz, but that is not a guarantee
for the assembled module/wiring/host controller. Do not exceed the board's stable
qualified rate. The program accepts at most 62.5 MHz, does not measure the wire
clock, and never performs read transactions that would need slower timing.

## Output and interpretation

Each successful run ends with a JSON line:

- `completed_frame_fps`: complete synchronous SPI submissions per wall-clock
  second, including rendering, packing, commands and requested delay. NOT physical
  panel FPS; `panel_fps` and `selected_wire_hz` deliberately remain null.
- `pixel_transfer_MB_s`: requested pixel bytes divided by measured pixel-loop
  duration. Includes ioctls, setup, DMA/PIO waits and inter-chunk overhead, but
  excludes render/packing/window commands/delay. Decimal MB/s; fbtft's diagnostic
  throughput is effectively KiB/s, so convert units before comparing.
- `pixel_transfer_p50_ms`, `pixel_transfer_p95_ms`: full-frame pixel-loop latency.
- `render_p50_ms`, `pack_p50_ms`, `command_p50_ms`: individual stage timings.
- Requested speed, mode, word size, chunk size, transfers per frame, init status,
  completed frame count, duration, delay and interruption status are retained.

A limit of 10 seconds / 1,000,000 frames is the default; the first limit reached
ends the run, with the current synchronous frame completed. No warm-up is excluded.
The display may scan while a frame is being transmitted; direct SPI does not
provide atomic replacement or TE synchronization. Count distinct complete
visible decimal numbers in high-speed video for actual visible FPS; do not
subtract first/last numbers when submissions are skipped.

| Result | Interpretation / next action |
| --- | --- |
| Direct 8-bit SPI is much faster than flexfb | Investigate fbtft conversion/deferred-I/O contention and consider a carefully qualified direct-SPI application backend. |
| Direct 8-bit SPI remains ~115 ms per frame | Slowdown is below the framebuffer layer; focus on the SPI controller/DMA path. |
| 16/32-bit payload words materially improve throughput | Wider-word DMA/burst handling is a software optimization candidate; qualify colors, ordering and stability before adding a backend/driver change. |
| Bigger chunks improve throughput | Reduce syscall/message/setup overhead within supported limits. |
| Throughput scales strongly with requested clock | Selected clock/wire bandwidth contributes; actual selected frequency remains unknown without additional measurement. |
| Throughput barely changes with clock | Controller pacing, DMA behavior or software overhead likely dominates. |

Do not infer DMA use solely from a fast/slow result. This program uses the same
kernel SPI path and cannot toggle DMA directly. Run an unlogged baseline, stop
competing workloads, retain stdout/stderr, and repeat each condition three times.

## Host tests

```sh
python3 tests/test_spi_display_benchmark.py
python3 tests/test_display_benchmark.py
```

Both self-tests avoid hardware. CLI tests use `/dev/null` only to verify failure
before GPIO access. Tests cover ownership acknowledgement, initialization guards,
word/chunk validation, native-word packing, shared counter rendering, and matching
the repository init sequence. On-target ownership, GPIO behavior, controller word
support and visible output still require hardware qualification.
