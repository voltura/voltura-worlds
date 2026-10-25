# Mars surface and terrain

The Mars World combines measured geography with cinematic exposure and accelerated rotation. It is not a reconstruction of the current date or a calibrated scientific radiometric simulation.

## Surface color

NASA Ames / NASA Viking Orbiter / USGS Astrogeology Science Center, **Mars Viking Colorized Global Mosaic (MDIM 2.1)**. The colorization is artistic; it is not a true-color spectrophotometric albedo measurement. The source also contains residual photographic relief shading, so the runtime normal lighting is deliberately restrained.

Record: https://astrogeology.usgs.gov/search/map/mars_viking_colorized_global_mosaic_232m

The downloadable 1 km global JPEG linked on that record is the conversion input. This supplies more than sufficient source resolution for the 8192x4096 runtime map without downloading the 12 GB master. Longitude is -180 to +180 degrees, positive east, with north at the top. USGS lists this product as public domain, with no use constraints.

## Terrain normals

NASA Goddard / MOLA Science Team / Mars Global Surveyor, **MOLA Mission Experiment Gridded Data Record**, 32 samples per degree, `megt90n000fb` (IAU2000, meters above the GMM3 areoid).

Archive overview: https://pds-geosciences.wustl.edu/missions/mgs/megdr.html

Product: https://pds-geosciences.wustl.edu/mgs/urn-nasa-pds-mgs_mola_topography_derived/meg032/megt90n000fb.xml

This is the original global gridded MOLA topography product, rather than the larger USGS 463 m resampling cited during planning. Its 11520x5760 measured grid supplies the 8192x4096 runtime normal map. Conversion reads signed big-endian 16-bit elevations, shifts 0..360 to -180..180 longitude, and computes physical east/north slopes using a 3396 km radius and latitude-dependent longitudinal spacing. The shader applies adjustable relief strength. Polar east/west derivatives are tapered to avoid coordinate singularities.

## Space background

The existing NASA SVS **Deep Star Maps 2020**, Ernie Wright, Milky Way derivative is shared with Orbit. See `../orbit/ATTRIBUTION.md` for its source, credits and conversion. Mars uses low-intensity neutral exposure and additional sparse stationary stars.

## Conversion and runtime

Run `python scripts/build-mars-textures.py` and `powershell -File scripts/build-mars-textures.ps1` (the PowerShell entry point also runs the Python step). Python requires Pillow and numpy; project-local `.deps/python` is supported. The pinned Microsoft DirectXTex May2026 executable is verified by SHA-256 before use. Source downloads and intermediate PNGs stay under `.deps/mars` and do not ship.

`sources.json` records exact input URLs, SHA-256 hashes, coordinate conventions and conversion versions. `runtime-hashes.json` records compressed output hashes. Runtime files contain complete 14-level mip chains: 8192x4096 BC7 sRGB color, and 8192x4096 BC7 linear terrain normals. Color mips are filtered in linear light; normals are renormalized after sampling. Compression is lossy and may vary across GPU/tool versions.

Sunlight uses mean distance 1.524 AU, inverse-square irradiance 0.4306 relative to Earth, and a 0.35 degree solar angular diameter for the terminator transition. The Sun is outside the frame in this composition. Rotation is eastward, accelerated to 900 seconds per turn at Movement 1; camera drift is artistic. The atmospheric integration uses an approximately 11 km density scale height, a thin dusty shell, and restrained color scattering. It is an approximation rather than a full spectral atmosphere model.

The terminator uses finite solar-disc visibility and a Lommel-Seeliger/Lambert mixture for particulate regolith, following the photometric model family documented by USGS ISIS (https://isis.astrogeology.usgs.gov/dev/Application/presentation/Tabbed/photemplate/photemplate.html). Both direct surface light and atmospheric scattering include attenuation along the solar path through a curvature-aware exponential atmosphere. Aerosol scattering depends on the Sun/view angle. Normal-mapped direct-light relief tapers on the sunlit side near the solar horizon so independently shaded facets do not produce a finite-brightness contour at the spherical visibility cutoff. This does not broaden atmospheric twilight or move the outer silhouette. There is no uniform night-side surface light or manually colored dusk band. These remain rendering approximations; the source mosaic is not a calibrated albedo map.

Settings: Brightness controls exposure; Movement controls the scene clock; Terrain relief controls MOLA normal strength; Camera drift controls framing/attitude drift; Dust variation controls suspended-dust contrast; Atmosphere controls optical density. Palettes provide subtle natural white-balance grades. Audio modes and strength gently modulate atmospheric scattering; Movement 0 freezes audio response too. Pause freezes the normal application scene clock.

NASA media guidelines: https://www.nasa.gov/nasa-brand-center/images-and-media/

NASA, USGS and the contributing teams are credited as sources; no endorsement is implied. These data and derivatives are separate from the application's MIT-licensed code.
