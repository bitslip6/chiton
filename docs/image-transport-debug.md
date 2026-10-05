# Debugging image transport on the i.MX8 target

## Likely UI failure path

The full UI shared `CHITON_INTERVAL` between image and measurement reads. Its
adaptive image-FPS controller reduces that delay when image FPS is low; at zero
FPS it reaches **5 ms**. Measurement polling could therefore accelerate exactly
when the image path was failing. It now uses an independent **1000 ms** delay
after completion, with one in-flight request and failure backoff preserved.
This changes observation cadence, not the device's capture mode or rate.

Requests to `/get/value/683/853` also indicate an older deployed/cached page:
the current UI uses `/get/value/client/<tab-id>/...`. These coordinates are
invalid for both 640x480 sensor images and 1500x165 planar images, so the current
server rejects them with 400. A legacy immediate retry loop can flood the
single-threaded HTTP dispatcher with fast errors. Valid measurement requests
also share camera ownership with acquisition/snapshot construction.

This can explain load/starvation or a latched legacy image gate, but does **not**
prove JPEG encoding or transport is broken. An active camera does not by itself
prove a processed frame/JPEG was published. Cloud mode (4) intentionally has no
JPEG and yields 409; unavailable/unpublished JPEG snapshots yield 503.

## Reduced diagnostic page

`release/files/debug.html` (mirrored to `bin/files/debug.html`) is independent of
the full UI. It has no external dependencies, point/temperature requests,
charts, cloud renderer, adaptive FPS controller, or automatic settings changes.
No requests are made on startup.

Controls:

- **Fetch JPEG once:** one `/image/camera` request, reporting status, content
  type/length, frame headers, actual received byte count, first 16 bytes, JPEG
  start marker, native image decoding and decoded dimensions.
- **Read frame metadata:** `/frame/camera` once.
- **Read device mode:** `/colormode/setting` once.
- **Explicitly select greyscale:** `/colormode/setting/1` once. This is an
  intentional global device-mode mutation affecting every UI, never retried.
- Optional JPEG polling waits **2 seconds after completion**, with a single
  in-flight operation and a 10-second deadline through body read/image decoding.
  Hidden/navigation-away pages cancel requests and release image URLs.

The diagnostic does not require metadata headers to render a JPEG; absent
headers are reported, not silently used to block a valid image. This helps
separate JPEG bytes from newer-client/older-server metadata mismatches.

## Deploy just the diagnostic asset

No binary rebuild or camera restart is required for this UI change. From AMD,
substitute the device hostname/IP for `TARGET`:

```sh
scp /home/cory/Work/chiton/release/files/debug.html \
  root@TARGET:/usr/local/chiton/current/files/debug.html
```

If `CHITON_ASSET_DIR` overrides the normal installed asset location, use that
directory instead. **Close all full-UI tabs** before visiting:

```text
http://TARGET/debug.html?image-debug=1
```

Use the actual HTTP port (8080 if the startup log reported fallback). Confirm
`Diagnostic UI build: image-debug-1`; the new filename/query avoids mistaking a
cached factory page for this diagnostic. Leave polling unchecked initially.
Read mode, then fetch JPEG once. Only explicitly choose greyscale if you intend
to leave cloud/current device mode.

To apply the full UI's slower measurement cadence, back up and then replace the
installed `files/app.html` with `release/files/app.html`, then reload without
cache. The repository's root `app.html` files are symlinks to those files; the
installed asset resolver prefers `files/app.html` over a root-level copy.

## Interpret the report

| Result | Next investigation |
|---|---|
| 409 / mode 4 | JPEG intentionally unavailable; explicitly select an image mode if desired. |
| 503 / frame unavailable | Capture publication, current mode, JPEG generation; read frame metadata next. |
| 504 | A requested newer frame/ownership deadline expired; inspect capture liveness. |
| No HTTP headers before timeout | Server not listening on this port, blocked dispatcher/camera ownership, or transport issue. |
| Headers arrive, body stalls/short read | HTTP framing, response length, socket transfer. |
| 200 but not `image/jpeg` / missing `ff d8` | Wrong route/server, unexpected payload, encoder/output buffer. |
| JPEG bytes arrive but decoding fails | Truncated/corrupted JPEG or encoding issue. |
| JPEG decodes, full UI remains blank | Full-UI asset version, metadata validation, display/mode/canvas logic. |
| Valid but black JPEG | Pixel content/calibration/illumination/rendering rather than HTTP transfer. |

Send the complete diagnostic report and relevant server log. Avoid changing
camera registers, calibration or power settings while isolating transport.

## Regression coverage

`tests/web_streams.js` proves point polling remains one request per second even
with a 5 ms image interval. `tests/web_image_debug.js` checks manual-only startup,
one in-flight request, slow polling, timeouts, invalid JPEGs, error bodies,
explicit-only mode changes and URL cleanup. The Chromium mock-sensor suite also
checks actual JPEG decoding and diagnostic failure reports, with external
traffic blocked. These tests do not execute the target camera or establish
hardware image correctness.
