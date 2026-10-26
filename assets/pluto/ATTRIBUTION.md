# Pluto sources and rendering

Pluto was discovered in 1930 and reclassified as a dwarf planet in 2006. It is smaller than Earth's Moon and has five known moons: Charon, Nix, Hydra, Kerberos and Styx. Its orbit takes approximately 248 Earth years. The bright heart-shaped Tombaugh Regio includes Sputnik Planitia, a large nitrogen-ice glacier.

## Imagery and credit

- [NASA/JPL PIA11707 Pluto Color Map](https://www.jpl.nasa.gov/images/pia11707-pluto-color-map/), published January 19, 2017, from New Horizons Ralph/MVIC July 2015 observations.
- Credit: NASA / Johns Hopkins University Applied Physics Laboratory / Southwest Research Institute.
- [NASA Pluto facts](https://science.nasa.gov/dwarf-planets/pluto/facts/).
- Stars reuse the shared NASA assets; see [space attribution](../space/ATTRIBUTION.md).

Run `scripts/build-pluto-textures.ps1` to reproduce the runtime map. The source JPEG and checked-in reconstruction are hash-verified, composited at 5920x2960 for BC7 block alignment, interpreted as sRGB and compressed with a complete mip chain. The NASA portion is minimally reduced from 5926x2963; the 1774x887 generated reconstruction is resampled to the runtime grid. See sources.json and runtime-hashes.json. Source downloads remain in .deps/pluto and do not ship.

## Rendering limits

The published photographic color mosaic is not calibrated albedo or an elevation map. Its encounter side is much sharper than the far side. Version 0.1.9 retains the original high-resolution heart/encounter imagery and uses a built-in imagegen artistic reconstruction for the under-resolved far side and unobserved southern cap. This resolves plausible craters and ice geology at comparable visual clarity while retaining the broad color/geographical layout. These reconstructed features are artwork, not new spacecraft observations or a measured terrain model.

The old reflected southern fill copied the source's dark coverage boundary into a visible line. It has been replaced completely. A feathered composite excludes that source edge; horizontal endpoint correction closes the longitude wrap. Eight rows at each pole converge to a mean color to prevent a pinwheel. No surface blur or synthetic elevation/normal map is applied. The checked-in reconstruction, generation prompt and hashes make the asset reproducible without another image-generation call.

The opening view faces the map's heart region from 25 degrees north. The globe rotates in 15 minutes at Movement 1, greatly accelerated from Pluto's roughly 153-hour day; this is not its 248-year solar orbit. Framing and lighting are artistic rather than a dated astronomical view. Default sunlight sits approximately 18 degrees from the viewing direction, 8 degrees above it, so the round silhouette stays readable against space with gentle side shading. Sun angle retains a near-frontal through stronger side-lit range. Exposure compensation keeps the surface readable despite weak distant sunlight. The narrow blue limb suggests Pluto's haze without simulating atmospheric scattering.

Surface detail adjusts photographic contrast; Camera drift changes framing; Sun angle changes illumination; Atmosphere scales the thin haze. Palettes are Natural Pluto (published source colors), Warm ice, Cool ice and Neutral survey. Optional audio gently affects globe/haze exposure, while stars stay stationary. Movement zero freezes all animation and audio response. Pluto's moons are documented but not rendered.
