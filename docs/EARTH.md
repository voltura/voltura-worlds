# Earth World — implementation and verification

Earth is available after Mars in the library and tray catalog, and participates
in Random. It uses the existing settings/World-package schema with the new
stable `earth` identity. Existing scene enum values are preserved.

## Sources and reproduction

See [Earth attribution and rendering model](../assets/earth/ATTRIBUTION.md) for
NASA source records, Natural Earth water-mask data, physical references,
conversion and approximation limits. Exact source and runtime hashes are
recorded beside that document. The unavailable legacy NASA water-mask archive
was replaced with versioned public-domain Natural Earth ocean/lake geometry;
NASA imagery supplies the surface, clouds, night lights and shared sky.

Run `powershell -File scripts/build-earth-textures.ps1` to regenerate the two
8K runtime DDS files. The 8K BC4 cloud map and star map are shared with Orbit.
The source download/geometry/PNG cache is development-only and is not shipped.
The narrow DDS reader now also accepts complete-mip BC5 two-channel textures.

## Verification — 2026-10-25

- Release compiled through the normal build directory. Its old cache referenced
  the previous `voltura-new` directory; it was regenerated with CMake `--fresh`.
- All 32 existing CTest checks passed, including Mars, other Worlds, native UI,
  settings/packages, desktop hosting, video, Smoke and updater contracts.
- The new `earth_render_contracts` passed in a focused CTest rerun after fixing
  a test classification error: bright mask output and ocean highlights can
  bloom across coastlines. The diagnostic now exposes its geographic mask
  below the bloom knee and checks land interiors away from shorelines.
- Earth tests verify independent controls, zero brightness, frozen time/audio,
  audio response, a 900-second turn, stationary stars and geographic alignment
  against independently projected source pixels at three phases. Isolated
  shader terms verify daylight city suppression, dark unlit ground, water-only
  glints and directional cloud shadows.
- At the sampled interior points: daylight city maximum 0/255; unlit ground
  maximum 0/255; land-interior glint maximum 1/255; water glint maximum 157/255.
  Full-turn mean difference was 0.000253/255.
- Native D3D12 debug captures passed repeated Earth/Mars/Orbit/other-scene
  transitions and portrait/landscape resize cycles with stable GPU allocation
  accounting on return to the original size.
- Inspected 1080p and 4K output, unscaled 4K detail, portrait and ultrawide
  framing, and later globe phases exposing the Pacific/longitude seam. Captured
  90 native frames at 30 FPS and encoded a three-second H.264 preview. Six native
  detail frames were inspected; the background remained pixel-identical across
  all frames. Adjacent-frame mean difference ranged 0.1166–0.1178/255.
- A local unsigned installer compiled successfully with Earth shaders, maps,
  manifests and credits, including explicit uninstall entries. Installation,
  upgrade and uninstall were not rerun for this change. No release was published.

Evidence lives in ignored `artifacts/earth/`: `regressions/results.json`,
`performance/results.json`, size/phase captures, `4k-detail.png`,
`motion/earth-motion.mp4` and `motion/motion-review.json`.

## Initial performance captures (before memory optimization)

Intel Arc Graphics, Release, 2× scene MSAA, D3D12 debug layer enabled, 180
offscreen frames with 45 timing warmup frames and 135 timing samples:

| Resolution | GPU p95 | Process private memory | Estimated GPU allocation |
| --- | ---: | ---: | ---: |
| 1920×1080 | 2.60 ms | 483.6 MiB | 234.5 MiB |
| 3840×2160 | 10.10 ms | 773.8 MiB | 507.1 MiB |

The 1080p capture is below the project's 8 ms GPU target. Both resolutions
exceed the 300 MiB process-private-memory target. These are short native
offscreen readbacks, not sustained physical desktop, presentation cadence,
multi-monitor, power-use or subjective visual acceptance measurements.

The verified `build/Release/VolturaWorlds.exe` was launched for review with Earth
selected. The separately installed older version was not upgraded.

## Memory optimization — 2026-10-25

