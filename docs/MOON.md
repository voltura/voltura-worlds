# Moon World — sources and rendering

Moon is available in the library and the tray’s Solar System submenu, and participates in Random and Random Solar System. Its stable settings/package identity is `moon`. Reentering Moon starts its own
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
