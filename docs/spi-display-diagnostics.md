# Opt-in i.MX8 SPI display instrumentation (Variscite Linux 4.14.98)

## Purpose and limits

`linux/files/spi-imx-display-diagnostics.patch` instruments `spi-imx.c` without
changing DMA eligibility, SPI mode, speed requests, controller burst lengths,
DMA descriptors, timeout policy, or framebuffer behavior. It defaults **off**.
No target changes or hardware execution occur when creating/testing this patch.

The 32 KiB buffer did not improve the observed ~1310 KiB/s driver throughput.
This patch determines whether pixel transfers actually use DMA or PIO, the
clock selected by the driver's divider, and whether time accumulates in DMA
preparation, TX completion waiting, RX completion waiting, or gaps between chunks.
It does not assume DMA is faulty. SPI statistics showing `spi_sync` do not tell
us whether the underlying operation uses DMA.

The source reference is the recipe in `linux/linux-variscite_4.14.98.bb`:

- repository: `https://github.com/varigit/linux-imx`
- revision: `a9b5dcb9ef09f9140ce91f9d09a6ba2ceead8928`
- original file: `drivers/spi/spi-imx.c`
- SHA256: `9a4d63562483b007bfff353bd58fc048bfae0a238cf54be38c2accc9e50739a3`

The deployed kernel may contain additional patches. Use its **actual** source,
configuration, compiler and build environment. Do not replace the whole driver
with the reference file or force a patch through mismatches. The repository's
`jabil_defconfig` has `CONFIG_SPI_IMX=y`, so this normally requires a **kernel
image rebuild**, not just installing a `.ko`.

## Build in the existing kernel workflow

`cross-build.sh` builds an application bundle, **not a kernel**. It does not
build, install, or activate these diagnostics. In the original configured kernel
source tree, first inspect and validate the patch:

```sh
git apply --check /path/to/chiton/linux/files/spi-imx-display-diagnostics.patch
git apply /path/to/chiton/linux/files/spi-imx-display-diagnostics.patch
```

For a reproducible Yocto build, alternatively copy the patch into your actual
kernel recipe's `files` search path and append this to its existing `.bbappend`:

```bitbake
SRC_URI += "file://spi-imx-display-diagnostics.patch"
```

Then build `virtual/kernel` with the existing configuration. Do not both apply
manually and apply through the recipe. Do not substitute the application's
recovered userspace sysroot for a configured kernel build tree.

Keep the known-good kernel/boot files, retain a recovery boot route, and deploy
through the board's established kernel-update procedure. Build matching modules
where required. The patch should remain off until explicitly enabled after boot.
No bootloader, device-tree DMA mapping, or hardware interface change is required.

Validation in this checkout covers patch application to the pinned source,
mocked C compilation/execution of the actual diagnostic helpers, transfer wrapper
and DMA timing function, parser tests, and host ASan/UBSan checks. **It is not a
real kernel compilation or DMA/hardware validation.** Perform those in the
original BSP environment before deployment.

## Enable for only the display

Stop Chiton and other framebuffer writers. Initialize the panel once, preserving
its working settings: 48 MHz, 32 KiB buffer, existing GPIOs and initialization.
Do not enable verbose per-transfer/SDMA logging. Remove timing debug flags when
running the primary low-overhead trace; keep a separate baseline with the same
configuration. A built-in `spi_imx` driver is not safe to unload/rebind casually:
it can also serve the touchscreen or other devices.

After booting the diagnostic kernel:

```sh
P=/sys/module/spi_imx/parameters
ls "$P"/diag_*
cat "$P/diag_bus"       # Expected initial value: -1 (disabled)

# As root; only while no diagnostic benchmark is running:
echo -1 > "$P/diag_bus"
echo 0 > "$P/diag_cs"
echo 1 > "$P/diag_epoch" # Use a new value for EACH separate capture.
echo 2 > "$P/diag_bus"   # spi2.0 only
```

If the new files are absent, verify you booted the rebuilt image and inspect
`/sys/module` for the driver name. These parameters use mode 0644 deliberately,
unlike the existing mode-0000 parameters that did not appear in sysfs.

Record dmesg continuously during a short test, because a small kernel log ring
can wrap. If your dmesg supports `-w`, one example is:

```sh
dmesg -w > /tmp/spi-diagnostic.log &
LOGGER=$!
./benchmarks/run.sh display --seconds 10 --method mmap \
  --region 320x240 --pattern alternate --delay-ms 25 \
  > /tmp/spi-diagnostic-benchmark.json 2> /tmp/spi-diagnostic-benchmark.log
echo -1 > /sys/module/spi_imx/parameters/diag_bus
kill "$LOGGER"
wait "$LOGGER" 2>/dev/null || :
```

Use your device's equivalent kernel-log capture if `dmesg -w` is unavailable.
Disable diagnostics manually on interruption or errors. Do not use `dmesg -c` to
clear unrelated logs. Counters reset on the next selected setup/transfer after
`diag_epoch` changes. Disabling/re-enabling alone does not reset them; change the
epoch while idle. Separate captures must not share an epoch.

Reports are cumulative since the first selected operation in that epoch, not
per-frame snapshots. They are emitted at most once per second, at transfer
completion. There is no idle timer or automatic final flush: the last fraction
of a second may not appear in a log. Compare logged spans rather than assume
reported bytes exactly equal an entire benchmark run.

