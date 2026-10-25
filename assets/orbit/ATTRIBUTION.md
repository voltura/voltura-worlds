# Orbit surface map

`earth-january.jpg`: NASA Blue Marble: Next Generation, January 2004 global base map. Credit: Reto Stöckli, NASA Earth Observatory, Terra/MODIS.

Source: https://science.nasa.gov/earth/earth-observatory/blue-marble-next-generation/base-map/

Original asset: https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/bmng-base/january/world.200401.3x5400x2700.jpg

SHA-256: `99f5faad74efe985fbf1714c8be7296ca9999759a1215b65f99b7f1df278dde5`

NASA media usage guidelines: https://www.nasa.gov/nasa-brand-center/images-and-media/

NASA is credited as the source; no endorsement is implied. This image is separate from the application's MIT-licensed code. The runtime decodes the map to a 4096×2048 sRGB texture. Orbit is an artistic scene with fictional rings, not an accurate Earth simulation.

## Cloud coverage

`earth-clouds-8192.tif`: NASA Blue Marble global cloud mosaic. NASA Goddard Space Flight Center; Reto Stöckli, enhancements by Robert Simmon.

Source record: https://visibleearth.nasa.gov/images/57747/blue-marble-clouds/77558l
Original asset: https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57747/cloud_combined_8192.tif
SHA-256: `d137775d8966ab8d443fd5126dc6e7ad72072bc1ed50555c5818d221735daf0f`

Runtime: derived8192x4096 single-channel BC4 coverage texture with prebuilt mipmaps. The earlier 2048x1024 `earth-clouds.jpg` evaluation copy is not used by the renderer (same NASA source; SHA-256: `daddaad84d7a33bbbc86cdda3f591099f57cee8607b7bcf3b67eb7e4f7a1c793`).

## Night lights

`earth-night-2016.jpg`: NASA Earth at Night, 2016 global grayscale map.
Source: https://science.nasa.gov/earth/earth-observatory/earth-at-night/maps/
Original asset: https://assets.science.nasa.gov/content/dam/science/esd/eo/images/imagerecords/144000/144897/BlackMarble_2016_01deg_gray.jpg
SHA-256: `4d2158f59123dadf0696a1cf8909c45018a1de8d0daab40da04122a5aa7f27c6`
Runtime: 3600x1800 single-channel R8 emission mask, artistically colored in the shader.

The NASA media usage guidelines above apply to these source assets too; no NASA endorsement is implied.


All runtime Orbit maps now include generated mip chains. RGB levels are averaged in linear light; cloud coverage and night masks remain linear single-channel data. Spherical texture sampling uses anisotropic filtering with horizontal wrapping.

## Milky Way background

`milkyway-2020.png`: derived from NASA Scientific Visualization Studio, **Deep Star Maps 2020**, Ernie Wright. The source maps plot Hipparcos-2, Tycho-2 and Gaia DR2 catalog data; this Milky Way variant omits the bright foreground stars. Galactic coordinate map corrected January 4, 2021.

Source record: https://svs.gsfc.nasa.gov/4851/
Original asset: https://svs.gsfc.nasa.gov/vis/a000000/a004800/a004851/milkyway_2020_4k_gal.exr
Original SHA-256: `fad63b36aa632c691d521cd9d8b828de9d29b4c2784276eec44e54c6cb159a49`
Derived PNG SHA-256: `6dbb55798cf64c0e306ebe21b1e57c9dee4a6207a83cf52fe9a732f1fbaaed38`

Conversion: Blender 5.2.2, Standard view transform, no look, exposure 0, gamma 1, 4096x2048 RGB 8-bit PNG. Runtime samples the sRGB map with linear-light mipmaps, artistic orientation and blue/gold tint. NASA media usage guidelines linked above apply; no NASA endorsement is implied. This asset is separate from the MIT-licensed application code.


## Runtime texture derivatives

`runtime/earth-january.dds` and `runtime/milkyway-2020.dds` preserve4096x2048 resolution using BC7 sRGB. `runtime/earth-clouds-8192.dds` preserves8192x4096 coverage using BC4 linear data. All contain complete mip chains. Original source attribution above applies to these compressed derivatives. Night lighting still uses its original JPEG and runtime R8 conversion.

Rebuild with `scripts/build-orbit-textures.ps1`, using Microsoft DirectXTex texconv May2026 (https://github.com/microsoft/DirectXTex/releases/tag/may2026). The script verifies the tool SHA256 before execution. The converter is a development dependency and is not shipped. GPU compression may differ between hardware/tool versions; generated asset hashes are recorded with the validation evidence. Compression is lossy; no pixel-identical claim is made.
