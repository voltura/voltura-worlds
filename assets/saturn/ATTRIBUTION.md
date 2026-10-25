# Saturn assets

Saturn colour map: Solar System Scope / INOVE, https://www.solarsystemscope.com/textures/
Original: https://www.solarsystemscope.com/textures/download/8k_saturn.jpg
License: Creative Commons Attribution 4.0 International, https://creativecommons.org/licenses/by/4.0/

This 4096 x 2048 (despite the upstream 8k filename) map is an authored reconstruction based on imagery, with tuned colours and filled gaps; it is not a calibrated observational albedo map. Voltura converts it to mipmapped BC7 sRGB DDS. The shader adds illumination, subtle bounded cloud advection, contrast adjustment, atmospheric haze and optional colour grading. The northern polar map is blended toward its filtered mip level to soften the authored boundary while retaining Saturn's real hexagonal jet-stream feature. No terrain elevation is inferred from colour.

Saturn shape and ring boundaries follow NASA fact sheets:
- https://nssdc.gsfc.nasa.gov/planetary/factsheet/saturnfact.html
- https://nssdc.gsfc.nasa.gov/planetary/factsheet/satringfact.html

The radial ring optical-depth profile and filtered fine structure are generated in HLSL. Optical depths and scattering colours are representative artistic approximations. C/B/A boundaries, Cassini Division, Encke gap and faint F ring use physical radial proportions. No resolved boulders or moons are rendered. Camera and 900-second motion are cinematic, not a live ephemeris. Shared stars retain assets/space attribution.

Rebuild with `powershell -File scripts/build-saturn-textures.ps1`. Source JPEG remains in .deps/saturn; runtime DDS is shipped. Source and runtime hashes are recorded alongside this document. These image assets are separate from the application's MIT-licensed code. No endorsement is implied.
