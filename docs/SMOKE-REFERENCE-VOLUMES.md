# Frozen volume diagnostics

`--smoke-reference-volume` is a capture-only renderer diagnostic. It uploads a
raw, tightly packed, little-endian RGBA16F volume and bypasses fluid evolution.
RGB holds gold/cyan/magenta concentrations; alpha holds total density. X varies
fastest, then Y, then Z. The file must exactly match the selected density grid.
Nonfinite or negative half-float values and incorrect lengths are rejected.
This is not a community file format or a user-facing background option.

For spatial-resolution comparisons, `--reference-density-grid 384` overrides
the cubic density dimensions independently of the velocity grid. It requires
an imported frozen volume and accepts multiples of four from 24 through 384.
Wide-density and transport modes are rejected. The file must match the override
exactly. Normal wallpaper dimensions and quality settings do not change.
High-resolution reference captures can exceed the product memory budget; they
are diagnostic evidence, not a production quality tier. The reference-volume
contract checks byte-identical upload/readback with independent grid dimensions
and rejection of invalid values and incompatible modes.

The default remains frozen. For capture-only transport research, adding
`--transport-test maccormack` seeds the analytic rotation with the imported
density. Adding `--flowmap-reference` as well instead advects FP32 coordinates
and samples the original density through that map. This mode requires an
imported volume and MacCormack transport selection; it does not use the live
smoke forces, emitters or chart resets. The command selects the comparison
fixture, not a claim that the map itself uses MacCormack interpolation.
Metrics identify `flowmapReference` and the number of `simulationSteps`.

The flow-map diagnostic keeps the original density in the first density
texture and writes mapped output into the second. It is not enabled in normal
wallpaper rendering. Repeated nonlinear deformation, chart renewal, source
injection and long-running smoke quality are still unverified.

`smoke_flowmap_contracts` checks actual quarter-turn motion and fine-pattern
retention after a full turn using GPU density readback. The central-plane
error bound is specific to this analytic fixture, not artwork acceptance.

`--transport-deform` selects a reversible divergence-free nonlinear flow rather
than rigid rotation. It requires imported density and a transport test. Its
time multiplier reverses the deformation so exact trajectories return after
12 seconds; a halfway check confirms that the field did deform.

The flow-map path now applies a forward/reverse MacCormack correction to FP32
coordinates, limited to their departure-neighborhood range. It reuses the
otherwise unused detail-velocity volume for the intermediate map. This limits
the capture-only flow-map mode to velocity grids at most 96; larger grids are
rejected. No extra volume allocation or normal-wallpaper pass was added.

`--flowmap-renew 2` optionally rebuilds the reference density and resets the map
every two simulation seconds (accepted range 0.25–12). It preserves the latest
output before resetting, then advances one normal step. No catch-up steps are
introduced. Metrics record the interval and actual renewal count. Default is
no renewal.

`--flowmap-cubic` selects bounded 64-tap Catmull–Rom reconstruction for mapped
density instead of trilinear sampling. It clamps each result to the eight
nearest reference samples to prevent interpolation overshoot. Both switches
are capture-only and require flow-map reference mode. Renewal still loses
detail; the sharper reconstruction reduces that loss in the measured fixture
but is not evidence of artifact-free live smoke or imperceptible renewal.

`--flowmap-live` is a separate capture-only experiment using the real fluid
velocity, pressure projection, vorticity, buoyancy and continuous pigment
sources. It requires Smoke with velocity grid <=96, and rejects imported volumes
or analytic transport options. Bounded cubic reconstruction is enabled; map
renewal defaults to 0.25 seconds and can be overridden by `--flowmap-renew`.
The shorter diagnostic default limits severe map folding seen at two seconds;
it is not a production quality setting or a substitute for distortion-aware renewal.

The existing two density buffers carry newly emitted residual density between
renewals. Two extra RGBA16F volumes hold the mapped reference and combined
output. Buoyancy, lighting, drawing and captured density use the combined field.
At renewal, combined density becomes the reference and residual density clears
without clearing velocity or pressure. The reference decays by its map age.
This path is not enabled in the wallpaper application or included in a new
installer. It currently has visible artifacts and misses performance targets.

`--flowmap-adaptive` optionally replaces the live prototype's short fixed
interval with distortion-triggered renewal and a two-second maximum age. A GPU
counter samples occupied interior coordinate cells. Cells fail when the sampled
Jacobian determinant is outside [0.5,2], nonfinite, or a Jacobian column length
exceeds 3. Renewal triggers when more than 1% (and more than eight cells) fail,
with a minimum age of 0.05 seconds. This is a heuristic, not proof of map validity.

The CPU reads 16-byte counter results only after the existing frame-slot fence
has completed, without an additional GPU wait. Epoch checks discard results
from before a renewal. Readback latency means distortion can exceed the limit
before renewal. This option requires live mode and cubic volume dimensions;
the rectangular-density experiment is rejected. Normal wallpaper remains
unaffected. Metrics distinguish distortion-triggered renewals from total count.

Example from the repository root:

```powershell
C:/Python314/python.exe scripts/test_desktop.py -- build/Release/VolturaWorlds.exe --capture-dir artifacts/reference-check --scene smoke --width 960 --height 540 --frames 1 --grid 96 --smoke-reference-volume artifacts/revision-102/reference-rgba16f.bin --debug-gpu --fluid-diagnostics --smoke-render-diagnostics
```

Grid96 selects192-cubed density. The reference upload remains alive for the
capture lifetime and is included in allocation estimates. Metrics explicitly
identify frozen imported density and zero simulation steps. Timing is not a
measurement of the real-time fluid solver.

`smoke-white-lit.png` disables pigment color/emission while retaining cached
lighting. Other diagnostic images show unlit pigment, a density slice and
integrated opacity. Normal rendering parameters remain unchanged.

## Revision102 evidence

- `artifacts/revision-102/bake-reference.py` created a local Blender5.2.2 smoke
  simulation:96-cubed base,2x noise, three basis-colored sources,40frames.
  Bake completed in222.96seconds. Blender's direct density/color arrays were
  empty after this headless bake; those empty exports are not reference data.
- `convert-reference.py` reads the completed192-cubed noise cache instead,
  maps Blender Z-up to native Y-up, normalizes source concentrations and scales
  density by.1. It validates the known scalar MNT3 layout against
  [Blender's Mantaflow grid I/O source](https://raw.githubusercontent.com/blender/blender/main/extern/mantaflow/preprocessed/fileio/iogrids.cpp).
- Native upload/readback SHA256 matches the converted input exactly:
  `0F3A8ACB86B6256376D7CB26CAAA0D1D5705A396AD9E914F77480B5B9D3C13B9`.
- `render-reference.py` renders the baked volume with Blender Cycles,32samples,
  no denoising, white material, black environment and directional light.
  `blender-white-reference.png` is offline evidence, not native rendering.
- `lighting/smoke-white-lit.png` uses native D3D12. Projection/light directions
  are approximately aligned, but scattering, density cutoff, exposure and
  display transforms are not calibrated identically. Do not claim numerical
  renderer equivalence or measured relative image quality.

Both images retain similar major smoke features. The native approximation
emphasizes small surface-like highlights; the offline image is softer/noisy at
32samples. Neither volume composition matches the approved concept. This
fixture isolates subsequent lighting work from solver changes; it is not a
replacement for live simulation or a passed visual checkpoint.

`smoke_reference_volume_contracts` verifies byte-identical native density
roundtrip and rejection of truncated, extended, NaN and negative inputs.
