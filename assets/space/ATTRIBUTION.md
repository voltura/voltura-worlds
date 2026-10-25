# Shared planet sky and catalog stars

Earth, Mars, Moon and Jupiter use NASA/Goddard Scientific Visualization
Studio **Deep Star Maps 2020**, Ernie Wright (USRA). Source record:
https://svs.gsfc.nasa.gov/4851/

The bright foreground is `hiptyc_2020_4k_gal.exr`, based on the Hipparcos-2
and Tycho-2 catalogs. The faint Milky Way background is the already credited
`milkyway_2020_4k_gal.exr` derivative described in
[Orbit's attribution](../orbit/ATTRIBUTION.md), including Gaia DR2 data
(ESA/Gaia/DPAC). Both use NASA's corrected January 2021 galactic coordinates.
No constellation lines or invented star positions are added.

`runtime/planet-sky.dds` is one 4096×2048 BC7 sRGB texture with a full mip
chain. RGB retains the existing Milky Way PNG. Independent **linear alpha**
contains the mean positive RGB intensity of the NASA bright-star foreground;
it is emission data, not transparency. Foreground stars are rendered neutral
at a restrained artistic exposure. This drops individual stellar color but
retains their mapped positions and relative source intensities. The sky is
not oriented to a current-date astronomical ephemeris.

Replacing the unused alpha of the former opaque BC7 sky costs no additional
GPU texture allocation. Only one sky texture is resident: Orbit retains its
existing star-free Milky Way map; the four planet Worlds load this derivative
instead. Alpha is filtered separately from RGB, avoiding premultiplication
or loss of diffuse color where foreground emission is zero. BC7 and 8-bit
quantization are lossy. A smooth low-intensity gate suppresses the BC7 alpha
compression floor and the faintest stars, avoiding gray background blocks. Source bright
star profiles, mipmaps and footprint filtering stabilize motion without
time-dependent twinkle or audio-driven star motion.

Earth, Mars and Jupiter retain a fixed sky while their globes rotate. Moon
samples the same map using its orbiting camera rays, so its stars move with
the camera. The fixed-view skies also use perspective directions, avoiding
vertical stretching of stars from the 2:1 panorama. Opaque bodies hide stars on both their day and night sides.

Reproduce with `powershell -File scripts/build-space-textures.ps1`, using
Python 3.12+ and `requirements-textures.txt` (or the project's `.deps/python`).
The script checks the NASA EXR and existing background PNG hashes, then uses
the hash-verified DirectXTex May2026 tool. Downloads/intermediates stay in
`.deps/space` and are not shipped. Exact URLs, hashes, versions and conversion
are in `sources.json`; output hashes are in `runtime-hashes.json`.

NASA media guidelines: https://www.nasa.gov/nasa-brand-center/images-and-media/
NASA and the catalog contributors are credited as sources; no endorsement
is implied. These assets are separate from the application's MIT code.
