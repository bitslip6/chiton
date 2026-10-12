# Persistent browser JPEG streaming

## Operator use / rollback

Use the complete versioned bundle, not a binary-only update. Preserve the
working `rel_cross_bundle_20261005_v2` release and shared device state. Stop the
old camera owner before manually testing the new release:

```sh
/usr/local/chiton/rel_stream_20261005/run.sh --display headless
```

Open the new UI on the port printed by that process, with other old tabs closed.
Reload without cache. It attempts WebSocket streaming automatically; target
rendered FPS defaults to 30. Global device mode is still authoritative; neither
the stream nor UI automatically selects greyscale/cloud or changes capture.
Cloud mode 4 continues using the existing point-cloud API, not JPEG streaming.

For a transport-only rollback use:

```text
http://TARGET/app.html?transport=poll
```

For a server-side stream kill switch set `CHITON_DISABLE_IMAGE_STREAM=1` before
starting the release. Full application rollback is stopping the new process and
running/restoring the previous versioned release. Shared configuration/calibration
should also be backed up; code rollback does not undo state changes.

## Pipeline

The camera thread acquires and processes one frame while holding camera ownership.
It publishes processed data/measurement metadata coherently, snapshots JPEG
quality, and releases ownership **before JPEG encoding**. The intensity/RGB
encoding buffer is private to that thread. The encoder never reads mutable
quality settings off-lock.

`JpegHub` owns the encoder's malloc buffer directly through a shared pointer.
There is no per-frame encoded-buffer copy to publish, no JPEG copy per stream
viewer, and no raw camera pointer escapes ownership. A short separate mutex
publishes an immutable JPEG + its own frame metadata/dimensions atomically.
Readers pin that buffer while newer frames replace the hub's latest pointer.
Raw/measurement publication can precede JPEG publication by the encoding duration;
each representation retains the metadata of its actual source frame.

`/image/camera` and its `after`/timeout queries now read immutable JPEG snapshots
without camera ownership. Their HTTP status/metadata contracts remain: capture
disabled/unavailable is 503, cloud mode is 409, and a newer-image deadline is 504.
Exports remain observational and cloud/OBJ export still requires explicit cloud
mode selection.

The HTTP listener hands validated upgrades to one dedicated reactor. Network
writes/credit waits never hold camera or hub ownership. Regular HTTP routes remain
serial; a large/slow HTTP request cannot occupy the WebSocket reactor, though
camera-owning mutations/exports can still affect acquisition.

Bounds:

- Four stream viewers maximum; excess upgrades receive 503.
- Maximum JPEG 2 MiB, metadata 4096 bytes, client receive buffer 512 bytes.
- One unacknowledged JPEG per viewer; no historical frame queue.
- Slow sends expire after 250 ms, unreturned frame credit after 2000 ms.
- Masked client control/credit frames only, at most 125 payload bytes and 256
  control frames/sec; fragmented, binary, oversized or invalid credit input
  disconnects the client. Ping/pong and normal close are supported.
- Socket send buffers are bounded; TCP_NODELAY avoids small-header/ACK latency.
- Shutdown joins the reactor and closes all accepted/pending stream descriptors.

As frames arrive, the reactor selects the newest compatible JPEG. Slow viewers
skip intermediate sensor frames instead of holding up capture or accumulating
latency. Four in-flight buffers plus the latest snapshot have bounded storage.

## Wire protocol

Endpoint: `GET /stream/camera HTTP/1.1`, RFC6455 version 13, subprotocol
`chiton.jpeg.v1`. Browser upgrades must have an Origin matching the request Host
(http or https); non-browser clients may omit Origin. The handshake uses the
standard SHA-1/base64 accept calculation without adding a new crypto/runtime
library. This is not authentication. Use a trusted network or a properly
configured TLS/authentication reverse proxy for remote access.

A binary WebSocket message contains:

```text
4 bytes: unsigned big-endian JSON metadata length
N bytes: UTF-8 JSON metadata
remaining bytes: complete JPEG
```

Metadata contains version 1, frame_id, mode, width, height, age_ms,
captured_at_ms, configuration_revision, source_units and timestamp_kind.
Timestamps describe host receipt, **not** physical exposure. The browser returns
one masked text message containing the decimal `frame_id` after successful native
decoding. The server then selects the latest frame for that viewer.

