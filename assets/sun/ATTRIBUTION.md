# Sun â€” NASA/SDO AIA 171 Ã…

Courtesy of NASA/SDO and the AIA, EVE, and HMI science teams.

Observation: **2026-09-24 00:06:10 UTC**, Atmospheric Imaging Assembly,
171 Ã… extreme ultraviolet. The fixed 4096Ã—4096 browse observation is bundled
for offline artistic display, not scientific analysis or a live solar feed.

- Source: https://sdo.gsfc.nasa.gov/assets/img/browse/2026/09/24/20260924_000610_4096_0171.jpg
- Data rights and attribution: https://sdo.gsfc.nasa.gov/data/rules.php
- Coronal-loop reference: https://science.nasa.gov/science-research/heliophysics/nasa-solar-observatory-sees-coronal-loops-flicker-before-big-flares/

SDO images are not copyrighted unless explicitly noted. NASA and its science
teams do not endorse Voltura Worlds. This derivative differs from the original
SDO product: the outer frame and bottom caption are faded away, full mipmaps
are generated, and the image is stored as BC7 sRGB. Exact source checksum,
observation date, geometry and processing are in `sources.json`; compressed
output hashes are in `runtime-hashes.json`.

Reproduce with `powershell -File scripts/build-sun-textures.ps1`. The source
and pinned DirectXTex executable are checksum verified. Python dependencies
are the same NumPy/Pillow stack as the existing planet asset builders.
Original downloads remain in `.deps/sun` and are not shipped.

The 171 Å source is front-facing imagery, not a global spherical map. A second
NASA observation supplies actual prominence emission and irregular, translucent
fine structure: **AIA 304 Å, March 30, 2010**, first-light frame 69.

- NASA source page: https://svs.gsfc.nasa.gov/10610/
- Source image: https://svs.gsfc.nasa.gov/vis/a010000/a010600/a010610/AIA_red_2_full_disk_4Kx3K_00069.jpg
- Prominence reference: https://www.nasa.gov/image-article/what-solar-prominence/
- Additional credit: NASA/Goddard Space Flight Center Scientific Visualization Studio.

This is explicitly an artistic composite of observations from different dates,
not a simultaneous observation. The 304 image is aligned by its observed limb,
masked smoothly from .89 to 1.02 disk radii, and faded at 1.41–1.48 radii. Its
real plasma structures replace generated outline curves entirely. Source hashes
and exact image transforms are recorded in `sources.json`.

The shader animates bounded local image advection, evolving emission cells and
localized brightness variation. Prominence roots stay anchored while the
outer structures drift and breathe. These motions are artistic, not recorded
solar motion or a physical simulation. There is no invented far-side surface
or full globe rotation. All four palettes are false color; the default shifts
the 171 Å disk toward orange and preserves the 304 prominence's warm colors.
Small, observed high-intensity roots receive a restrained warm-white highlight.
There are no artificial white spotlight patches or evenly spaced ellipse loops.

Corona scales the 171 off-limb emission and a restrained surrounding streamer
envelope. Solar activity independently scales observed prominence emission and
surface animation. Eco uses one emission-noise band and Auto/High use two,
without changing the NASA base image or prominence geography.
