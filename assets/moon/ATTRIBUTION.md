# Moon — source data and rendering

The Moon World combines measured geography with cinematic exposure and an
accelerated camera tour. It does not reconstruct the current date.

## NASA sources

NASA Goddard Scientific Visualization Studio, **CGI Moon Kit**, Ernie Wright
(USRA), scientist Noah Petro (NASA/GSFC), LRO / LROC camera team (Arizona State
University), and LOLA instrument team. Record: https://svs.gsfc.nasa.gov/4720/

Color input: **December 2025** `lroc_color_16bit_srgb_8k.tif`, 8192×4096 RGB16
sRGB, adapted from the Hapke-normalized WAC mosaic. NASA adjusted exposure
and white balance, inpainted small dropouts, and filled missing polar color
with lower-resolution monochromatic LOLA albedo. This is a visualization map,
not a calibrated scientific color measurement.

Elevation input: `ldem_64_uint.tif`, 23040×11520, 64 pixels/degree LOLA grid
repackaged by SVS. Unsigned samples represent half meters above radius
1,727,400 m. Conversion uses `sample * 0.5 - 10000` meters relative to the
1,737,400 m mean sphere. Both maps are centered on 0° longitude, north up,
positive east; no longitude shift is applied.

The compact Earth uses NASA Blue Marble January 2004 (Reto Stöckli, NASA Earth
Observatory / Terra MODIS), NASA Blue Marble clouds (NASA GSFC, Reto Stöckli,
Robert Simmon), and Black Marble 2016 night lights. See the existing
[Earth](../earth/ATTRIBUTION.md) and [Orbit](../orbit/ATTRIBUTION.md) credits.
RGB is a linear-light cloud/surface composite encoded as sRGB; linear alpha
stores cloud-attenuated night emission. Clouds and Earth orientation are
static; no full-size Earth textures or atmosphere integration are loaded.

The sky shares NASA SVS **Deep Star Maps 2020**, Ernie Wright, Milky Way
derivative, with artistic orientation and exposure. Its camera rays follow
the tour. Source and conversion details are in Orbit's credits.

Exact input URLs, hashes and conversion versions are in `sources.json`;
compressed output hashes are in `runtime-hashes.json`. NASA usage guidelines:
https://www.nasa.gov/nasa-brand-center/images-and-media/
NASA and contributors are credited as sources; no endorsement is implied.
These assets are separate from the application's MIT-licensed code.

## Reproduction and efficiency

Run `powershell -File scripts/build-moon-textures.ps1` with Python 3.12+, Pillow,
numpy, tifffile and imagecodecs (needed for the LZW RGB16 TIFF). The local
`.deps/python` environment is supported; install the pinned tools with
`python -m pip install --target .deps/python -r requirements-textures.txt`.
The script checks source hashes and
uses the hash-verified Microsoft DirectXTex May2026 converter shared with
Mars/Earth. Downloads and PNG intermediates stay in `.deps/moon` and
`.deps/earth`; they are not shipped.

RGB16 precision is retained until ordered-dither quantization to the BC7
input. Elevations are filtered in float32, then converted to physical east /
north / outward normals using lunar radius and latitude-dependent spacing.
Longitude derivatives wrap; the first/last two polar rows are flattened.
Runtime maps are 8192×4096 BC7 sRGB color, 8192×4096 BC7 linear normals and
2048×1024 BC7 sRGB/linear-alpha Earth, with complete mip chains. Color mipmaps
are filtered in linear light; sampled normals are renormalized. BC7 is lossy
and output may vary with hardware/tool versions.

The renderer uses analytic silhouette coverage, one HDR sample, no depth
target, and the existing bounded DDS uploader. Scene textures are loaded on
demand and released when leaving Moon.

## Geometry and rendering limits

The Moon's near side remains directed toward Earth. A camera at three lunar
radii orbits every 900 seconds at Movement 1, starting at longitude 0°.
Camera drift adds a small periodic latitude/framing excursion. The entire
composition repeats each tour. This simplified equatorial camera path omits
ephemerides, libration and the system's motion around the Sun.

Earth's center is 384,400 km away; its radius is 6,371 km. It starts behind
the camera, enters the frame briefly on either side of the far-side portion,
and passes behind the Moon. Visibility depends on viewport aspect. Its scale
and position are not enlarged for composition. Perspective ray/sphere
intersections determine silhouettes and front-to-back occlusion.

One fixed distant Sun lights both bodies, initially 60° from the near-side
axis, with approximately 0.533° solar diameter at 1 AU. Lunar diffuse shading
uses a restrained Lommel–Seeliger/Lambert mixture; relief fades near the solar
horizon to avoid a false bright contour. Normals do not displace the limb or
cast crater shadows. There is no lunar atmosphere or uniform night fill;
shared bloom may create camera glare. Earthshine, mutual eclipses, opposition
surge and calibrated photometry are not simulated.

Controls: Brightness adjusts exposure; Movement advances the tour; Terrain
relief scales normals; Camera drift adjusts framing/latitude; Sun angle sets
the fixed azimuth from 30° to 90° (default 60°); Earth brightness scales
Earth's light. Four palettes provide subtle natural grades. Audio strength
scales a small exposure response. Movement zero freezes time/audio; normal
application pause freezes rendering. Eco/Auto/High retain the same lunar
asset detail: there is no iterative atmosphere or simulation to downsample.

## Shared catalog-star update

The planet sky now includes the NASA Hipparcos/Tycho foreground in the existing BC7 sky texture�s unused alpha. It replaces the earlier faint procedural stars (Earth/Mars) or Milky-Way-only sky (Moon/Jupiter), with no extra GPU texture allocation. See [shared sky sources and conversion](../space/ATTRIBUTION.md).
