# Third-party notices

- Microsoft Direct3D 12 Agility SDK 1.619.6: Microsoft DirectX license distributed in the NuGet package. D3D12Core.dll is an application-local redistributable. The SDK debug layer is for development and is not included in the installer.
- nlohmann/json 3.12.0: MIT, Copyright (c) 2013-2025 Niels Lohmann. License text is retained in the downloaded header.
- Private desktop test harness adapted from Voltura Markdown, MIT, Copyright (c) 2026 Voltura AB.
- All three concept images were generated with OpenAI's built-in imagegen tool. Aurora now uses a separate clean edit of its concept as runtime artwork; its provenance and full editing prompt are recorded under `assets/aurora/PROVENANCE.md`. Orbit and Ink concepts remain visual references.

Dependency license texts are included under `licenses/` and copied by the build and installer. The bootstrap process downloads pinned versions; version and hashes are recorded in scripts/bootstrap.ps1.
# Artwork

Tropical Paradise uses original generated tropical landscape artwork. Provenance and the full generation prompt are recorded in `assets/tropical-paradise/ATTRIBUTION.md`. Waterfall, pool, mist and foliage motion are rendered locally by the app.

Forest Water uses synthetic woodland artwork generated with OpenAI's built-in image generation tool. Provenance and the generation prompt are recorded in `assets/forest-water/ATTRIBUTION.md`; no footage from the reference video is bundled.

Mars uses NASA Ames/Viking/USGS colorized imagery, NASA MOLA topography and the shared NASA SVS star map. Exact source products, credits, hashes, conversion steps and public-domain notes are in `assets/mars/ATTRIBUTION.md` and `assets/mars/sources.json`.

Orbit's NASA Blue Marble map is separately credited in `assets/orbit/ATTRIBUTION.md` and is not covered by the application code's MIT license. Aurora's generated texture-layer provenance is in `assets/aurora/PROVENANCE.md`.

- Automatic updater and its policy tests adapted from Voltura Markdown, MIT, Copyright (c) 2026 Voltura AB. Worlds uses installer updates, its own release feed and cache, and its own opt-in preference.

Earth uses NASA Blue Marble January 2004 surface imagery, the shared NASA cloud/sky maps, NASA Black Marble 2016 night lights, and public-domain Natural Earth v5.1.2 ocean/lake geometry. Exact credits, source hashes, reproduction and rendering approximations are documented in `assets/earth/ATTRIBUTION.md` and `assets/earth/sources.json`.

Moon uses NASA SVS December 2025 LROC imagery, LOLA elevation and compact derivatives of the existing NASA Earth/cloud/night imagery, plus the shared NASA sky. Exact sources, hashes, credits, reproduction and rendering limits are in `assets/moon/ATTRIBUTION.md` and `assets/moon/sources.json`.

Jupiter uses NASA/ESA Hubble OPAL Cycle 32 (December 2025) color, F631N and FQ889N maps, credited to NASA, ESA, A. A. Simon and M. H. Wong, under CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/). Color resampling, polar fill, methane/continuum proxy derivation and estimated wind processing are documented in `assets/jupiter/ATTRIBUTION.md`; exact source and output hashes accompany it. OPAL dataset DOI: https://doi.org/10.17909/T9G593.

Earth, Mars, Moon and Jupiter share a NASA SVS Deep Star Maps 2020 derivative with Hipparcos/Tycho foreground emission packed into alpha. Source catalogs, credits, conversion and hashes are in `assets/space/ATTRIBUTION.md` and its adjacent manifests.