Earth now renders directly to the existing single-sample HDR target. Its
fullscreen pixel shaders use analytic silhouette coverage, so the former
multisample color/depth targets duplicated the same shaded samples. Earth also
avoids allocating the unused single-sample depth buffer. Mesh Worlds retain
their geometry depth and multisampling paths, recreated on demand.
Allocation accounting includes the actual optional depth allocation.

The shared DDS loader now reads a read-only file mapping and uploads unchanged
compressed blocks through a reusable staging buffer capped at 4 MiB. Previously
it allocated both an entire private file copy and a full-mip-chain upload buffer
for each texture. Fence completion precedes staging-buffer reuse. Texture
dimensions, compression, mip chains, sampling, shaders and exposure are unchanged.

Fresh before/after captures on Intel Arc Graphics used the Release executable,
D3D12 debug layer, 180 frames and 45 timing warmup frames:

| Resolution | Private memory before → after | Estimated GPU allocation before → after |
| --- | ---: | ---: |
| 1920×1080 | 462.4 → 364.2 MiB (−21.2%) | 234.5 → 170.6 MiB (−27.3%) |
| 3840×2160 | 773.7 → 441.3 MiB (−43.0%) | 507.1 → 271.5 MiB (−46.5%) |

Both resolution comparisons are pixel-identical. Another 28 original Earth
control, audio, lighting-probe and rotation captures match pixel-for-pixel.
Mars and Orbit 1280×720 captures also remain pixel-identical after the shared
upload change. Earth regression coverage additionally checks that viewport
allocation growth contains only display, HDR and bloom buffers, and that
Earth reports one sample per pixel. Scene-switch/resize GPU validation passes.

The Release rebuild and all nine selected CTest checks passed: core,
capture-time, Earth, Mars, Bloom, cinematic coverage, desktop hosting, video
and library UI. The updated Release application was reopened after validation.

Private memory remains above the 300 MiB target. GPU p95 in these short runs
was 2.64 → 5.51 ms at 1080p and 11.03 → 12.90 ms at 4K; these samples do not
establish a speed improvement or sustained presentation performance. Allocation
savings and exact pixel comparisons are the acceptance evidence for this change.

Evidence: ignored `artifacts/earth-memory/final-comparison.json`,
`control-comparison.json`, `before/`, `final/`, `upload-before/` and
`upload-after/`. The initial before/after buffer-only experiment is in `after/`.

## Applying the findings to Mars and Orbit

The file-backed, bounded DDS uploader already applied to all three space
Worlds. Mars now also uses the same direct HDR path as Earth, retaining its
analytic silhouette coverage and all source textures and shading. Orbit keeps
multisampling and its matching depth target for its meshes, rings and moons.
Its unused single-sample depth target is no longer allocated while MSAA is
active. The shared bloom and display passes bind no depth target.

Single-sample depth is now allocated only when Orbit or Bloom needs the
non-MSAA fallback. Capture-only `--single-sample` exercises that path; Orbit
and Bloom debug captures passed in this configuration. Earth's lifecycle test
also exercises scene switching and resizing in the forced fallback mode.

The following incremental measurements start **after** the shared DDS-loader
optimization was already present, using fresh 180-frame debug captures with
45 timing warmup frames:

| Scene / resolution | Private memory before → after | Estimated GPU allocation before → after |
| --- | ---: | ---: |
| Mars / 1080p | 402.6 → 348.7 MiB | 211.6 → 146.6 MiB |
| Mars / 4K | 658.1 → 415.7 MiB | 485.3 → 247.5 MiB |
| Orbit / 1080p | 361.3 → 351.6 MiB | 158.3 → 149.3 MiB |
| Orbit / 4K | 616.4 → 581.0 MiB | 432.0 → 398.2 MiB |

All four image pairs are pixel-identical. Orbit retains two samples per pixel
on the tested Intel Arc adapter; Mars uses one. All 19 original Mars control,
terrain, lighting and rotation captures also remain pixel-identical. Evidence is in ignored
`artifacts/space-memory/comparison.json`, `before/`, `after/` and `fallback/`.
These are short offscreen measurements with the same limitations described
above; private-memory targets are not claimed met.

The Release build and the same nine selected regression checks passed again
after extending the cleanup to Mars and Orbit. The updated local app was
reopened for review.
