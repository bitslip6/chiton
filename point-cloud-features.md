# Ranked point-cloud renderer improvements

For the current **approximately 40,000-point cloud**, prioritize **clearer presentation and targeted optimizations**, rather than techniques intended for millions of points.

Distinguish **making the data easier to see** from **making the geometry more accurate**. A renderer cannot repair an incorrect projection or unreliable depth.

This is a ranked proposal, not an implementation or authorization to change production geometry, filtering, protocols or device settings. Rankings reflect expected benefit, implementation cost and confidence—not theoretical maximum performance. Performance gains require measurement on the actual client and device.

Implementation handoff for **#2 and #5**: [point appearance and GPU coloring plan](docs/pointcloud-point-style-gpu-color-plan.md).

## Highest-value improvements

| Rank | Improvement | Expected impact |
| --- | --- | --- |
| **1** | **Fix the depth color palette** | **High visual benefit, negligible cost.** The current palette makes both range endpoints black. Values outside the range also clamp to black, against an almost-black background. Consequently, returned points can appear missing. Use a perceptually ordered palette with visible endpoints and distinguish below/above-range values. |
| **2** | **Better point appearance and size controls** | **High visual benefit.** Currently points are fixed-size squares. Offer crisp circular points, adjustable size, and optional bounded distance-based sizing. Small points preserve detail; larger points fill gaps. Avoid making everything translucent, which increases overdraw and obscures depth. |
| **3** | **Better framing when outliers exist** | **High visual benefit.** A few distant points can make the useful scene tiny during Fit. Add separate **Fit all** and **Fit main cloud** actions. The latter changes framing only—not the dataset—and should report that distinction. |
| **4** | **Adaptive drawing resolution / quality presets** | **Potentially high performance benefit**, especially on mobile or software WebGL. Reduce resolution while navigating, then restore it when stationary. Base this on measured render time, with an explicit quality setting. |
| **5** | **Move coloring into the GPU shader; consider packed XYZD attributes** | **Good architectural improvement.** Currently the CPU calculates colors and uploads Float32 position/color arrays. A shader could color directly from depth and a range uniform. Uploading the existing signed-int16 XYZD format could reduce attribute storage/upload volume from roughly **24 to 8 bytes per point**, without changing network bytes or geometry. |
| **6** | **Explicit viewer-only diagnostic masks** | **High usefulness for investigating clutter.** Provide toggles for origins, selected depth/radius ranges, and suspicious subsets, with received/shown counts and a clear “display filtered” indicator. Keep the unfiltered baseline available. **Do not automatically remove every Y=0 point.** |
| **7** | **Orthographic views and clearer orientation** | **High interpretability, little rendering cost.** Add XZ/XY/YZ presets, perspective/orthographic switching, saved poses and a small orientation widget. Clearly distinguish the cloud-centered axes from API `(0,0,0)`; neither establishes the physical sensor pose. |
| **8** | **Tune cloud refresh independently of navigation** | **High benefit for moving scenes**, but more server/network work. The default is currently 2 cloud updates/s. Test 5 Hz against actual device load and measurement latency. Increasing render FPS alone cannot make a 2-Hz source update more frequently. |

### Palette issue: confirmed current behavior

The current `cloudColor` function in `release/files/console-cloud.js`, checked with a range of 600–5400, returns:

```text
600 and below  -> black
1800           -> blue
3000           -> green
4200           -> red
5400 and above -> black
```

This is a display limitation, not evidence that the black points are invalid.

## Useful next, depending on measurements

