# Pluto sources and rendering

Pluto was discovered in 1930 and reclassified as a dwarf planet in 2006. It is smaller than Earth's Moon and has five known moons: Charon, Nix, Hydra, Kerberos and Styx. Its orbit takes approximately 248 Earth years. The bright heart-shaped Tombaugh Regio includes Sputnik Planitia, a large nitrogen-ice glacier.

## Imagery and credit

- [NASA/JPL PIA11707 Pluto Color Map](https://www.jpl.nasa.gov/images/pia11707-pluto-color-map/), published January 19, 2017, from New Horizons Ralph/MVIC July 2015 observations.
- Credit: NASA / Johns Hopkins University Applied Physics Laboratory / Southwest Research Institute.
- [NASA Pluto facts](https://science.nasa.gov/dwarf-planets/pluto/facts/).
- Stars reuse the shared NASA assets; see [space attribution](../space/ATTRIBUTION.md).

Run `scripts/build-pluto-textures.ps1` to reproduce the runtime map. The source JPEG is hash-verified, minimally reduced from 5926x2963 to 5920x2960 for BC7 block alignment, interpreted as sRGB and compressed with a complete mip chain. See sources.json and runtime-hashes.json. Source downloads remain in .deps/pluto and do not ship.

## Rendering limits

The published photographic color mosaic is not calibrated albedo, a uniform-resolution survey or an elevation map. Encounter-side terrain is sharper than the far side. The unobserved southern cap uses a synthetic photographic continuation: observed rows are reflected across the coverage boundary at native pixel scale. This retains nearby source texture clarity without blurring any observed imagery. Reflected features are reconstructed artwork, not measured polar geography. Only the final 16 texture rows converge to a mean ice color to avoid a polar pinwheel. Source shadows and mosaicking artifacts remain; no normals are inferred from image brightness.

The opening view faces the map's heart region from 25 degrees north. The globe rotates in 15 minutes at Movement 1, greatly accelerated from Pluto's roughly 153-hour day; this is not its 248-year solar orbit. Framing and lighting are artistic rather than a dated astronomical view. Exposure compensation keeps the surface readable despite weak distant sunlight. The narrow blue limb suggests Pluto's haze without simulating atmospheric scattering.

Surface detail adjusts photographic contrast; Camera drift changes framing; Sun angle changes illumination; Atmosphere scales the thin haze. Palettes are Natural Pluto (published source colors), Warm ice, Cool ice and Neutral survey. Optional audio gently affects globe/haze exposure, while stars stay stationary. Movement zero freezes all animation and audio response. Pluto's moons are documented but not rendered.