## Analyze on the host

Copy the log and benchmark JSON back without altering the release or config:

```sh
python3 tools/display-driver/analyze_spi_diagnostics.py spi-diagnostic.log
```

The analyzer emits one JSON summary for the last complete two-line report in
each bus/chip-select/epoch group. It retains raw counters and adds effective
payload MB/s, average payload transfer time, DMA phase averages, and estimated
full-frame payload time. It never reports physical panel FPS.

### Main report (`spi-diag`)

- `n`, `dma_n`, `pio_n`: attempted transfer counts and selected paths. Small
  command transfers normally use PIO; this alone is not evidence of DMA failure.
- `payload_n`, `payload_dma_n`, `payload_pio_n`: transfers >=1024 bytes, used to
  separate pixel-data-sized transfers from display commands. Size is a heuristic,
  not knowledge of the display protocol.
- `bytes`, `payload_bytes`: requested byte counts, including failed attempts if
  `errors` is nonzero. Qualify performance with zero errors.
- `xfer_us`, `dma_us`, `pio_us`, `payload_us`: total duration of the driver transfer
  callback, including RX FIFO flush, setup within that callback and completion
  waits. These are elapsed software times, not electrical wire measurements.
- `setup_us`: total separate controller `setup_transfer` time. This is outside
  `xfer_us` but can also fall inside the gap metric: do not add every field together.
- `ideal_payload_us`: theoretical pixel-sized transfer wire time calculated
  from byte count and the driver's selected clock. Excludes overhead and gaps.
- `payload_gap_us`, `payload_gap_max_us`: gaps between consecutive >=1024-byte
  transfer callbacks. Address commands break the chain, so normal inter-frame
  delays are usually excluded. These gaps can include buffer conversion, SPI core
  work, scheduling, setup, and intentional caller pauses. They are **not** a pure
  measurement of SPI hardware idle time. Our own report-printing time is excluded
  from the next gap, but logging still perturbs scheduling.

### Detail report (`spi-diag-detail`)

- `source_hz`: controller clock cached by the driver at probe.
- `req_min/max`, `clk_min/max`: requested and driver-selected pixel-sized transfer
  clocks. Divider-derived selected clock is not an oscilloscope measurement.
- `bpw`, `burst_bits`: last pixel-sized transfer's software word size and eCSPI
  CTRL burst-length field. An 8-bit hardware burst is an observation, **not**
  permission to change burst configuration: preserve applicable chip errata.
- `len_min/max`: attempted transfer size range, including commands.
- `wml_min/max`: DMA watermark chosen by the existing driver.
- `tx_sg_max`, `rx_sg_max`: maximum DMA scatterlist entry count per attempted DMA
  transfer. This is distinct from SPI-core transfer splitting and internal SDMA
  bursts. Missing values (zero) can indicate no DMA attempts.
- `dma_prepare_us`: DMA function entry through issuing descriptors and triggering
  hardware, up to immediately before waiting for TX completion. Early preparation
  failures contribute to `errors` but not this phase total.
- `tx_wait_us`: elapsed wait for TX completion.
- `rx_wait_us`: subsequent elapsed wait for RX completion (not total RX duration;
  RX runs concurrently with TX).
- `dma_tx_chan`, `dma_rx_chan`: whether channels are allocated at report time.

DMA can already be transferring before the preparation phase ends. Completion
waits include scheduler wakeup latency; a long wait alone does **not** prove slow
hardware or bad DMA firmware. The patch deliberately avoids hot interrupt/SDMA
callback logging. A later callback-timestamp refinement may be needed if
completion-versus-wakeup latency is the remaining uncertainty.

## Interpret and choose the next change

| Observation | Next investigation |
| --- | --- |
| `payload_pio_n` dominates | Check DMA channel allocation, eligibility, firmware and controller match; do not infer this from touchscreen PIO warnings. |
| Selected clock is much lower than requested | Inspect the controller clock parent/divider using the actual BSP's supported clock configuration. |
| DMA selected; TX wait dominates; payload time far exceeds theoretical wire time | Investigate eCSPI/SDMA burst behavior, firmware/errata and completion scheduling; don't increase FPS as a fix. |
| RX tail wait dominates | Investigate RX draining, watermark and completion/descriptor behavior. |
| Preparation dominates | Inspect descriptor preparation, DMA configuration and mappings. |
| Consecutive payload gaps dominate | Profile conversion, SPI core setup and scheduling between chunks. |
| SPI transfer durations are short but fbtft updates/mmap writes are slow | Return to fbtft conversion, deferred-I/O locks and scheduling; DMA is not established as the culprit. |

Keep an uninstrumented baseline. Repeat counter and alternate-pattern captures
with distinct epochs, fixed buffer/clock settings and no competing framebuffer
writer. Do not remove errata workarounds or change DMA request identifiers based
only on these counters.

## Host tests

Parser tests require only Python. For reference patch application and mocked C
execution, obtain the original driver from the pinned revision and run:

```sh
SPI_IMX_SOURCE=/path/to/original/spi-imx.c python3 tests/test_spi_diagnostics.py
```

The reference hash is checked before applying anything to a temporary tree. This
test neither changes an actual kernel tree nor loads a module.
