# Local depth colors, near/far sizing and stronger EDL

These are **renderer-only** controls under **Display controls → 3D → Point
appearance**. They do not change classification, calibration, measurements,
acquisition, original XYZD, clipping, raw/binary/OBJ exports or device settings.
They apply to viewport PNGs. Settings survive renderer recreation and mode/context
retry within this page, but are not persisted across reloads.

## Colors

**Reverse colors · red near / blue far** is an optional checkbox, off by default.
It maps the acknowledged depth-color range's minimum to red, midpoint to green,
maximum to blue, with linear RGB interpolation. Values outside the range clamp to
those endpoint colors; they are not removed. The legend reverses with the points,
including while paused or using a frozen capture. No usable range still means
neutral gray. Cleanup's magenta Highlight overrides either depth palette.

The existing palette is unchanged when reversal is off (including its historical
black endpoints). Reversal uses the visible red-to-blue segment rather than
retaining black near/far endpoints. Colors always describe returned **D**, not
distance to the orbiting browser camera or proof of sensor validity.

## Sizing

- **Constant screen size**: unchanged default, circle / 2.5 CSS px.
- **Viewer perspective · near larger**: the existing inverse eye-depth calculation,
  `referenceDistance / viewerEyeDepth`. This was already nearer-viewer = larger;
  it has not been incorrectly inverted to make distant points larger.
- **Sensor depth · near larger**: new mode, `referenceDepth / returnedD`. Use this
  if near/far should follow the depth colors instead of the current viewing camera.
  Orbiting, zooming or changing projection does not reverse this size ordering.

Both scaled modes clamp the multiplier to **0.5–2×** before effective-DPR and
hardware point-size clamps. Diameter stays 1–10 CSS px. Sensor-depth reference is
the midpoint of positive minimum/maximum D captured on first nonempty frame or
explicit Fit/Reset, independent of the color range. Ordinary frames do not
recapture it (no size pumping). Nonpositive D receives neutral 1× size, not an
infinite sprite or a new validity exclusion. If there are no positive readings,
reference is 1. A genuine source/units/revision context reset recaptures the reference
through the existing first-fit behavior. Positive outliers can affect the reference;
this is a display scale, not a calibrated physical point diameter.

## Eye-dome lighting

EDL remains **off by default** and optional on supported WebGL1/2 contexts.
Its strength is now adjustable **0–8**, default **4** (previously fixed at 2).
Thus enabling it gives twice the former exponent strength. Strength 0 gives
unshaded colors; turn EDL off to avoid its second render pass entirely. The existing
eight depth samples / approximately 1.5 CSS-pixel radius and all size/resource
bounds are unchanged. Larger strength increases edge/silhouette darkening without
changing geometry or clear background. Unsupported/failed EDL still falls back to
plain points; the strength slider is disabled when unavailable.

Reset appearance restores default point style, original palette and strength 4;
it does not change the EDL enable toggle, camera pose, clipping or cleanup policy.
PNG provenance records palette direction, sizing basis/reference and EDL strength.
Local changes update uniforms/DOM only, with no new per-point color array, point
buffer upload, extra animation loop or sensor-side work. Enabling EDL still adds
the existing color/depth target and fullscreen pass, not additional sample passes.

For the live scene, a starting visual setup is: reverse colors enabled, sensor-depth
sizing, circles 2.5–3 CSS px, EDL on / strength 4. Increase strength toward 6–8 only
if useful: darkening and larger sprites can conceal information and cost client GPU
work. They do not improve measured depth accuracy or establish target 20 FPS.

Validation/evidence: `_build/renderer-depth-style/phase-20261026-154157/`.
Actual WebGL1/2 tests exercise reversed signed-depth/endpoint colors, near/far
sizes and viewer-distance independence, neutral nonpositive D, hardware/DPR clamps,
no geometry upload for palette/style/strength updates, EDL monotonic strength/zero
parity, clipping/cleanup composition, PNGs and lifecycle/fallback. UI tests cover
legend, local state, renderer recreation, reset, capture provenance, narrow layouts,
paused no-write/no-paint behavior and unchanged measurements/pose. Host-only
validation is not operator visual approval or physical GPU qualification.
