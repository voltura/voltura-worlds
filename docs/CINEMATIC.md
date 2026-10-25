# Cinematic collection pass

The ten Worlds retain their existing identities, settings and assets. This pass
refines their materials, lighting and motion within the existing render-target,
geometry and simulation budgets. It does not establish visual acceptance against
the older concept art.

## Rendering changes

- Shared post processing has a soft highlight-extraction knee, restrained bloom,
  partial hue preservation in bright regions, and static sub-code-value dither.
  Orbit retains its separate display curve and focus pass. SDR video bypasses
  these effects as before.
- Aurora retains its authored composition and separate stationary stars, with
  gentler counterflow and stronger luminous crossings. Orbit refines warm cloud
  lighting, cool grazing fill, ring illumination and ocean reflections.
- Smoke refines existing broad curls and cached gradient lighting without changing
  the solver, transport stencil, grid resolution or iteration count. Its fine
  structure remains limited by the existing simulation.
- Bloom retains the original single sheet and uses more restrained cavity-aware
  satin lighting. Silk gains changing face orientation, width and asymmetric sheen.
- Eclipse has unequal streamer lengths and a tighter limb. Drift separates near
  and far particle size, softness and speed. Contour reuses terrain derivatives
  for relief beneath its major and minor lines.
- Lattice separates inner and outer walls through stronger directional lighting
  and restrained fill within its original sampling budget. Mercury uses shaped
  studio reflections and four-sample tetrahedral normals.

Movement zero now also stops Smoke simulation advancement and suppresses audio
deformation in frozen Aurora and Bloom. Orbit's post-process sunlight now respects
Brightness, including zero. Scene names, settings and World packages are unchanged.

## Reproducing native evidence

Run these from the repository root with the Release build and Worlds closed.
The capture harness owns and closes its private desktop and rendering process.

```powershell
python scripts/capture-cinematic.py before
# Build the revised renderer, then:
python scripts/capture-cinematic.py after
python scripts/review-cinematic.py
python scripts/test-cinematic.py build/Release/VolturaWorlds.exe --formats-only
python scripts/test-aurora-stars.py
```

Evidence is local under `artifacts/cinematic/`: native PNGs, 12-second 1080p clips
at 30 fps, repeated timing/allocation reports, motion contact sheets and
`review.html`. This harness does not regenerate published site media; release 0.1.6 refreshes it separately with `capture-site-media.py`. Encoded clips retain the
original capture cadence; no synthetic frames are used. Contact sheets and decoded
frame differences aid inspection but do not measure artistic quality or frame pacing.

`benchmark-cinematic.py` alternates the saved baseline shaders and current shaders
in the normal Release output, restores the candidate in a `finally` block, and
excludes the first quarter of each run from timing. It requires the original
`artifacts/cinematic/baseline-shaders/` snapshot. Do not build or run other GPU tests
while it swaps shader sets. `--timing-warmup` is an optional capture-only CLI setting;
the default remains eleven excluded frames for compatibility.

All capture and timing evidence is native D3D12 offscreen readback on a private
Windows desktop. It does not establish physical-display, sustained desktop power,
multi-monitor performance, or subjective visual acceptance. Orbit's existing lack
of audio deformation is retained.

## Validation results

Release compilation and all 31 CTest contracts passed. After adding timing sample
metadata and extending Movement-extreme coverage, capture-time and cinematic
contracts passed again. The dedicated Aurora regression compared six isolated-sky
frames with zero changed pixels. All ten Worlds passed native D3D12 debug captures
at 720x1280, 2560x1080 and 3840x2160.

All 30 scene/quality comparisons retained identical estimated GPU allocations.
Using alternating warmed runs for the outliers, 28 of 30 measured combinations
fit the 5% p95 target. Lattice Eco measured +15.6% and Auto +6.3%; High measured
+4.2%. The Lattice cavity-sampling experiment was removed, its original normal
sampling restored, and focused native parameter/format checks passed again.
Its scene shader has the same 644 SSA instructions and zero texture samples as
the original. That static count does not prove equivalent GPU time. The 5% gate
remains unconfirmed for Lattice Eco/Auto; do not describe this as a universal
performance pass. Original and alternating measurements remain available in
`comparison.json`, `paired-performance.json`, and `final-performance.json`.

The original ten 1080p recordings and final ten recordings each contain 360
native frames at 30 fps. Final recording is batched across three private desktops;
those concurrent recording timings are explicitly excluded from performance
evidence. Separate serial captures supply all reported performance comparisons.

Visual assessment: Silk has clearer ribbon faces and overlap, Eclipse has a
tighter limb with unequal streamer lengths, and Mercury's broad reflections
make its silhouette easier to read. Aurora, Orbit and Bloom retain their
established compositions with restrained lighting changes. Smoke has more
readable broad curls and shadow separation but still lacks fine fluid detail.
Aurora remains animated authored artwork, rather than a new volumetric simulator.
