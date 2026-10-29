# Live-cloud camera keyframes

**Display controls → 3D → Camera motion · local keyframes** animates the browser's
virtual camera while the normal live point-cloud reader continues independently.
It does not move the physical sensor, record a video/cloud history, reconstruct
unseen surfaces or change acquisition, classification, measurements or exports.

## Workflow

1. Select Depth / 3D and wait for a nonempty cloud. Arrange the starting view by
   orbiting, panning and zooming as usual.
2. Leave **New keyframe time** at **0 seconds**, then **Add current view**.
3. Move to another view. Add it at **3 seconds** (the next-time field automatically
   advances by 3 seconds after each successful add).
4. Add more views as needed. Each row's seconds field edits its absolute timestamp;
   rows automatically reorder. Times must be distinct. **Update view** replaces
   that keyframe with the current camera; **Go to view** previews it; **Remove**
   deletes it.
5. Select **Ease in/out** (default), **Smoother in/out**, or **Linear**. Scrub the
   timeline slider to preview any time, then **Play**. Live clouds and measurements
   keep arriving at their independently selected rates.
6. **Pause motion** holds the playhead; **Play** resumes. **Stop / rewind** stops
   and returns to time 0 / the first view. Replaying from the end starts at 0.
   **Clear keyframes** removes the path but keeps the current camera view.

For a full orbit, record intermediate quarter-turn views (for example at 0, 3, 6,
9 and 12 seconds), finishing at the starting view. Rotations take the shortest
angular arc between neighboring keyframes; two identical endpoint views alone
cannot encode a revolution. **Loop** repeats the timeline, not an invented closing
transition: match first/last views for a seamless loop. The camera does not track
moving people/objects automatically.

The final keyframe determines duration, up to **3600 seconds / 64 keyframes**.
A first keyframe later than 0 is held from time 0 until that timestamp. One
keyframe can be previewed but at least two are required for playback. Projection
is fixed by the first keyframe: use the same perspective/orthographic mode when
adding/updating views, or clear and start a new path. Playback/Go restores that
projection. Both projection modes support target/pan, orbit and dolly/zoom.

**Cloud target updates/s** remains separate. Choose 20 if appropriate for the live
connection/device; playback does not raise polling or promise that rate. The
renderer paints at up to its existing 30 Hz cap, using elapsed time rather than
counting cloud frames. A low cloud rate can still give smooth camera motion over
the most recently received cloud; stale/unavailable source status remains visible.

## Local-state and safety behavior

- Manual pointer/touch/wheel/keyboard navigation, Fit/Reset, API view/projection
  controls and zoom buttons pause playback before taking control. They do not
  delete keyframes. Editing a keyframe or easing/loop also pauses it.
- Page hide, viewer pause, renderer disposal/context loss, capture disable or an
  empty cloud pauses the clock. Resume/retry never silently resumes camera motion.
  Click Play deliberately. There is no hidden-tab catch-up jump.
- Keys remain in this page across same-coordinate renderer recreation/retry.
  A confirmed source restart or coordinate context change (source/type/units/scope/
  configuration revision) clears them with a visible explanation, rather than
  applying old unit-space coordinates to a new cloud. A revision can be broader
  than a true projection change; clearing is deliberately conservative.
- Settings/keys are in memory only, not persisted to browser/device storage.
  Reload starts empty. Normal and fullscreen use the same control tree.
- Original XYZD/flags, sample counts/order, cleanup policy, clipping, point style,
  color reversal, EDL and sizing references are unchanged by camera playback.
  It never refits on ordinary live frames. Camera-only paints do not upload point
  attributes or make extra API calls. Actual point updates still upload normally.
- PNGs capture the current rendered pose and report keyframe playhead/count/easing/
  playing state. The playhead is an editor position: after manual navigation while
  paused, the current camera can differ from the sampled path until Go/Play.
  Raw/binary/OBJ exports retain their independent device-frame behavior.

## Interpolation and scheduling contract

`console-camera-timeline.js` is a bounded, dependency-free model. Poses contain
look-at target XYZ, positive radius, orbit azimuth theta and polar angle phi—no
roll, mesh/surface tracking, source-frame copy or geometry interpolation. Target
and elevation interpolate linearly under the chosen eased fraction; radius uses
log-space interpolation for a positive, smooth dolly; azimuth uses the shortest
signed angular difference. Ease in/out is `3t²−2t³`; smoother is `6t⁵−15t⁴+10t³`.
Eased segments come to rest at each keyframe, with continuous endpoint velocities.

Validation rejects nonfinite fields, targets/azimuth outside ±1e9, radius outside
0.0001..1e9, polar angles outside 0.0001..π−0.0001, unsupported projections/easing,
duplicate/negative/over-hour times and capacity overflow. Failed model edits do
not replace valid keys. Poses are copied, not aliased to the mutable camera/UI.

Playback advances from `performance.now()` on the renderer's **existing coalesced
RAF**, requests the next capped paint only while playing, and stops requesting
frames on pause/end. There is no second camera timer/RAF queue or source reader.
Clock regressions are held at the last monotonic timestamp. Pause/resume rebases
elapsed time; loop uses duration modulo. At most 64 keyframes are searched per
paint; only small pose/DOM state is derived, never another point cloud. The existing
100 ms UI and measurement/status/cloud schedulers are not animation clocks.

## Validation and remaining limits

Evidence: `_build/camera-timeline/phase-20261026-162853/`.

- `node tests/console_camera_timeline.js`: bounds/copies/atomic edits, retiming,
  easing, shortest-arc seam, geometric radius, time holds, clock pause/resume/end/
  loop and finite values.
- `node tests/console_camera_timeline_browser.js`: actual WebGL1/2 eased camera
  poses with moving orthographic EDL, live frames/geometry uploads and measurements
  advancing during playback, frozen-ID zero camera-only uploads, no device writes,
  editing/scrub/loop/manual takeover, pause/hide/context loss/retry/context invalidation,
  narrow/fullscreen controls and idle-RAF termination.
- Existing source/renderer/cleanup/native/browser/host gates remain applicable.

This is virtual camera animation over currently observed points, not a complete
3D scan. Occluded/back-side holes remain. High point counts, large glyphs, EDL,
network and client GPU limits can reduce achieved paint/point-update rates. Host
software-WebGL validation does not establish target 20 FPS or physical-GPU/visual
acceptance. No deployment or physical sensor access is performed by implementation.
