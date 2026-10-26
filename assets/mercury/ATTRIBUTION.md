# Mercury — source data and rendering

Mercury combines NASA MESSENGER / USGS measured geography with cinematic
exposure and accelerated rotation. It does not reconstruct the current date.

## Sources and reproduction

Surface: **Mercury MESSENGER MDIS Global Mosaic 250m**, May 2013,
NASA / Johns Hopkins University Applied Physics Laboratory / Carnegie
Institution of Washington / USGS Astrogeology. This monochrome mosaic contains
photographic relief shading and is not calibrated albedo. The natural gray-brown
appearance uses a restrained display tint, not measured true color.
Record: https://astrogeology.usgs.gov/search/map/mercury_messenger_mdis_global_mosaic_250m

Elevation: **Mercury MESSENGER Global DEM 665m V02**, USGS Astrogeology,
K. J. Becker, M. S. Robinson, T. L. Becker, L. A. Weller, K. L. Edmundson,
G. A. Neumann, M. E. Perry and collaborators (2016), *First Global Digital
Elevation Model of Mercury*, 47th Lunar and Planetary Science Conference,
abstract 2959: https://www.hou.usra.edu/meetings/lpsc2016/pdf/2959.pdf
Please cite the product authors and record:
https://astrogeology.usgs.gov/search/map/mercury_messenger_global_dem_665m

The original 61324x30662 uint8 surface spans -180..180 east, north up.
The 23040x11520 int16 DEM spans 0..360 east, north up, with elevations in meters
relative to the source sphere radius 2,439,400 m. Conversion shifts DEM longitude
180 degrees, filters elevations in float32, and derives east/north/outward
normals with latitude-dependent spacing. Longitude derivatives wrap and the
first/last two polar normal rows are flattened. The verified DEM has no nodata.

Run `powershell -File scripts/build-mercury-textures.ps1` with Python 3.11+,
Pillow, numpy and tifffile (project-local `.deps/python` is supported).
The script verifies original SHA-256 hashes and the shared pinned Microsoft
DirectXTex May2026 converter. Originals and intermediates stay in `.deps/mercury`.
`sources.json` records URLs, hashes, projection metadata and conversion versions;
`runtime-hashes.json` records compressed output hashes. Runtime maps are
8192x4096 BC7 sRGB surface and BC7 linear normals with complete mip chains.
Color mips are filtered in linear light; sampled normals are renormalized.
Compression is lossy and can vary with GPU/tool versions.

The stationary sky shares the existing NASA SVS Deep Star Maps 2020 and
Hipparcos/Tycho catalog-star texture without an additional sky allocation.
See [shared sky attribution](../space/ATTRIBUTION.md). Scene textures load on
demand and are released when leaving Mercury. Rendering uses analytic limb
coverage, one HDR sample and no depth target.

## Rendering and controls

Sunlight follows inverse-square irradiance at mean distance 0.38709893 AU,
with a finite solar-disc radius of approximately 0.688 degrees. Mercury has
an extremely thin exosphere: no visible atmosphere or haze shell is rendered.
Reference: https://nssdc.gsfc.nasa.gov/planetary/factsheet/mercuryfact.html?level=1

Particulate regolith uses the existing restrained Lommel-Seeliger/Lambert
mixture and normal-map relief fading on the sunlit side of the terminator.
The night side receives no uniform ambient fill. Normals do not displace the
limb or cast crater shadows. Source photographic shading remains visible;
photometry is approximate. Shared post-processing may produce camera glare.

At Movement 1, rotation completes in 900 seconds, matching Earth/Mars. This
is an accelerated presentation, not Mercury's physical spin/orbit resonance.
The small 0.034-degree obliquity is retained; viewing inclination and camera
drift are artistic. The Sun stays outside the composition.

Controls: Brightness sets exposure; Movement controls spin/time; Terrain relief
scales normals; Camera drift changes framing; Sun angle ranges from 0 to 90
degrees (default 30); Surface contrast adjusts measured surface contrast.
Natural Mercury, Warm stone, Cool gray and Neutral survey palettes preserve
geography. Audio strength adds a subtle exposure response. Movement zero
freezes animation and audio response; application pause freezes rendering.
Eco/Auto/High retain the same texture detail without atmosphere integration.

NASA, USGS and the contributing teams are credited as sources; no endorsement
is implied. Source data and derivatives are separate from the application's
MIT-licensed code. NASA media guidelines:
https://www.nasa.gov/nasa-brand-center/images-and-media/

The abstract metallic World is now **Plastic metal**. Its serialized scene and
capture token is `plastic_metal`; this planet uses `mercury`.
