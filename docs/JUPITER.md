# Jupiter World — implementation and verification

Jupiter appears after Moon in the library/tray catalog and participates in
Random, settings and `.world` packages under stable identity `jupiter`.
Existing scene enum values and data schemas are preserved. A per-display
clock starts Jupiter with the Great Red Spot visible, without changing the
other Worlds' timing. Moon retains its near-side clock behavior.

## Imagery, atmosphere and efficiency

See [Jupiter attribution](../assets/jupiter/ATTRIBUTION.md), `sources.json`,
`runtime-hashes.json` and `wind-profile.json` for exact NASA/ESA OPAL sources,
license, hashes, processing and limitations. Optional pinned development
tools are in `requirements-textures.txt`; regenerate with
`powershell -File scripts/build-jupiter-textures.ps1`.

December 2025 Hubble color imagery supplies the cloud deck. Matched FQ889N
and F631N maps supply a normalized methane/continuum reflectance ratio for
an aerosol proxy. Successive red-filter mosaics supply a simplified inferred
zonal-wind profile. The proxy is not cloud height or measured optical depth;
wind motion is deliberately bounded and reversible, not a forecast.

The 3600×1800 source resolution is represented by 4096×2048 BC7 sRGB color
and BC5 linear atmospheric data, approximately 21.3 MiB combined. No 8K
upscale, surface-terrain normals, depth target or multisample target is used.
The nominal 71,492 / 66,854 km radii define the oblate silhouette; normal-
derived planetographic latitude aligns both maps with the ellipsoid.

Atmosphere uses a cheap curvature-bounded column approximation and a
footprint-filtered subpixel limb at the reference 27 km scale height.
Sunlight follows mean 5.204 AU irradiance before cinematic exposure.
Descriptors 402–404 follow Moon in a 408-entry heap. Textures load on demand
through the existing bounded DDS uploader and release on scene changes.

## Shared star background

Earth, Mars, Moon and Jupiter now use the matching NASA Hipparcos/Tycho
foreground packed into the unused alpha of the existing 4K BC7 sky. This
adds visible catalog-based stars with no extra resident texture memory.
The fixed-view Worlds keep their stars stationary; Moon projects them with
its orbiting camera. See [shared sky attribution](../assets/space/ATTRIBUTION.md).
Orbit retains its original sky. Switching between the two sky variants
replaces the existing resource rather than keeping both resident.

## Verification

`jupiter_render_contracts` checks independent OPAL geography/methane
projection at four phases, source landmarks, inferred wind motion, measured
oblate shape, all controls/palettes/quality modes, frozen time/audio, a
900-second full composition, dark night-side lighting, viewport allocation
growth and D3D12 scene-switch/resize lifecycle, including geometry fallback.

`planet_stars_render_contracts` checks visible small stars in all four
Worlds, stationary backgrounds in Earth/Mars/Jupiter, Moon camera motion and
its complete sky loop, plus unchanged packed texture size. Existing space
World tests continue to check opaque unlit globe interiors and frozen audio.

Ignored evidence is stored under `artifacts/jupiter/` and
`artifacts/planet-stars/`. Native motion recordings use the original
30 FPS animation speed. Performance captures are short offscreen D3D12
readbacks, not sustained desktop/multi-monitor presentation or power tests.

## Verified 2026-10-25

- Release built in the normal output directory. The complete 35-check suite
  passed before the star update. Core, library UI, all four planet suites and
  the new shared-star suite then passed; final sky refinements were rechecked
  with the shared-star suite and Jupiter contracts. All 36 distinct current
  CTest contracts are covered. Logs: `full-ctest.log`, `planet-ctest.log` and
  `artifacts/planet-stars/final-ctest.log` in the ignored evidence folders.
- Independent projection mean errors were 0.92–0.96/255 for visible cloud
  geography and 0.65–0.71/255 for the methane proxy at four rotation phases.
  Deep-night values were 0/255 below the bloom knee. The full composition
  repeated after 900 seconds with a mean difference of approximately
  0.00005/255. The measured oblate mask differed from independent double-
  precision intersections by 23 pixels in a 640×360 capture.
- D3D12 validation passed repeated scene/sky-variant switches, portrait and
  landscape resizing, return-to-baseline allocations and geometry fallback.
  Cloud drift changes the observed cloud pattern without moving the sky.
- Shared stars are visible in all four Worlds. Fixed-view backgrounds remain
  pixel-identical over rotation; Moon's sky follows its camera and closes
  after one tour. Brightness zero and frozen Movement/audio checks pass.
  Before/after estimated GPU allocations are identical in all four matched
  captures (`artifacts/planet-stars/resource-comparison.json`).
- Native 1080p, 4K, portrait, ultrawide, seam and opposite-side views were
  inspected, including unscaled Great Red Spot detail. A 90-frame native
  sequence was encoded as a three-second 30 FPS H.264 preview. Adjacent-frame
  mean differences were 0.01223–0.01245/255, with a stationary star background.
- The local unsigned installer includes Jupiter and shared sky maps, compiled
  shaders, provenance and explicit uninstall entries. Installation, upgrade
  and uninstall were not rerun; nothing was published.
- The final Release app was reopened with Jupiter selected. Native status
  reports `Desktop · jupiter | Audio off` and the Jupiter cloud controls;
  evidence is in `artifacts/jupiter/app-review.json`.

### Matched offscreen performance

Intel Arc Graphics, Release, D3D12 debug layer, 180 frames with 45 timing warmup
frames and 135 timing samples; separate processes after other GPU checks and
installer compression. Both scenes use one sample per pixel.

| World / resolution | GPU p95 | Process private memory | Estimated GPU allocation |
| --- | ---: | ---: | ---: |
| Mars / 1920×1080 | 2.09 ms | 339.2 MiB | 146.6 MiB |
| Jupiter / 1920×1080 | 1.80 ms | 264.8 MiB | 72.6 MiB |
| Mars / 3840×2160 | 6.34 ms | 415.7 MiB | 247.5 MiB |
| Jupiter / 3840×2160 | 3.79 ms | 342.2 MiB | 173.5 MiB |

Jupiter stays below the 8 ms GPU target in these short captures. Its 1080p
private memory is below 300 MiB; 4K remains above that target. These samples
do not establish sustained presentation cadence or power use. The saved
performance report records executable and shader hashes.

## Near-frontal lighting refinement — 2026-10-25

The fixed Sun now sits 8 degrees from the viewing direction instead of
approximately 43 degrees. This removes the broad dark crescent that made
the illuminated area look excessively elliptical. The actual oblate geometry,
map coordinates, cloud motion, haze and resource allocation are unchanged.

The Release rebuild and Jupiter render contracts passed. A new isolated
daylight mask measures 99.47% illuminated projected area, and 30 independently
selected points on the narrow unlit limb remain 0/255. The silhouette still
matches measured radii within 23 pixels at 640×360. Native debug captures at
1080p, 4K, portrait and an opposite rotation phase were inspected; evidence
is in `artifacts/jupiter/frontal-light/` and the updated regression results.
The earlier performance and installer records above predate this refinement.
