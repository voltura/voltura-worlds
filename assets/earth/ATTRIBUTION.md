# Earth imagery and rendering

Earth is a measured-geography visualization with accelerated rotation, fixed
historical imagery and artistic cloud motion. It does not show current weather,
seasons, city lighting or astronomical orientation.

## Surface

NASA Earth Observatory / Reto Stöckli / Terra MODIS, **Blue Marble: Next
Generation**, January 2004 unshaded base map, 21600×10800. The runtime derivative
is 8192×4096 BC7 sRGB. The base map avoids baked terrain lighting and bathymetric
coloring. Some residual cloud contamination, coastal transitions and observation
artifacts belong to the original composite.

Source: https://science.nasa.gov/earth/earth-observatory/blue-marble-next-generation/base-map/

## Cloud coverage and space background

NASA GSFC / Reto Stöckli, enhancements by Robert Simmon, **Blue Marble: Clouds**,
8192×4096 coverage mosaic. The complete-mip BC4 linear runtime file is shared
with Orbit at `../orbit/runtime/earth-clouds-8192.dds`.

The NASA SVS **Deep Star Maps 2020**, Ernie Wright, Milky Way map is also shared
with Orbit. Source details and conversion are in `../orbit/ATTRIBUTION.md`.

## Night lights

NASA Earth Observatory / Joshua Stevens, using Suomi NPP VIIRS observations from
Miguel Román, NASA GSFC, **Black Marble 2016**, global grayscale 13500×6750
(3 km) composite. Downsampled to 8192×4096 linear intensity and stored in the
material texture's green channel. Runtime emission has restrained warm coloring.

Source: https://science.nasa.gov/earth/earth-observatory/earth-at-night/maps/

## Geographic water mask

Made with **Natural Earth**, public-domain 1:10m ocean and lake geometry from
release v5.1.2. The NASA BMNG mask archive cited during planning now redirects
to a general landing page and does not supply the mask. The versioned Natural
Earth data replace it; water is never classified from blue surface pixels.

Ocean polygons and lake polygons, including island holes, are rasterized at
16384×8192 then antialiased down to 8192×4096. Water coverage occupies the
material texture's red channel (1 water, 0 land). Its green channel is night
emission; both use linear BC5 with a complete mip chain. The shader suppresses
water reflection on bright snow/sea ice using the surface composite. Seasonal
ice and vector coastlines may differ, and minor shoreline mismatches remain
possible at the source dates/resolution.

Data: https://github.com/nvkelso/natural-earth-vector/tree/v5.1.2

Terms: https://www.naturalearthdata.com/about/terms-of-use/

## Reproduction

Run `powershell -File scripts/build-earth-textures.ps1`, which runs the Python
conversion and the existing SHA-256-pinned Microsoft DirectXTex May2026 texconv.
Python needs Pillow; `.deps/python` is supported. Original images, geometry and
intermediate PNGs stay in `.deps/earth` and do not ship. `sources.json` records
exact URLs, source hashes and conventions. A rebuild verifies downloaded/cache
files against that manifest. `runtime-hashes.json` records compressed hashes.
BC7 color mips are filtered in linear light; BC5/BC4 material data stay linear.
GPU compression is lossy and can vary with hardware/tool versions.

## Rendering model and controls

Earth uses a spherical 6371 km mean radius, 23.44° obliquity, a 0.533° solar
disc, mean-distance 1 AU relative sunlight, and an 8 km cloud shell. Radius,
tilt and sunlight references: https://nssdc.gsfc.nasa.gov/planetary/factsheet/earthfact.html

The clear-atmosphere model integrates single scattering through a 100 km shell,
with curvature-aware solar columns, 8 km molecular and 1.2 km aerosol scale
heights, Rayleigh phase and forward-directed aerosol phase (g=0.76).
Scattering coefficients follow Sébastien Hillaire, **A Scalable and Production
Ready Sky and Atmosphere Rendering Technique**, Table 1:
https://diglib.eg.org/bitstream/handle/10.1111/cgf14050/v39i4pp013-022.pdf

This is a bounded RGB approximation; ozone absorption, multiple atmospheric
scattering, true volumetric weather, terrain displacement and oblateness are
not simulated. Cloud shell relief and self-shadowing are approximate. Finite
solar-disc visibility gates direct sunlight, with no uniform night-side fill.
Ocean reflection uses a geographic water mask, Fresnel response and a GGX-style
microfacet lobe; cloud coverage casts directional shadows and obscures cities.

East longitude is positive rightward on the initial front-facing globe.
Rotation is accelerated to one eastward turn in 900 seconds at Movement 1.
Clouds rotate rigidly relative to the surface to preserve storm shapes; this
animation is artistic and is not a forecast. Stars remain stationary.

Brightness controls exposure; Movement controls the scene clock; Cloud detail
adjusts directional relief/self-shadow; Camera drift controls framing/attitude;
Cloud drift controls relative cloud motion; Atmosphere adjusts optical density.
Four palettes apply subtle white-balance grades. Audio modes gently modulate
scattering, and Movement 0 freezes audio response. Application Pause freezes
the scene clock. No new settings or World-package format version is required.

NASA media guidelines: https://www.nasa.gov/nasa-brand-center/images-and-media/

NASA and contributors are credited as sources, without endorsement. Source
media and data are separate from the application's MIT-licensed code.

## Shared catalog-star update

The planet sky now includes the NASA Hipparcos/Tycho foreground in the existing BC7 sky texture�s unused alpha. It replaces the earlier faint procedural stars (Earth/Mars) or Milky-Way-only sky (Moon/Jupiter), with no extra GPU texture allocation. See [shared sky sources and conversion](../space/ATTRIBUTION.md).