| Rank | Improvement | Expected impact / tradeoff |
| --- | --- | --- |
| **9** | **Eye-dome lighting / screen-space depth shading** | Can substantially improve perception of edges and overlapping surfaces without meshing. Requires additional rendering work; make it optional, with a plain-points reference mode. |
| **10** | **More informative performance measurements** | Existing counters are useful, but add decode, preparation, upload and render timings, plus interaction latency. This identifies whether the bottleneck is network, JavaScript or pixel rendering before optimizing the wrong part. |
| **11** | **Tighter camera near/far planes** | Can improve depth-buffer precision and reduce visual overlap artifacts. Derive safe bounds without clipping legitimate points during navigation. |
| **12** | **Interactive clipping boxes and slices** | Very useful for examining interior structures or isolating a surface. This is a labeled viewing operation, not a validity filter. GPU clipping alone does not reduce transfer or vertex-upload costs. |
| **13** | **Capacity-based buffer allocation** | Current position/color arrays reserve about **7 MiB** regardless of the typical cloud size. Grow capacity geometrically instead of allocating the maximum immediately. Useful on constrained clients, but avoid allocation/shrink cycles every frame. |
| **14** | **Worker-based decoding and preparation** | Can keep controls responsive when parsing, bounds calculation or filtering causes main-thread stalls. Probably unnecessary at 40,000 points unless profiling shows a problem. Transfer buffers rather than repeatedly copying them. |
| **15** | **Interaction-only level of detail** | Show fewer points while dragging, restore all when stationary. Helpful on slower clients. Prefer spatially representative sampling over a simple record stride, which may reinforce the producer’s existing sampling pattern. Show rendered/received counts. |
| **16** | **Higher-quality still-image export** | Render a higher-resolution PNG on demand, retaining pose, colors and provenance, without paying that cost during live viewing. Particularly useful for reports and comparisons. |
| **17** | **Spatial tiling and frustum culling** | Useful for much larger clouds or when zoomed into small regions. At this cloud size, partitioning and additional draw calls may cost more than drawing everything. |
| **18** | **Transport improvements: compression or streaming** | Potentially useful at higher refresh rates, but first measure transfer versus processing time. Streaming needs backpressure and latest-frame handling—not a queue of increasingly stale clouds. Protocol/backend changes are a separate project. |

## Lower priority, higher risk, or outside the renderer

These can help in particular circumstances, but are not recommended starting points.

| Rank | Improvement | Why it ranks lower |
| --- | --- | --- |
| **19** | **Improve producer sampling density/pattern** | Could reduce visible banding and preserve more detail, but this is a backend change. More points increase acquisition processing, transfer and drawing costs. **Do not simply change `i+=4` to `i++`: storage capacity must be corrected and proven first.** |
| **20** | **Higher-precision coordinates / revised cloud format** | Could reduce coordinate quantization and address signed-range limitations. Needs an explicit versioned contract; the renderer cannot restore precision already discarded upstream. Establish whether the current quantization is visibly significant first. |
| **21** | **Spatial outlier removal or denoising** | Can clean isolated noise, but can also erase thin objects, real edges and small obstacles. Optional, explicitly labeled, with raw-versus-processed comparison—not a silent default. |
| **22** | **Normals and oriented surfels** | Can make surfaces more coherent and support lighting. Normal estimation adds neighborhood-processing cost and can produce misleading results near sparse regions and depth discontinuities. |
| **23** | **Temporal accumulation or smoothing** | Can improve static scenes, but moving objects or camera motion create trails and doubled surfaces. Requires registration or reliable correspondence. **Never blend points merely because their record indices match.** |
| **24** | **Surface reconstruction / meshing** | Attractive appearance, but can bridge gaps and invent surfaces that were never measured. Better as a separate reconstruction mode than a replacement for the point cloud. |
| **25** | **WebGPU or a renderer/library rewrite** | Potential long-term benefits, but no evidence that the current point count needs it. Compatibility and migration costs are substantial. Library maintenance is worthwhile separately; it is not automatically a performance upgrade. |
| **26** | **AI completion / aggressive hole filling** | Lowest priority for an inspection tool. It creates plausible-looking geometry rather than additional measurements, with considerable complexity and trust implications. |

## Existing strengths to retain

The renderer already has:

- Reusable buffers.
- A single point-cloud object.
- Bounded drawing resolution.
- On-demand rendering.
- Duplicate-frame upload suppression.
- Correct shrinking draw ranges.
- Resource disposal.

These are good foundations—not missing optimizations to rebuild.

## Recommended next build

1. **Visible, perceptually ordered depth colors.**
2. **Adjustable circular points.**
3. **GPU-based coloring**, with packed attributes evaluated alongside it.
4. **Fit-all / fit-main-cloud and orthographic presets.**
5. **Measured quality presets**, including reduced resolution during interaction.
6. **Explicit diagnostic masks**, preserving the raw view.

This offers the strongest combination of immediate clarity, responsiveness and low risk of misrepresenting the measurements.
