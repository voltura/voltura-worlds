# Voltura Worlds documentation

The current app has 25 built-in Worlds, independent display assignments, Random and Random Solar System, optional body statistics, local audio reaction and silent video playback. Version 0.5.0 includes Tropical Island, Tropical Paradise, Forest Water, optional desktop right-click menus, HDR output, adaptive display performance, video framing and speed controls, and saved desktop presets.

## Using and maintaining the app

- [User guide](USING.md): installation, library, adjustments, playback, video, displays and presets.
- [Desktop presets and `.world` packages](PRESETS.md): persistence, display mapping, bundled media and validation.
- [Body statistics](body-statistics.md): fact conventions, sources and card rendering.
- [HDR output](HDR.md): output selection, fallback and numeric diagnostics.
- [Video implementation](VIDEO-IMPLEMENTATION.md): decoder support, GPU transfer, framing and playback rates.
- [Automatic updates](UPDATES.md): settings, verification, staging and installation.
- [Release checklist](RELEASING.md): visual acceptance, signing, packaging and publication.
- [Website maintenance](PAGES.md): preview, native media, publication and downloads.

## Rendering and sources

- [Bloom](BLOOM.md), [Earth](EARTH.md), [Moon](MOON.md), [Jupiter](JUPITER.md), [Venus](venus.md), [Uranus](uranus.md), [Neptune](neptune.md) and [Sun](sun.md).
- [Artwork and third-party credits](../THIRD-PARTY-NOTICES.md), including the source and rendering notices for Mars, Mercury, Saturn and Pluto.
- [GPU profiling](GPU-PROFILING.md) and [frozen-volume diagnostics](SMOKE-REFERENCE-VOLUMES.md). Diagnostic capture options are separate from library settings.

## Development verification

This repository uses code review, compiler feedback and focused use of the actual app, with the user's visual feedback guiding iteration. Automated tests and their build targets have been removed. See the [release checklist](RELEASING.md).

Optional native captures and profiling tools remain available for deliberate visual review and investigation. Physical HDR appearance, monitor reconnection and sustained desktop performance require observation on suitable hardware.
