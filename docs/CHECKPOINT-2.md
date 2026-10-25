# First native motion checkpoint — 2026-10-23

Checkpoint 1 concept art was approved in chat. Checkpoint 2 was rejected: all three clips are visually far below the approved images. Those images remain the quality target. The first-motion clips are retained as failure evidence, not accepted milestones.

## Evidence

- `artifacts/native/aurora/aurora-motion.mp4`: 1280×720, six seconds, 30 FPS, audio Off.
- `artifacts/native/orbit/orbit-motion.mp4`: same dimensions/duration, actual sphere/ring geometry, audio Off.
- `artifacts/native/ink/ink-motion.mp4`: same dimensions/duration, 96³ compute fluid volume, 20 seconds of simulation warmup, audio Off.
- Corresponding PNG stills and `metrics.json` files accompany each clip.

These first-checkpoint images originate from native D3D12 rendering on Intel Arc, running on a private Windows desktop. PNG readbacks were encoded into MP4 without interpolation. They are not physical-display recordings, real-time presentation benchmarks, or evidence of desktop-icon interaction. These initial renderers did not use concept pixels. Aurora revision 5 subsequently switched to a clean edit of the approved concept as authored runtime artwork; see VISUAL-REBUILD.md for the current implementation and its limitations.

The first readback exposed a WIC RGBA/BGRA negotiation mismatch, now corrected. Orbit ring detail is filtered to reduce aliasing. Smoke has bounded higher-order density transport, pressure projection, vorticity confinement, colored emitters, and volumetric self-shadowing. Its small-scale shading includes procedural density modulation.

## Assessment

Aurora needs further evaluation of composition and luminous detail against the concept. Orbit needs more cinematic lighting and depth. Smoke has visible volume and richer structure but remains softer and less sculptural than the approved direction; it has not passed the separate Music comparison gate.

## Validation

Release compilation and CTest pass. Core tests cover pause policy, bounded resume timestep, invalid parameters, settings persistence, package roundtrip, tampering, traversal, truncated/corrupt PNG previews, and offline staged import.

All three scenes completed 600-frame 1920×1080 offscreen runs with D3D12 debug validation, with no error/corruption messages reported. Reports are under `artifacts/validation/`. Smoke at grid64 measured p95 GPU 7.81 ms and process private memory approximately 104 MiB. Its GPU allocation figure is a resource-size estimate, not measured driver residency. The review clip uses grid96 at 720p and must not be presented as the 1080p configuration.

These short tests do not establish sustained 60 FPS, CPU percentage, endurance, adaptive-quality acceptance, multi-monitor costs, or physical-display correctness. Power mode and refresh rate were not recorded, so they do not meet the full performance acceptance protocol.
