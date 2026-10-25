# Jupiter â€” observed clouds and atmospheric maps

## Sources and license

NASA/ESA Hubble Space Telescope, WFC3/UVIS, **Outer Planet Atmospheres Legacy
(OPAL), Cycle 32**, observations of December 11â€“12, 2025. Credit: NASA, ESA,
A. A. Simon and M. H. Wong. OPAL PI: Amy Simon; this cycle is program GO17843.
The Space Telescope Science Institute archives these data; STScI is operated
by AURA under NASA contract NAS 5-26555.

Archive: https://archive.stsci.edu/hlsp/opal/opal-jupiter-cycle-32
Dataset DOI: https://doi.org/10.17909/T9G593
Program and processing references: https://archive.stsci.edu/hlsp/opal

These OPAL high-level science products are **CC BY 4.0**:
https://creativecommons.org/licenses/by/4.0/
Voltura's modifications are described below. These assets are separate from
the application's MIT code; NASA/ESA and the authors do not endorse Worlds.

Inputs, with exact URLs and SHA-256 hashes in `sources.json`:

- Rotation A `f395n-f502n-f631n` global color TIFF. Its RGB channels represent
  F631N, F502N and F395N respectively. This is an arbitrarily scaled visible
  color visualization composite, treated as sRGB; not calibrated true color.
- Rotation A FQ889N methane-band and F631N continuum FITS maps. README
  conversion factors 0.00088 and 0.00383 produce I/F reflectance values.
- Rotation B F631N FITS, for an estimate of latitude-dependent cloud motion.
- The cycle README, including processing, scaling and residual-fringe notes.

Physical shape, orientation and atmospheric references: NASA NSSDCA
[Jupiter Fact Sheet](https://nssdc.gsfc.nasa.gov/planetary/factsheet/jupiterfact.html).
The nominal 1-bar radii are 71,492 km equatorial / 66,854 km polar; obliquity
3.13Â°, System III day 9.925 h, mean distance 5.204 AU, density scale height
27 km. Dominant gases are hydrogen and helium with aerosols. These constants
constrain an approximate visible rendering, not a spectral atmosphere model.

The space background shares the NASA SVS **Deep Star Maps 2020** Milky Way
derivative. Credits and processing are in [Orbit attribution](../orbit/ATTRIBUTION.md).

## Conversion and coordinates

Run `powershell -File scripts/build-jupiter-textures.ps1`. Python 3.12+ with
the tools in `requirements-textures.txt` is supported via `.deps/python`.
PowerShell downloads using Windows certificate verification. The source
hashes and Microsoft DirectXTex May2026 executable hash are checked before
conversion. Original TIFF/FITS files stay in `.deps/jupiter` and are not shipped.

All source maps are 3600Ã—1800, 10 pixels per degree. Runtime maps are
4096Ã—2048 BC7 sRGB color and BC5 linear atmospheric data, with complete mip
chains: approximately 21.3 MiB combined. The modest resampling adds no
observed detail; an 8K upscale would waste memory. Color resizing/mipmaps are
filtered in linear light. BC compression is lossy and may vary by hardware.
`runtime-hashes.json` identifies the generated compressed files.

The source starts at 0Â° System III west longitude, decreasing to the right
(positive east). Rows run north to south over both hemispheres. The global
1800-row maps span 180Â° latitude; the cycle README's phrase "0 and +90" does
not describe their complete two-hemisphere extent. The color and FITS maps
have matching row order, verified by red-channel correlation; no vertical
flip is applied. Coordinates use planetographic latitude, derived from the
ellipsoid normal, rather than geocentric latitude.

Observed pixels are retained through Â±75Â°. From 75Â° to 80Â°, the maps blend
smoothly toward the corresponding 80Â° zonal mean. Beyond 80Â° they retain
that mean, suppressing marginal gaps/fringes and avoiding invented polar
cyclones. These caps are a reconstruction, not additional Juno observations.

## Atmospheric data and motion

Material R contains the FQ889N/F631N reflectance ratio, normalized using its
2nd/98th percentiles between Â±80Â°. It is a high-cloud/aerosol **proxy**: no
cloud elevation, optical depth or surface terrain is measured by this
conversion. Residual methane-band fringes and spectral/temporal differences
remain limitations. Haze optical depth and color in the shader are approximate.

Material G encodes a cloud-motion estimate from successive red maps, whose
observation-midpoint separation is approximately 33,893.5 seconds. Eleven-row
latitude strips are cross-correlated over longitude within a Â±180 m/s search.
Correlations below 0.65 and search-boundary matches are rejected. Subpixel
peak interpolation and 0.7Â° Gaussian smoothing reduce noise; rates taper to
zero from 65Â° to 70Â° latitude. `wind-profile.json` preserves the estimated
rates and correlation diagnostics. This is a simplified Voltura estimate
from OPAL observations, not an instrument-team-validated wind product.
G byte 128 is zero; angular radians/s = `(G * 255 - 128) * 0.00001 / 127`
when G is sampled as a normalized channel.

The cloud deck rotates prograde every 900 seconds at Movement 1, starting
with the observed Great Red Spot facing the viewer. Artistic cloud motion
uses the inferred zonal wind directions at 300 times their physical rates.
Two staggered 80-second backtraces blend smoothly, bounding stretching and
retiring each trace at zero weight rather than reversing the winds.

An elliptical vortex centered on the source landmark (38 degrees east,
22 degrees south) circulates the photographed storm detail. Its base
angular rate is 0.025 radians per cinematic second, with a bounded radial
perturbation that varies local speeds without accumulating shear. A smooth
sample blend feathers the vortex into surrounding belts, which taper their
drift around the storm core. Color and methane proxy use identical sample
coordinates and blend weights. The motion is an artistic animation of an
observation, not a fluid simulation, measured storm evolution, or forecast.

## Rendering and controls

The globe is an analytically intersected oblate spheroid with single-sample
HDR coverage and no depth/MSAA target. Texture resources load on demand and
are released on scene changes. The fixed sky does not rotate with the planet.

Cloud lighting uses diffuse reflection, inverse-square sunlight at 5.204 AU
and a finite solar disc, followed by cinematic exposure. The fixed Sun is
8 degrees from the viewing direction, giving a nearly full illuminated
disc while preserving the measured oblate silhouette. A cheap,
curvature-bounded column approximation represents neutral/warm aerosol haze
above the opaque cloud deck. Its scale height is physically constrained;
limb contribution is filtered over the pixel footprint to stabilize the
subpixel shell. There is no uniform night light, terrain-normal map or blue
Earth-like atmosphere. Cloud self-shadowing, volumetric storms, lightning,
aurorae, rings, moons and ephemerides are not modeled in this World.

Brightness controls exposure; Movement controls all clocks; Cloud detail
adjusts contrast against a lower-resolution observed image; Camera drift
adjusts framing/attitude; Cloud drift scales zonal advection and storm circulation;
Atmosphere adjusts approximate aerosol optical depth. Four natural palettes
apply subtle grades. Optional audio affects exposure gently. Movement zero
freezes motion and audio; application pause stops rendering/time advancement.
Eco, Auto and High preserve the same maps and analytic rendering detail.

## Shared catalog-star update

The planet sky now includes the NASA Hipparcos/Tycho foreground in the existing BC7 sky texture’s unused alpha. It replaces the earlier faint procedural stars (Earth/Mars) or Milky-Way-only sky (Moon/Jupiter), with no extra GPU texture allocation. See [shared sky sources and conversion](../space/ATTRIBUTION.md).
