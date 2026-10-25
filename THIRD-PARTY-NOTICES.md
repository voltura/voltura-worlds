# Third-party notices

- Microsoft Direct3D 12 Agility SDK 1.619.6: Microsoft DirectX license distributed in the NuGet package. D3D12Core.dll is an application-local redistributable. The SDK debug layer is for development and is not included in the installer.
- nlohmann/json 3.12.0: MIT, Copyright (c) 2013-2025 Niels Lohmann. License text is retained in the downloaded header.
- Private desktop test harness adapted from Voltura Markdown, MIT, Copyright (c) 2026 Voltura AB.
- All three concept images were generated with OpenAI's built-in imagegen tool. Aurora now uses a separate clean edit of its concept as runtime artwork; its provenance and full editing prompt are recorded under `assets/aurora/PROVENANCE.md`. Orbit and Ink concepts remain visual references.

Dependency license texts are included under `licenses/` and copied by the build and installer. The bootstrap process downloads pinned versions; version and hashes are recorded in scripts/bootstrap.ps1.
# Artwork

Orbit's NASA Blue Marble map is separately credited in `assets/orbit/ATTRIBUTION.md` and is not covered by the application code's MIT license. Aurora's generated texture-layer provenance is in `assets/aurora/PROVENANCE.md`.

- Automatic updater and its policy tests adapted from Voltura Markdown, MIT, Copyright (c) 2026 Voltura AB. Worlds uses installer updates, its own release feed and cache, and its own opt-in preference.