When waiting/disabled/in cloud mode, bounded periodic text status messages report
the global mode, capture_enabled and pipeline measurements. They never mutate
camera settings. Extensions/compression are not negotiated; JPEG is already
compressed and is sent as binary, not base64.

## Browser behavior

`image-stream.js` validates the envelope, JPEG marker and metadata, uses
`createImageBitmap` when available (Image/object-URL fallback otherwise), and
verifies decoded dimensions. Only one decode is in flight and one decoded bitmap
waits for requestAnimationFrame; replacement drops/closes the older bitmap.
Returned credit follows decode rather than paint so RAF scheduling does not
create a network queue. The target FPS limits painting; it does not change the
sensor's capture rate. Canvas dimensions are updated only when layout changes,
not reset/reallocated for every frame.

Hidden/navigation-away pages cancel HTTP work, close the socket and dispose
pending images. Native bitmap decoding cannot be forcibly cancelled; obsolete
results are closed on completion and reconnect cannot stack more decoders while
one remains outstanding. Decoder/transport errors switch to bounded snapshot
polling and retry streaming after a 10-second cooldown. An eight-second stream
watchdog handles missing traffic/decoding stalls. Snapshot failure backoff and
one-in-flight request rules are retained. The old FPS feedback loop that pushed
polling toward 5 ms on image failure is removed; fallback polling budgets the
frame interval from request start, not an extra interval after completion.

## Measurements

`GET /stream/metrics` is an independent, no-store JSON read. It reports:

- Total processed acquisitions and JPEG publications; rolling acquisition/JPEG
  FPS (two-second window, 120 samples maximum).
- Mean requestFrame/getData, processing, and JPEG encoding milliseconds.
- Mean JPEG bytes, last publication age, active/max clients.
- Sent/acknowledged JPEGs, skipped sensor-frame IDs, slow disconnects.
- Total JPEG bytes sent, last send and decode-credit turnaround milliseconds.
- Bounded mean/p95/p99 summaries for main image processing, calibration/smoothing,
  preview copy and mode conversion; register-I/O mean/count without changing cadence.
- Automatically sampled process/camera/per-core CPU, available temperature,
  frequency/cooling observations and collection cost. Unknown throttling is null.

Both browser copies read detailed diagnostics once/second independently of image
transport and display missing/stale values explicitly. See
[pipeline-performance-metrics.md](pipeline-performance-metrics.md) for timing
boundaries, histogram approximation, overhead limitations and SDK-free benchmarks.

The normal UI reports pipeline counters/timings from stream status and client
received/decoded/displayed/dropped counts plus decode time. The existing FPS
counter measures actual draw calls. Client streaming statistics are also available
as `window.CHITON_IMAGE_TRANSPORT.stats`. Pipeline acquisition FPS counts the
application's completed/processed acquisitions, not frames the physical sensor
produced but the application never fetched. Skipped sensor IDs can include
non-JPEG modes, so that counter is not solely JPEG loss.

To diagnose a target below 30 FPS: first compare acquisition/JPEG/display FPS;
then processing/encode time, bytes and send/credit turnaround. At 50 KiB/frame,
30 FPS needs about 12.3 Mbit/s before overhead. Select a lower JPEG quality
explicitly if bandwidth is limiting. If processing/encoding cannot keep up,
further codec/algorithm tuning is needed; the transport cannot manufacture frames.
No new libjpeg-turbo, hardware codec, resolution/illumination/register change or
replacement OS library is silently introduced.

## Validation / limits

Native simulator tests cover RFC handshake/rejection, four-viewer admission,
credit, binary JPEG metadata, ping/pong, non-ACKing viewers, global/cloud mode
changes and continued ordinary HTTP service. Focused C++ tests cover immutable
buffer lifetimes, concurrent hub readers/publication and reactor shutdown; run
under ASan, UBSan and TSan. JS tests cover bounded decode/render queues,
obsolete-image disposal, pause/invalidation, watchdog and fallback. Real Chromium
tests exercise native JPEG decoding over a credit-controlled localhost mock
stream and both shipped UI copies, with external traffic blocked.

Both Chromium copies measured **30.0 displayed FPS** in the local mock test.
That is not a measurement of the i.MX8 camera, CPU or network. Hardware 30-FPS
qualification remains the operator's test, and the stream does not resolve the
previous unsupported-framebuffer issue.
