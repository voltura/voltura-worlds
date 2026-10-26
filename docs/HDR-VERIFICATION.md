# HDR implementation verification

Verified 2026-10-26 on Intel Arc Graphics. The normal Release build is `build/Release/VolturaWorlds.exe`. No commit, installer, release, publication or deployment was made. Existing per-display performance and GPU-sharing work was preserved.

## Automated results

- All 47 registered CTest checks have passing results. The full sweep initially passed 46 and exposed an Output dropdown notification-filter bug. After fixing that handler, the five affected core/UI/performance/shared-GPU/HDR checks passed together. The final HDR check also explicitly requires the D3D12 debug validation interface and passed.
- All 22 built-in World SDR captures retain exactly the SHA-256 PNG hashes recorded from the original current checkout before rendering changes. Final captures use the updated normal Release executable, fixed 320x180 dimensions, 12 deterministic frames and D3D12 validation.
- Settings migration, persistence, invalid preferences, Apply to all, selected-display Reset, import preservation and exclusion from portable packages passed.
- Active HDR versus SDR ACM, missing data, clone veto/minimum luminance limits, reference-white scale/fallbacks, video override, rollback and retry were covered by output-resolution/transition tests.
- GPU FP16 readbacks matched an independent double-precision oracle for ordinary and Orbit tone mapping, neutral and chromatic ramps, black, reference-white changes, peak bounds and highlight continuity. Synthetic SDR-card sRGB decoding, alpha blending and white scaling passed.
- Mixed-format renderers retain shared Earth texture identity and independent mutable targets/PSOs. Repeated output changes, resize, video/World changes, pause/resume, independent 24/30/60 deadlines, actual test-device removal and device recovery passed. Repeated unchanged configuration does not rebuild output; transitions retain no extra mutable format history.
- `git diff --check` passed. The native UI was inspected from a private test-desktop capture.

Maximum absolute GPU/oracle error: **0.00389774 scRGB units**. Neutral peak output reached **5.0**, with synthetic W=203/P=400 nits. These are numeric output values, not measured panel luminance.

## Memory and short GPU timings

Paired 1920x1080 native offscreen captures, 120 frames each, 109 GPU timing samples after 11 warm-up frames. HDR diagnostics use W=203/P=400 nits. Sun includes the actual body-statistics overlay in both modes. Each row is one independent process; GPU allocation totals include scene resources and artwork, and process-private memory is sampled after capture.

| World | Estimated GPU MiB, SDR / HDR | GPU p95 ms, SDR / HDR | Process private MiB, SDR / HDR | HDR maximum scRGB | Pixels above reference white |
| --- | ---: | ---: | ---: | ---: | ---: |
| Aurora | 42.99 / 58.81 | 2.063 / 2.309 | 249.98 / 259.30 | 4.98828 | 201,519 |
| Orbit | 149.31 / 165.13 | 6.195 / 6.377 | 362.82 / 370.16 | 5.00000 | 1,194 |
| Sun | 82.12 / 97.94 | 1.940 / 1.998 | 286.38 / 293.53 | 4.96094 | 85,929 |

FP16 adds **16,588,800 bytes (15.82 MiB)** for two 1080p back buffers per display. At 320x180, the HDR contract test measured mutable allocation accounting changing from 979,200 to 1,440,000 bytes, returning to the SDR value after repeated transitions. The shared-GPU test counted the same 143,654,912 bytes of Earth artwork once for two displays, then zero after both displays closed.

The timing comparisons are short single offscreen runs, not a stable performance-regression estimate or physical presentation measurement. All HDR diagnostic RGB samples were finite, nonnegative and at most 5.0 scRGB (the configured P/80). Raw files contain tightly packed RGBA16F and separate metadata; no HDR bytes were passed to the PNG encoder. Invalid diagnostic luminance values and PNG recording with HDR were rejected.

## Physical acceptance still open

Discovery observed one display with **Windows HDR off**. No Windows HDR setting, monitor setting or color profile was changed. Physical HDR presentation behind icons, actual HDR highlight/gradient/card brightness, mixed physical HDR/SDR monitors, live HDR toggles, sleep/resume, Explorer restart, cable reconnection and ten minutes of HDR desktop playback were not verified. Synthetic FP16 targets, private desktops and PNGs do not establish those results.

The updated normal application is relaunched after verification because it was running before the work. Its normal Auto behavior currently resolves to SDR on the detected display.

## Evidence

- [Full CTest sweep](../artifacts/hdr/ctest.log), [fixed UI and related checks](../artifacts/hdr/final-focused-tests.log), [final HDR validation](../artifacts/hdr/final-hdr-validation.log).
- [Release build log](../artifacts/hdr/final-build.log), [final SDR comparisons](../artifacts/hdr/final-sdr-check.log).
- [Machine-readable combined results and binary hash](../artifacts/hdr/verification.json), [GPU oracle results](../artifacts/hdr/contracts/results.json).
- [1080p memory/timing/readback results](../artifacts/hdr/diagnostics/results.json), [diagnostic CLI checks](../artifacts/hdr/diagnostics.log).
- [Native UI on a private test desktop](../artifacts/preview-0.1.4/ui/performance-status.bmp). Its display entries are test fixtures.

Reproduction and raw file format are documented in [HDR output](HDR.md).
