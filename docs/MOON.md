# Moon World — implementation and verification

Moon appears immediately after Earth in the library/tray catalog and
participates in Random. Its stable identity is `moon`; old scene enum values,
settings and package schemas are preserved. Reentering Moon starts its own
per-display camera clock on the near side. Normal pause policy stops that
clock, while Movement zero freezes shader time and audio response.

## Reproduction and rendering

See [Moon attribution](../assets/moon/ATTRIBUTION.md), `sources.json` and
`runtime-hashes.json` alongside it for the NASA source records, conversion,
checksums, known limitations and rendering assumptions. Optional pinned
texture tools are in `requirements-textures.txt`.

The Moon uses December 2025 RGB16 LROC imagery and 64-pixels-per-degree LOLA
elevations, converted to 8K BC7 color/normals. A separate 2K NASA Earth
composite packs surface/cloud color and independent night emission. Alpha is
filtered separately when generating its mipmaps. Original source caches are
development-only. Lunar runtime maps total approximately 88 MiB on disk.

The single-sample analytic renderer uses no scene depth or MSAA target.
Descriptors 399–401 follow Earth's existing range; heap capacity is 404.
The bounded DDS uploader and shared sky remain unchanged. Scene texture
allocations are released when leaving Moon.

The camera circles at three lunar radii every 900 seconds at Movement 1.
Earth remains fixed at its natural radius and mean separation; the Moon's
near side faces Earth. A fixed Sun lights both bodies. Earth begins behind
the camera, becomes visible beside the Moon during portions of the tour,
and is occluded by the lunar globe. This is an accelerated camera journey,
not an ephemeris simulation. There is no displaced terrain, crater
self-shadowing, lunar atmosphere or uniform night-side fill. Shared bloom
represents camera glare rather than atmospheric scattering.

## Validation commands

```powershell
python scripts/test-moon.py build/Release/VolturaWorlds.exe
```

The `moon_render_contracts` CTest entry checks official-source landmark
samples, seam/pole conversion, independent perspective geography at four
tour phases, finite night illumination, a complete camera loop, independent
controls/palettes, audio/frozen time, all quality options, Earth size and
occlusion, Earth lighting against the shared Sun, and debug-layer resource
lifecycle under scene switching/resizing and single-sample fallback.

Ignored evidence directories: `artifacts/moon/regressions/` for checks and
metrics, `artifacts/moon/visual/` for 1080p/4K/portrait/ultrawide and phase
captures plus a 90-frame, 30 FPS native motion recording, and
`artifacts/moon/performance/` for matched Moon/Mars measurements.

## Verified 2026-10-25

- Release compiled through the normal build directory. All 33 preexisting
  CTest checks passed. After the final near-side clock change, core, Moon and
  native library UI checks passed again (34 distinct checks covered in total).
- Native Moon checks verify nine official-source landmark samples, both
  polar normal rows, four independently projected globe orientations, all
  controls and palettes, quality options, audio and frozen Movement. Mean
  geographic display error was 2.28–2.56/255. Deep-night maximum was 0/255
  in captures below the bloom knee. Full-tour mean difference was
  0.000472/255.
- At 640×360, Earth's visible mask contained 162 and 167 pixels versus
  independently predicted 161 and 162. Earth was absent when behind the
  camera and when hidden by the Moon. Shared-Sun Earth lighting comparisons
  had mean errors of 7.18 and 3.93/255, including image/mipmap filtering.
- D3D12 debug checks passed scene switching, portrait/landscape resizing,
  allocation return-to-baseline and the single-sample geometry fallback.
  Viewport allocation growth contains only display, HDR and bloom targets.
- Native 1080p, 4K, portrait, ultrawide, far-side, Earth-visible and dark-passage
  captures were inspected, including unscaled 4K detail. A 90-frame native
  sequence was encoded as three seconds of H.264 at the original 30 FPS.
- An unsigned local installer compiled with the Moon shaders, textures,
  manifests and credits, including uninstall entries. Installation, upgrade
  and uninstall were not rerun. No release was published.
- The final Release app was reopened on the user's desktop with Moon
  selected. Its native status reports Moon with audio off and the lunar
  terrain controls; review evidence is in `artifacts/moon/app-review.json`.

### Matched performance captures

Intel Arc Graphics, Release, D3D12 debug layer enabled, 180 offscreen frames,
45 timing warmup frames and 135 samples per capture. Both Worlds use one
scene sample; each capture ran in a separate process after other tests and
installer compression completed.

| World / resolution | GPU p95 | Process private memory | Estimated GPU allocation |
| --- | ---: | ---: | ---: |
| Mars / 1920×1080 | 2.45 ms | 338.9 MiB | 146.6 MiB |
| Moon / 1920×1080 | 2.73 ms | 341.0 MiB | 150.1 MiB |
| Mars / 3840×2160 | 6.80 ms | 416.1 MiB | 247.5 MiB |
| Moon / 3840×2160 | 5.06 ms | 417.6 MiB | 251.0 MiB |

The compact Earth adds about 3.5 MiB of actual GPU allocation over Mars's
two-map setup. The 1080p timing difference is 0.28 ms; both samples are below
the project's 8 ms GPU target. Process private memory remains above the
300 MiB target. These short captures establish neither sustained desktop
presentation performance nor multi-monitor power use. See the ignored
`performance/results.json`, `full-ctest.log` and `final-ctest.log` evidence.
