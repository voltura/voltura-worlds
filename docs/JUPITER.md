# Jupiter World — sources and rendering

Jupiter is available in the library and the tray’s Solar System submenu, and participates in Random and Random Solar System under stable identity `jupiter`. A per-display
clock starts Jupiter with the Great Red Spot visible, without changing the
other Worlds' timing.

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
wind advection is bounded with staggered texture backtraces. The Red Spot
circulates independently with a feathered elliptical vortex. This artistic
animation preserves source detail and is not a forecast.

The 3600×1800 source resolution is represented by 4096×2048 BC7 sRGB color
and BC5 linear atmospheric data, approximately 21.3 MiB combined. No 8K
upscale, surface-terrain normals, depth target or multisample target is used.
The nominal 71,492 / 66,854 km radii define the oblate silhouette; normal-
derived planetographic latitude aligns both maps with the ellipsoid.

Atmosphere uses a cheap curvature-bounded column approximation and a
footprint-filtered subpixel limb at the reference 27 km scale height.
Sunlight follows mean 5.204 AU irradiance before cinematic exposure.
Textures load on demand
through the existing bounded DDS uploader and release on scene changes.

## Shared star background

The mapped planetary Worlds use the matching NASA Hipparcos/Tycho
foreground packed into the unused alpha of the existing 4K BC7 sky. This
uses visible catalog-based stars with no extra resident texture memory.
The fixed-view Worlds keep their stars stationary; Moon projects them with
its orbiting camera. See [shared sky attribution](../assets/space/ATTRIBUTION.md).
Orbit retains its original sky. Switching between the two sky variants
replaces the existing resource rather than keeping both resident.
