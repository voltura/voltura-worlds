# Implementation and visual checkpoints

This file records delivered behavior and outstanding acceptance work. It is not a release claim.

- Checkpoint 1: Aurora / Orbit / Ink concept art approved by the user in chat on 2026-10-23.
- Checkpoint 2: REJECTED. All three native clips fall far below the approved concepts: crude, dull, and insufficiently cinematic. The concepts remain the target. Visual treatment is being rebuilt; no new acceptance request yet.
- Checkpoint 3: Ink versus Music comparison pending.
- Checkpoint 4: audio response review pending.
- Checkpoint 5: installed desktop review pending.

## Required delivery

- Native C++20 / D3D12 / HLSL desktop renderer and tray/library.
- Three scenes: Aurora (2D), Orbit (geometry), Ink (3D compute fluid).
- Audio off by default; WASAPI loopback with local analysis.
- Per-monitor choices, auto quality, pause policy, recovery.
- MP4 H.264 import and GPU video playback.
- Validated versioned data-only World packages and import preview.
- PHP/MySQL community with verified accounts and moderation.
- Per-user installer and standard-account validation.
- Measured performance, endurance and physical-desktop verification.

Approved concept images in concepts/ are generated art-direction mockups, not runtime screenshots. Native visual work must be reviewed before claiming it matches them.

## Implementation snapshot

Implemented and built: three D3D12 scenes, authored-artwork Aurora candidate, textured geometry Orbit, compute-fluid Ink prototype, HDR/bloom pipeline, native tray/library, monitor enumeration, desktop attachment, pause policy, WASAPI analysis, settings, strict stored-ZIP World package reader/writer/staging, native capture harness, and PIX capture script. Aurora revision 4 was rejected for milky color, almost imperceptible motion and pixelation. Revision 5 derives its artwork directly from the approved concept, with desktop UI removed, and replaces the silk assets and animation shader. It remains unaccepted; see VISUAL-REBUILD.md for evidence and limitations.

Validated: Release build, core-contract tests, native offscreen captures with D3D12 debug-error checks, 18 GPU scene-switch/resize cycles, and a saved PIX GPU frame. No full product acceptance claim. Detailed PIX replay is blocked by Windows Developer Mode being off.

Outstanding: shared D3D12 system across monitors (currently separate devices), complete adaptive quality, device-loss recovery, remaining export/library UI and preview polish, PHP/MySQL community site, signing, audio demonstration, physical desktop interaction, multi-monitor and endurance validation. Media Foundation video playback and import UI now have passing native tests; the earlier placeholder description is obsolete. Revision93 completed non-elevated install/upgrade/uninstall, packaged video and synthetic-host recovery checks (artifacts/revision-93/installer-results.json). That installer is an unsigned development build, not a release candidate or validation of later renderer experiments.

Aurora revision 7 separates starless moving artwork from a fixed procedural sky. Native star-layer regression passes with zero changed pixels while time, motion, palette and audio inputs vary. See VISUAL-REBUILD.md for capture evidence and scope.

Revision 9 remains in progress: experimental procedural Aurora is not accepted or enabled by default; Orbit has higher-resolution NASA cloud/night textures and revised lighting/composition. Ink is unchanged. The approved concepts remain unmet. See VISUAL-REBUILD.md for current captures and bounded measurements.

Revision 10: Orbit uses mipmapped anisotropic maps and adds three lit geometry moons. Ink now has actual limited forward/reverse density correction, reduced excessive vorticity, and revised volume integration. Native recordings exist, but neither scene meets the approved artwork; Ink is still soft and exceeds the GPU target in the tested 1080p/96-cubed capture. Aurora's unresolved fabric appearance remains. See VISUAL-REBUILD.md.

Revision 13 replaces Aurora's rejected raster-warp renderer with native procedural wisps and independent curved flows. The ordinary runtime now uses this path; it is no longer a capture-only experiment. Fixed stars remain unchanged and verified. The visual target remains unmet; see VISUAL-REBUILD.md for captures and limitations.

Revision 16: Orbit closer framing, revised cloud/ocean lighting, ring shadows, corrected atmosphere/ring blend order, and Orbit-only filmic mapping. Release build, core_contracts and native debug capture pass. Current evidence is revision-16/balanced. All three approved visual targets remain unmet; the visual goal remains active. See VISUAL-REBUILD.md for limitations and measured resource use.

Revision 17 sharpens Aurora's procedural wisps while retaining the organic curved layout. Native 1080p capture and a four-second recording exist; the stationary-star regression passes. Visual fidelity remains below the approved concept. See VISUAL-REBUILD.md.

Revision 18 corrects Ink's pressure-operator consistency. Native debug captures at 12 and 60 simulated seconds pass. Rejected velocity and source experiments are archived, not active. Ink's approved visual target remains unmet.

Revision 19 increases Ink's optical depth and makes camera/shadow density cutoffs consistent. Grid192 diagnostic proves brute-force resolution is too expensive and insufficient visually; ordinary quality presets are unchanged. All visual acceptance gates remain open.

Revision 20 enriches Orbit's dust/star background and adds irregular foreground geometry. Native 1080p debug capture passes; GPU p95 is 7.94 ms and private memory remains above target. Approved artwork remains the unmet target.

Revision 21 introduces elevated Orbit cloud geometry and sun-directed surface shadows. Native debug/lifecycle capture passes. GPU timing exceeds the 8 ms target; visual acceptance remains open.

Revision 22 changes Aurora to three distinct flow groups with finer wisps and localized gold highlights. Native debug capture and fixed-star regression pass. The visual goal remains unmet.

Revision 23 adds restrained Aurora translucency and warm gas particles, with rotated noise octaves. Native capture and fixed-star regression pass. Reference fidelity and full motion acceptance remain incomplete.

Revision 24 adds gentler localized Aurora curls and fixes elongated gas-particle highlights. Native 4K debug capture and a pixel-preserving crop were inspected. Full-resolution 4K performance and artistic fidelity remain below acceptance.

Revision 25 improves Aurora noise evaluation and filters finer detail; Orbit textures now release on scene exit. Release build, core tests and native lifecycle/debug capture pass. Final Aurora GPU p95 is 5.96 ms at 1080p and 17.76 ms at 4K. Lifecycle private memory remains above target. The approved visual references remain unmet; goal stays active.

Revision 26 changes Aurora from bright noise contours to tapered translucent density sheets. Native capture and fixed-star regression pass; 1080p GPU p95 6.21 ms. The result is still below approved artistic fidelity; no visual gate is accepted.

Revision 28 warms Orbit atmospheric/surface rim lighting and refines cloud self-shadowing. Native debug capture passes; target fidelity and performance remain unmet. See VISUAL-REBUILD.md.

Revision 29 replaces blocky moon relief with analytic crater normals and varied crater radii. Pixel-preserving native close-ups verify the shading improvement. Overall visual acceptance remains open.

Revision 32 uses midpoint trajectory integration for Ink transport and its reverse correction. Native 60-simulated-second debug run passes. Fine detail and full reference fidelity remain unresolved.

Revision 34 implements separate density and velocity grids for Ink, with bounded high-resolution density, correct resource bounds and actual fluid GPU-allocation reporting. Core and native lifecycle/debug checks pass. Visual target and performance acceptance remain unmet.

Revision 40 brings real source-map storms into Orbit's visible hemisphere with a rigid spherical cloud transform and seam-aware gradients. Native debug capture passes; overall visual/performance acceptance remains open.

Revision 41 broadens Orbit sunrise illumination. Native debug capture inspected; p95 GPU8.54 ms and private323.3 MiB remain above targets. Aurora fabric-like structure and Ink fine curls remain unresolved. No visual checkpoint accepted; goal active. See VISUAL-REBUILD.md.

Revision 42 improves Aurora curve composition by fanning layers around focal regions. Native debug capture and fixed-star regression pass. Higher-frequency cellular density experiment rejected. Fabric-like material remains unresolved; approved visual target is not met.

Revision 43 sharpens Aurora edges and reduces uniform glow. Native capture/recording passes; finer material structure and approved visual fidelity remain incomplete. See VISUAL-REBUILD.md.

Revision 44 corrects Aurora texture coordinates along curved flows and removes a confirmed cusp in the ribbon-width taper. Native 4K before/after crops verify continuous curves at the focus. Approved texture/detail fidelity remains unmet.

Revision 47 retains Ink pressure between frames and adds opt-in GPU fluid diagnostics. Matched captures reduce remaining RMS divergence from34.38% to5.06% with12 iterations; longer/lifecycle capture3.86% and finite fields. Release/core/native GPU checks pass. Ink still lacks approved fine curls; this numerical improvement does not close visual acceptance.

Revision 48 adds restrained warm-pigment luminosity to Ink after rejecting a washed-out stronger glow. Native debug capture passes; coarse smoke structure and reference fidelity remain unresolved.

Revision 49 aligns Ink velocity reconstruction and advection with pressure's face offsets. Native GPU diagnostics remain finite and residual improves; cost increases and smoke remains coarse. Force/boundary placement and visual fidelity still need work.

Revision 50 aligns Ink body/confinement forces with face-based transport. Native GPU diagnostics pass with1.25% remaining interior RMS divergence. Stronger confinement rejected visually; coarse detail remains unresolved.

Revision53 adds irregular fine Aurora veils; broader sweep experiment rejected. Native debug capture and fixed-star regression pass. Reference composition/material fidelity remain unmet.

Revision54 improves Orbit foreground rock relief and darker stone material. Native debug capture and unscaled crop inspected; overall reference fidelity/performance remain unaccepted.

Revision55 refines Orbit ring placement and backlighting, with matching shadow geometry. Native debug capture inspected; approved overall fidelity and performance remain unaccepted.

Revision56 replaces Orbit's coarse procedural sky with a credited NASA catalog-based Milky Way texture. Native debug capture, recording, scene lifecycle and core tests pass. Sky detail improves; planet/ring fidelity remains unfinished. Memory targets are exceeded (378.4 MiB private / 261.5 MiB estimated GPU in lifecycle capture). Goal remains active; no visual approval claimed.

Revision57 introduces shared ring optical depth, filtered fine bands, angle-dependent scattering and matching shadows. Native 1080p/4K captures inspected; no visual gate passed. The 4K crop exposes debris/plane intersections needing correction. GPU p95 8.48 ms at 1080p / 17.97 ms at 4K; memory targets remain unmet.

Revision58 offsets resolved ring boulders clear of the thin dust sheet, removing the straight-cut silhouettes visible in the prior 4K crop. Native 1080p recording and 4K debug captures pass; full fidelity and memory targets remain unmet. Planet/cloud lighting remains the next major Orbit gap.

Revision59 refines Orbit's directional cloud/surface fill, local cloud occlusion and highlight intensity. Native 1080p/4K GPU-debug captures pass; unscaled cyclone detail inspected. A softening parallax experiment was rejected and reverted. Cinematic depth, visual acceptance and performance targets remain unresolved.

Revision60 softens Aurora's gas shading by removing hard sheet-edge emission and reducing thin veil intensity. Broader parametric path candidates were rejected for seams and fabric-like folds. Native debug capture and six-frame fixed-star regression pass; approved composition and motion fidelity remain unmet. Retained capture: artifacts/revision-60/soft/aurora.png.

Revision61 reorients and broadens Aurora's gas flows around the focal crossings, keeping the left area quieter. Native debug recording and fixed-star regression pass. Composition is closer but remains too sparse and band-like versus approved art; no visual gate passed. Retained evidence: artifacts/revision-61/motion.

Revision62 tested and rejected face-corrected MacCormack velocity transport: finite fields but strong grid-like smoke artifacts, even with weaker confinement. Prior simulation restored and rebuilt. Next diagnostic is density-only versus cached-light rendering to localize the blocky structure. No visual gate passed.

Revision63 adds frozen Ink density/unlit/opacity capture views and confirms grid artifacts originate in the density. Monotone forward fallback replaces clipped MacCormack corrections, reducing block-like features. Native12/30-second simulation captures pass GPU checks; fields finite. Fine fluid detail and visual/performance acceptance remain unmet.

Revision64 replaces manual fluid interpolation with hardware texture sampling, without additional volume memory. Native grid64 p95 falls from prior13.26 ms to7.13 ms in short captures; grid96 measures16.23 ms. GPU-debug lifecycle checks, finite diagnostics and core tests pass. Ink remains visually coarse; no gate passed or default grid increase.

Revision65 provisionally changes Ink source placement and forces to make colors interact. Native 12/30 simulated-second captures and finite-field checks pass; later rendering remains coarse and sheet-like, well below the approved image. No visual gate passed. Evidence: artifacts/revision-65/finer and later. Goal remains active.

Revision66 rejects source noise, tighter framing, grid192 velocity and fourfold opacity after native visual inspection. None supplies the reference's fine billows. Restored revision65 shaders and rebuilt Release. Finer velocity alone costs44.92 ms p95 and412.1 MB GPU without adequate visual improvement; defaults unchanged. Evidence: artifacts/revision-66. Next focus is coherent transported multiscale detail; no visual gate passed.

## Desktop and installer delivery — revision 68

User requested that visual work leave room for feature completion, especially the installer and Windows 11 background behavior. Visual goal remains active and unmet; near-reference quality is acceptable, not pixel identity.

Fixed NSIS payload/uninstall lists to include the current Orbit cloud, night and Milky Way maps. Normal application launch now attaches desktop artwork and opens the library; --wallpaper remains a quiet startup and --preview explicitly requests a preview window. Repeated ordinary launch requests the existing instance's library instead of showing an already-running message. Failed attachment is shown in the library even when there are no surfaces.

Validation: scripts/package.ps1 built Release, passed core_contracts, and produced dist/VolturaWorlds-Setup-0.1.0-win-x64.exe (57,465,509 bytes). Extracted the actual installer using 7-Zip into artifacts/installer-68/payload. That extracted executable completed 18 scene-switch/resize cycles and an Orbit capture at1280x720, using packaged dependencies/assets (artifacts/installer-68/capture). No SDK debug layer is shipped, so this payload check is a runtime check, not equivalent to GPU debug-layer validation.

Computer-use automation launched the build, observed library status Desktop / aurora / Audio off, closed the library and confirmed the process remained, then relaunched and confirmed the same library HWND264810 and same process PID32708 returned. Test-owned process stopped afterward. Automation reported a 640x480 display; screenshot capture was unusable. Therefore this confirms the library workflow/status only, not physical-display rendering, icon interactions or complete Explorer integration.

Still required: actual standard-account install/upgrade/uninstall tests, signing, desktop icon/taskbar/Show Desktop checks, Explorer recovery, multi-monitor and endurance tests, remaining product features. Installer is a development artifact, not a validated release.

## Installer lifecycle — revision 69

Added persisted InstallLocation and upgrade-safe startup settings to NSIS. Fresh installs still default to Start with Windows; existing opt-out is preserved on upgrade. /NOSTARTUP provides an explicit silent-install opt-out. Added DisplayIcon registration.

scripts/test-installer.ps1 refuses existing Worlds installations, associations, shortcuts, startup registration or running processes and refuses elevated execution. It installs into a fixed validation folder beneath the current user's LocalAppData/Programs, verifies default startup and association, upgrades with /NOSTARTUP, upgrades again without flags, and checks retained location/opt-out. It runs the actual installed executable through the native scene/resize lifecycle on a private desktop, uninstalls silently, checks removal of files/registry/shortcut, and verifies retained user-library hashes and a test sentinel. Test-owned sentinel is removed afterward. No existing installation or user content was replaced.

Executed successfully on 2026-10-23 with elevated=false. All installation, both upgrades, installed-runtime and uninstall checks passed. Evidence: artifacts/installer-lifecycle/results.json and installed-runtime/. Setup SHA256417504BAFD9FB9F11C953CFBC2DEE56E6F31742576554D2973BBAFB7389E07D0.

This achieves non-elevated per-user lifecycle validation for the current development package. It does not prove signed release readiness, physical desktop icon/taskbar/Show Desktop behavior, Explorer restart, or multi-monitor reliability. Next delivery priority is desktop integration behavior, followed by remaining import/video/community features; visual fidelity remains open.

## Desktop recovery — revision 70

Added bounded five-second automatic retries for an absent or destroyed desktop host. Surface creation now removes partial surfaces on attachment failure and keeps the recovery message visible in the library. Rendering skips destroyed windows. The ordinary wallpaper configuration remains untouched. Host validation rejects non-WorkerW parents and WorkerW windows containing desktop icons.

Added tests/desktop_tests.cpp and CTest desktop_host_contracts, always launched via scripts/test_desktop.py on a private synthetic desktop. Tests cover absent, classic and nested WorkerW layouts; rejection of icon-bearing hosts; negative monitor coordinates; non-activating child style; repeated host destruction and reattachment; combined opaque coverage; hidden/own-window exclusions. The same test starts the actual application with no host, verifies recovery UI and no floating surface, creates a host, verifies automatic attachment, destroys the host for5.5 seconds, recreates it, verifies reattachment, then requests clean exit and verifies surface removal.

Release build and both CTests pass; actual application recovery test took16.96 seconds. Evidence: build/Testing/Temporary/LastTest.log. This verifies controlled host loss/recovery, not a real Explorer restart or physical desktop icon/selection/taskbar behavior. Those acceptance checks remain outstanding. The installer lifecycle script now also runs this recovery harness against its installed executable.

Updated installer validation passed all checks including installed-application host recovery, non-elevated installation, both upgrades, rendering lifecycle, and clean uninstall with retained library data. Current setup SHA256B80D0C7924580F0B8515EB09C32577AB75AF725D717C7C26CC4A5B19AEF9E15A; artifacts/installer-lifecycle/results.json is authoritative for this build.

## Native World import — revision 71

Added a native import preview with a decoded image, title, description/attribution, selected-display destination, scene and audio mode. Package reading/validation and offline-library copying run on a worker thread. Import and apply is disabled until validation succeeds; Cancel does not install or apply a preset. Applying stores the data-only package in the managed offline library, updates the selected display assignment, and requests persistent settings save. Only one import preview can be open at a time to bound transient allocations. Preview bitmap/font and worker lifetimes are explicitly cleaned up.

The library has Import World, and --import now works both at first launch and via bounded WM_COPYDATA handoff to an existing instance. Imported video packages are clearly marked unavailable and cannot be applied while video playback remains unimplemented. This is not a claim of completed video support or export UI.

Private-desktop tests: import_ui_contracts verifies preview bitmap/title, no pre-confirmation writes, cancellation, confirmed offline preset roundtrip, and malformed-package rejection. desktop_host_contracts additionally launches the actual second instance with --import and verifies that the running application receives/rejects the malformed package in the preview UI. Core, desktop/recovery/handoff, and import UI tests pass. Physical-display UI layout and a full installed visual acceptance review remain open. Updated installer packaging is in progress for this source revision.

Revision71 installer build and non-elevated lifecycle validation completed successfully, including the actual installed application's file handoff and host recovery. Current setup SHA256C85F384345411D8BA4F1BA1B7214865B22B473781EA6EC37C6F13A18A27F2FBB; artifacts/installer-lifecycle/results.json contains the current result. All three CTests pass. Export UI, video playback, community features and visual fidelity are still unfinished.

## Video playback core — revision 72

Replaced the video.cpp placeholder with an actual Media Foundation Media Engine frame-server core, matching-adapter D3D11 device, GPU-shared BGRA output and explicit shared-fence synchronization with D3D12. Added local MP4/H.264 metadata checks, dimension bounds, rejection of reported HDR transfer functions, muted looping playback and pause/resume. Decoder hardware acceleration is enabled but actual hardware-decoder selection is not yet profiled.

Release builds and existing CTests passed during development. New worlds_video_tests uses generated320x180/30FPS two-second H.264 and MPEG4-codec fixtures. Native private-desktop test passed twice, including after final cleanup changes:90 GPU frames,6 distinct GPU readbacks, loop boundary, paused playback clock, resume, unsupported-codec rejection and D3D12 error checks. Evidence: artifacts/video-72/results.txt; implementation contract and source references in docs/VIDEO-IMPLEMENTATION.md.

This core is not yet wired into renderer output or video import/UI. Fit/Fill, managed video copying, HDR/damaged-stream coverage, pause-policy integration and video resource accounting remain. Video World imports stay disabled until that work passes. Current installer remains the previously validated revision71 artifact. Procedural visual targets remain unmet.

## Integrated video backgrounds — revision 73

Connected Media Engine output to the D3D12 renderer with explicit shared-fence ownership, muted looping playback, Fill/ Fit positioning and per-monitor pause handling. The library now offers Import video with a decoded thumbnail and explicit confirmation. Personal files are copied into an atomically staged managed-library folder; moving the original does not break the assignment. Validated video World packages can also be applied. Video controls disable procedural palette/audio adjustments.

Native Fill and Fit captures at 800x800 passed GPU debug checks (artifacts/video-73/fill and fit). All four CTests pass: core contracts, synthetic desktop host recovery and second-instance handoff, native import UI (including personal video cancellation and moving the original), and GPU video readbacks/loop/pause/resume/unsupported codec. Tests use private desktops, not the user's physical display. Source-owned synthetic fixtures are in tests/fixtures.

Remaining video acceptance: broader HDR and damaged-stream coverage, hardware-decoder selection profiling, complete decoder resource accounting, pause submission instrumentation, long-running playback and physical desktop checks. Renderer GPU timings do not include separate decoder work; GPU allocation estimates include the shared output but not decoder-private pools. Visual target remains unmet; no new Ink visual candidate is accepted in this revision.

Revision73 installer lifecycle passed without elevation: install, two upgrades preserving startup choice, installed procedural rendering and video playback, synthetic desktop host recovery/file handoff, and clean uninstall retaining the library. Current installer SHA256 D715D4695BD1EB2DF6D00E792CE5EEF07BF44FAB331160F0A34C6DEDA914099A. Evidence: artifacts/installer-lifecycle/results.json. The installer is unsigned development output; physical desktop and visual acceptance remain outstanding.

## Ink experiment review — revision 74

Three native plume experiments were rejected for columns/diffuse sheets and insufficient detail. All rendering changes reverted and shaders explicitly recompiled. Evidence, performance and the next isolated transport/roll-up investigation are documented in docs/VISUAL-REBUILD.md. The revision73 installer/features remain intact. Visual goal stays active and unmet.

## Ink density transport correction — revision 75

Retained an evidence-backed FP16 density rounding fix. Native one-revolution transport test improved mass retention .7130 to1.0409 and peak .4201 to .5987, with the same GPU texture allocation. Added repeatable analytic GPU transport regression coverage; diagnostic and full-precision comparisons are in artifacts/revision-75. See docs/VISUAL-REBUILD.md for limitations and source references. This is a numerical correction, not completion of the approved Ink artwork; all visual gates remain open. Installer remains validated revision73.
Revision75 Release build and all five CTests passed, including ink_transport_contracts (9.12seconds). Normal Ink capture: p95GPU7.810ms, private233,705,472bytes, estimatedGPU101,741,568bytes. Short offscreen run; not sustained performance acceptance.

## Ink precision and boundary shading — revision 76

Retained nearest-even rounding for velocity/curl and valid shadow values in empty neighboring cells used by boundary interpolation. Rejected seeded-blob composition, closer framing, larger ray budget and per-pixel lighting-cache experiments. Final native GPU debug/finite-field capture passed; p95GPU8.062ms at1080p/grid64, same101,741,568estimatedGPUbytes. Artwork still lacks reference detail and composition. Full evidence and validation scope are in docs/VISUAL-REBUILD.md; installer remains revision73.

## Rectangular density investigation — revision 77

Added capture-only rectangular density layouts (--wide-density); normal defaults unchanged. Native384x216x64 transport preserved76% of peak detail after one revolution versus60% for128cubed, with mass ratio .99964 and L2error .15545. Normal-flow image has finer folds but does not meet the reference. Cost11.566msGPU /340MiBprivate exceeds targets, so the option remains experimental. All five CTests passed; details/evidence in docs/VISUAL-REBUILD.md and artifacts/revision-77. Installer remains revision73.

## Ink resource/detail investigation — revision 78

Smaller experimental rectangular grid retained72% peak density after analytic rotation and ran at6.689msGPU/~229MiBprivate in a short native capture. Actual artwork remains too soft for acceptance. Added rectangular transport regression coverage. Rejected costly render-displacement experiment and restored source/build. Fixed desktop-test DPI awareness after diagnostics showed225% coordinate mismatch; production manifest already uses PerMonitorV2. Evidence in artifacts/revision-78 and docs/VISUAL-REBUILD.md. Installer remains revision73 and visual goal stays open.
Affected desktop_host_contracts rerun passed in16.05seconds after the test-only DPI correction. The other five tests passed in the preceding full run. No production desktop behavior changed.

## Orbit lighting — revision 79

Retained warmer cloud highlights and a modestly broader daylight region; reverted ring-angle/radius experiments. Native GPU-debug capture plus18scene/resize transitions passed. Reference fidelity remains unfinished. Resource targets also remain unmet, especially private memory after lifecycle cycling; uncompressed runtime mipmap construction is the next concrete resource investigation. Evidence and limitations in docs/VISUAL-REBUILD.md / artifacts/revision-79. Installer remains revision73.

## Ink resolution/velocity review — revision 80

Tested higher native fluid resolution and second-order velocity transport against the supplied Ink reference. Both remained visually inadequate; the high-resolution run cost29.890msGPU and the corrected-velocity variants introduced artifacts or soft columns at11+ms. Reverted experimental shader/renderer changes and successfully rebuilt. Evidence and next investigation in docs/VISUAL-REBUILD.md / artifacts/revision-80. No visual acceptance claimed; installer remains revision73.

## Ink coherent-detail foundation — revision 81

Implemented capture-only advected material coordinates with staggered resets and native analytic regression tests. Fixed accumulated coordinate interpolation drift using explicit float-weight interpolation. All seven CTests passed, plus normal-flow GPU-debug lifecycle capture. This prepares coherent detail synthesis; it does not yet change Ink's appearance. Source/evidence and remaining work are documented in docs/VISUAL-REBUILD.md. Visual goal remains unmet and installer remains revision73.

## Experimental Ink detail coupling — revision 82

Connected advected coordinates to an experimental density-detail field. Native captures show finer local folds but still do not match the reference; source/lighting experiments were reverted. Detail remains capture-only, with world-space incompressibility and distortion control still unresolved. Release build, all seven CTests and an18scene/resize GPU-debug lifecycle capture passed. Evidence and limitations: docs/VISUAL-REBUILD.md / artifacts/revision-82. Installer remains revision73; visual goal stays open.

## Ink detail-flow correction and refreshed installer — revision 83

Replaced experimental material-space injection with a precomputed world-space discrete curl, including spatial taper and amplitude variation. Reuses existing scratch storage for potential generation. Added direct detail-field divergence regression checks; all seven CTests passed. Native captures still fail the reference-quality target and exceed the8ms GPU target, so the feature remains capture-only. Details/evidence in docs/VISUAL-REBUILD.md and artifacts/revision-83.

Rebuilt and validated the per-user installer without elevation: install, upgrades preserving startup choice, installed scene/video playback, synthetic desktop host recovery and uninstall retaining library all passed. Current setup SHA256324FC00FCA61DA116D206E9B272DF7D1FAFED295B29A0D8844ABEF169CCAF8A2. This is unsigned development output, not an accepted release. Physical desktop proof and visual match remain outstanding.

## Orbit compressed textures — revision 84

Preserved planet/sky4096x2048 and cloud8192x4096 detail using prebuilt compressed mipmapped DDS assets. Matching1080p captures reduce private memory397->297MiB and estimatedGPU allocations257->157MiB, with small measured image differences. Repeated scene-switch memory remains above target; overall performance/visual acceptance is incomplete. Added strict DDS parser tests; all seven CTests passed. Evidence and limitations: docs/VISUAL-REBUILD.md / artifacts/revision-84.

Rebuilt unsigned installer; non-elevated upgrade from revision83 passed including removal of obsolete texture copies, preservation of startup preference, installed scene/video playback, synthetic host recovery and clean uninstall retaining library. Current setup SHA2562DF82BD08B2F097F26F205240BDA315E851C2E732D55510CDC9F73D54F2BCC5E. Physical desktop and reference-art match remain unverified/unmet.

## Orbit ring illumination — revision 85

Retained warmer, stronger ring illumination and slightly more shadow-side fill. Rejected wider ring tilt and longer camera experiments because they displaced the composition. Native GPU-debug scene/resize validation passed; source geometry restored exactly. Reference fidelity and performance targets remain unmet. Evidence: artifacts/revision-85 / docs/VISUAL-REBUILD.md. Installer remains validated revision84.


## Revision 86 — authored Aurora candidate

Rejected three further procedural density variants (fine threads, flat veils, irregular fine density); restored aurora_volume.hlsli byte-for-byte. Native captures/source snapshots are under artifacts/revision-86. Generated approved-clean-v1.png from the approved Aurora concept with the built-in image editor, removing UI and isolated sky particles. Actual source is1672x941, not4K. Full prompt and provenance are saved. Current Aurora shader uses that image with bounded smooth coordinate deformation, separate fixed stars, palette/brightness/detail/audio controls and inverse display mapping. This is animated artwork, not procedural gas. It preserves substantially more of the concept composition in the inspected native still, but does not establish natural motion or 4K crispness. No visual checkpoint accepted; Ink remains below reference.

Native1080p180frame capture plus120recordedframes passed GPU debug: p95GPU3.8338ms,private186,290,176bytes,estimatedGPU57,635,840bytes. Recorded at30Hz without synthetic frames: artifacts/revision-86/authored/aurora-motion.mp4. These are offscreen/private-desktop readbacks, not physical presentation performance. Six isolated-star frames remained pixel-identical while time/movement/palette/audio inputs changed. All seven CTests passed in52.17seconds. NSIS manifest includes the new artwork and deletes it on uninstall. Installer lifecycle result follows separately.

Revision86 unsigned installer SHA256 E0FAF2E56B60D2DACD21C46EB545C0267F4042B3B520176A98D6EA4A83D3C665 passed nonelevated installation, upgrade from validated84, startup opt-out preservation, installed native scene/resize lifecycle, video playback, synthetic desktop host recovery, and clean uninstall retaining library data. Evidence: artifacts/revision-86/installer-results.json. Includes revision85 Orbit lighting and revision86 Aurora candidate. No physical Explorer interaction or visual acceptance claim.


## Revision 87 — rejected Ink source experiments

Tested separated pulsed injections with toroidal forcing, then crossing cyan/magenta/gold jets, then toroidal forcing aligned to each jet axis with darker pigment and reduced emission. Captured native D3D12 at1080p after720steps. Pulsed64grid formed tall soft columns. Crossing96grid produced interacting heads but insufficient fine structure; aligned jets became smooth diagonal tubes and lost the required composition. None accepted. Restored fluid.hlsl and ink.hlsl byte-for-byte; source variants and native evidence retained under artifacts/revision-87. No installer change.

Aligned96grid/192cubed density capture: p95GPU21.701ms,private428,068,864bytes,estimatedGPU238,711,808bytes. GPU diagnostics clean. Finite-field inspection showed projection RMSdivergence .00110773 -> .0000346456, remainingratio .0312762. This supports solver behavior, not visual quality. Raising coarse resolution and changing source direction alone does not recover the reference's fine folds. Next work must address transported multiscale density detail and shadow structure together, while preserving coherent large-scale motion; simply adding brighter emission or stronger source jets is not justified by these captures.


## Revision 88 — decouple density transport from pigment extrema

Retained component-wise MacCormack fallback in fluid.hlsl. Previously any out-of-bounds RGB correction discarded all four corrections, including valid density. Each scalar now falls back independently; pigment values are bounded to [0,density] before FP16 storage. No new GPU resources or passes. The analytic rigid-rotation fixture now carries independently varying sinusoidal pigments instead of proportional RGB, and inspection checks pigment bounds and reports per-channel error. This is a stronger diagnostic, not artwork acceptance. Historical reports with the former proportional-color fixture are not directly comparable for RGB metrics.

Matched128cubed mixed-pigment rotations: old coupled fallback density peak retention .5562133,relativeL2 .3054429,massratio1.032024; new independent correction .6153537,.2839846,1.042161. Old result had11834 pigment cells outside [0,density] beyond1e-5 tolerance; new has0. RGB relativeL2 improves from [.57545,.61125,.31871] to [.54555,.59728,.29333]. The RGB bound clamp contributes to this comparison; do not attribute all improvement solely to independent fallback. Cubic and rectangular native transport CTests passed; tightened cubic peak>.59 and L2<.30 also checked against saved native report. No physical-display or visual-quality gate passed.

Native1080p smoke comparison atgrid64/720steps inspected before/after. Some contour differences, but still far below concept detail. Retained capture p95GPU10.644ms,private247848960bytes,estimatedGPU101741568bytes; target8ms notmet. Finite-field pressure diagnostic remainingdivergenceratio .0117957. Artifacts under artifacts/revision-88. Installer remains validatedrevision86, not rebuilt for this shader/diagnostic change.


## Revision 89 — complete Ink shadow integration

Retained a lighting-only correction in fluid.hlsl: previous12geometrically spaced shadow intervals covered onlyabout.24domainunits. Rays now integrate until the unit-volume exit, using atmost20intervals with clipped final length and earlyexit. This includes distant occluders while retaining fine nearby samples; no additional GPU resources or simulation changes. Fixed current light direction has nonzero components; maximum integration coverage exceeds its maximum box-exit distance.

Native1080p720step/grid64 capture passed GPU debug (artifacts/revision-89/full-shadow): p95GPU10.689ms,private239124480bytes,estimatedGPU101741568bytes. Visual difference is small for this particular density field. Tested extinction1100 with stronger directlight and narrower goldemission: increased contrast but produced overly solid broad forms, not the reference's fine folds. Rejected that variant; restored ink.hlsl and ink_lighting.hlsli byte-for-byte and rebuilt. Source snapshots/capture in revision-89/dense. No visual acceptance, no installer rebuild. The current field still lacks sufficient multiscale structure; stronger opacity is not a substitute.


## Revision 90 — detail-spectrum comparison

Tested capture-only detail synthesis with18directional modes across40/80/160radian spatial frequencies, transverse amplitude envelopes, and normalized band amplitudes. This replaced the previous6plane-wave modes in the experiment only. Native1080p720step wide48grid captures compare both using the current scalar transport and complete shadow fixes, avoiding comparison against stale revision83 physics.

The new field retains discrete divergence0 in GPU readback (peakcomponent .262742), but its image remains broad and soft; no convincing gain in the reference's fine rolling folds. New capture p95GPU13.770ms,private324780032bytes,GPU153383936bytes; control p95GPU13.122ms. Neither meets8ms. Rejected new spectrum and restored ink_detail.hlsl byte-for-byte, rebuilt and captured control successfully. Source and captures: artifacts/revision-90. Production default remains without capture-only detail, installer unchanged. The comparison does not establish natural motion; no new visual gate passed. Increasing wave count alone is not supported as the next improvement.


## Revision 91 — pressure specialization measured and rejected

Tested a dedicated Jacobi pipeline with8x8x8workgroups and a10cubed groupshared halo, then a direct-load specialized shader with the identical stencil. Both used existing descriptors and12iterations, no added allocations. Native1080p720step/grid64 GPUdebug captures passed. Tiled p95GPU10.798ms, direct10.788ms versus revision89capture10.689ms; these short whole-frame captures show no useful improvement. Direct solver finite and projection remainingdivergenceratio .0113501. Fluid fields after720steps were not byte-identical to the original; no exact-equivalence claim. Rejected both variants, restored renderer.cpp/h and CMakeLists.txt byte-for-byte and rebuilt; removed task-owned experimental shader and generated binaries. Evidence/source snapshots under artifacts/revision-91. No production behavior or installer change, no visual checkpoint passed. Next optimization needs per-stage timing rather than assuming the pressure loop dominates.


## Revision 92 — native GPU stage timing

Added optional capture --gpu-stages. Uses bounded32paired stage queries per frame plus enclosing frame timestamps, with separate metadata for each fenced frame slot. Results are read only after that slot's submission completes. Normal wallpaper rendering does not emit stage queries. Captures export samplecounts, means andp95 for14named stages;12pressure iterations aggregate into one per-frame timing. Scope gaps and profiling overhead are explicitly stated; individualp95 values must not be summed. Query/readback capacity is fixed, no per-frame profiling allocations in Renderer.

Native1080p720step/grid64 split profile (artifacts/revision-92/split): sceneDraw mean4.369ms,p954.811ms; post mean.999ms; lighting .901ms; densityCorrection .789ms; pressureIterations .493ms. Overallp9510.769ms, no performance acceptance. This identifies volume drawing as the largest measured average cost, motivating conservative empty-space skipping before adding more simulation detail. Measurements are offscreen private-desktop GPU scopes, not physical presentation or steady-state power evidence.

All seven preexisting CTests passed64.91s. New gpu_stage_contracts passed2.08s: two native captures with profiling off/on produce byte-identical PNG,14finite nonnegative stage reports with79samples, positive draw/pressure timing, and sum of nonoverlapping mean scopes below enclosing frame mean. Total registered tests now8. No visual checkpoint accepted; installer remains validatedrevision86. Current worktree includes revision88transport and89shadow fixes plus this instrumentation.


## Revision 93 — conservative empty-space skipping

Retained four-cell occupancy bricks in the unused alpha channel of Ink's existing lighting volume, no extra GPU allocation. Each4cubed workgroup reduces density over its6cubed footprint including a one-cell trilinear halo, with conservative cutoff margin. Lighting RGB is unchanged. Ray marching checks the brick and advances only whole original sample intervals before its exit; occupied samples keep their original positions. Grid layouts are multiples of4, so group barriers have full participating groups. Capture --no-empty-skip disables traversal skipping for matched comparisons (occupancy generation still runs).

Native1080p/grid64/720step matched captures (artifacts/revision-93): PNG byte-identical. Draw mean4.7087 ->2.8074ms,40.38percent reduction. Lighting mean1.0875/1.1978ms in control/skip captures; occupancy introduces some work compared with prior renderer. Whole-frame p95control11.756/skip10.071ms, stillabove8mstarget. Control includes occupancy generation, so this is not a direct whole-frame comparison against pre-change binary. Prior revision92 shortcapture p9510.769ms. Timing variability and profiling overhead remain. No visual detail increase or acceptance claim.

Added native regression comparing byte-identical PNG outputs with skipping on/off for sparse, mature, and rectangular density layouts. Existing query/transport/desktop/video checks also run. Installer/result updates follow separately.

All nine CTests passed in 77.05 seconds, including byte-identical sparse/mature/rectangular skipping comparisons. Rebuilt unsigned development installer (SHA256 7B51B96A2E59B9D8EDD5B186CDE0F5942F34931D8750664A5927772BCB972E8D) and validated a non-elevated upgrade from revision86, startup opt-out preservation, installed native scene/resize and video playback, synthetic desktop-host recovery, and clean removal retaining library data. Evidence: artifacts/revision-93/installer-results.json. Real Explorer interaction, physical displays, visual checkpoints and release performance acceptance remain open. Profiling usage and evidence limits are documented in docs/GPU-PROFILING.md.


## Revision 94 — Ink composition and color placement

Rejected closer lens2.65/yaw-.45 and lens2.2/yaw+.7 captures: cropping magnified low-resolution softness and did not uncover enough cyan. Restored ink.hlsl byte-for-byte; renderer camera remains unchanged. Retained only emitter pigment ordering in fluid.hlsl: gold source0, paletteA(cyan default) source1, paletteB(magenta default) source2. At12simulatedseconds this moves cyan into the upper/left dominant plume, magenta right and gold between, closer to approved color arrangement than magenta-dominated previous output. No advection/force/resource changes.

Native1080p/grid64 captures passed GPU debug at720 and1440steps. Later24second capture reveals a separate unsolved composition problem: the large plumes recede into a smaller low cluster, with desaturated mixed regions. Do not claim stable reference composition from the12second still. Captures/source variants under artifacts/revision-94. Fine folds, opacity/light detail and long-running composition remain below acceptance. No new checkpoint request; installer remains validatedrevision93. A short native check is appropriate to this one-line pigment permutation; no unrelated suite rerun.


## Revision 95 — rejected smoke lifetime changes

Tested density dissipation .08/s instead of .22/s, then .08/s plus a smooth upper absorbing band (4*smoothstep(.70,.98,y)). Native1080p/grid64 captures at12and24simulatedseconds passed GPUdebug. Longer lifetime sustains height but accumulates old mixed smoke, showing a pale flat-topped mass at24s. Upper absorption removes the conspicuous roof but does not restore saturated, articulated plumes. Rejected both variants and restored fluid.hlsl byte-for-byte, retaining revision94pigment ordering and prior transport/shadow/skipping fixes. Source variants and captures under artifacts/revision-95. No new tests required for restored source, rebuild completed. Installer remains revision93. Density lifetime alone is not supported as a solution to composition collapse; mixed-color material behavior and density transport need separate inspection before increasing persistence again. No visual gate passed.


## Revision 96 — source pigment concentrations

Ink's transient density RGB channels now store three nonnegative source concentrations (gold, palette A, palette B); alpha remains total density. Independent advection is followed by concentration renormalization to total density before decay/boundary storage. This is a consistency constraint, not a claim of globally conservative species transport. Zero-concentration remnants are removed rather than leaving density with undefined color. A shared scale enforces the total-density cap without independently clipping pigment channels. No texture size or resource changes.

Rendering normalizes the concentrations and blends palette colors geometrically (weighted logarithms) as an artistic subtractive approximation. This is not a spectral pigment model. Gold emission follows gold concentration directly. Palette changes now recolor existing material immediately. Analytic --transport-test deliberately keeps its existing diagnostic RGB field and bypasses production concentration normalization, so its dump semantics remain different. Raw field dump filename is unchanged; interpret production RGB as concentrations, not display color.

Native1080p/grid64 captures at12and24seconds passed GPUdebug. Mixed regions become darker/colored instead of simple pale averaging, but24second composition still collapses and remains far from the reference. Initial concentration inspection found122zero-pigment residual cells with density below8e-7; fixed by dropping those negligible remnants. Visible-cell sum rounding error was below.001 relative in that initial capture. Added inspect-pigments.py and native ink_pigment_contracts to check finite nonnegative fields and concentration sum within FP16 tolerance. Test outcome follows. Evidence/source snapshots under artifacts/revision-96. Installer remains validatedrevision93. No visual checkpoint accepted.
Revision96: all five relevant Ink CTests passed (57.11 seconds), including new pigment consistency and existing transport, coordinate and empty-space checks. Ten tests are now registered; unrelated tests were not rerun this turn.


## Revision 97 — rejected source microstructure

Tested two octaves of coherent value noise in each Gaussian injection, evaluated only where Gaussian amplitude exceeds .001; unchanged tails avoid a cutoff discontinuity in injection support. Modulation .22..1.78, source-local frequency48/97.44 and slow temporal evolution. Native1080p/grid64/720step capture passed GPU debug, p95GPU10.177ms. The large folds change, but visible structure remains broad/soft; no convincing fine-detail retention. Rejected and restored fluid.hlsl byte-for-byte, rebuilt. Evidence and source variant under artifacts/revision-97. Installer remains revision93. Source-only microstructure is insufficient; subsequent detail work must follow the volume through its evolution rather than relying on fine injection structure surviving the coarse transport. No visual checkpoint accepted.

## Revision 98 — rejected advected material erosion

Capture-only prototype used existing two advected FP32 coordinate charts and shared 128-cubed noise texture to attenuate ray density. Applied the same attenuation in shadow integration, with macro gradients retained as a lighting approximation. Attenuation stayed within .12..1, so existing macro occupancy remained conservative. Chart blend followed the existing staggered reset schedule. Additional descriptor tables and explicit compute/pixel transitions passed native D3D12 debug validation.

Matched 960x540/grid48/720-step private-desktop offscreen captures with detail coordinates enabled in both cases: control GPU mean4.385ms/p956.241ms; erosion mean9.218ms/p9511.311ms. Lighting mean .477 ->4.246ms. Prototype also loaded the Aurora noise/artwork resources (~14.1MB added GPU allocations); no production optimization was attempted. Small-scale texture changed but remained translucent/wispy, without the dense folds, crisp silhouette or lighting of the user's Ink reference. Rejected; restored renderer.cpp, main.cpp, ink.hlsl, fluid.hlsl and CMakeLists.txt byte-for-byte and removed experimental include from source. Prototype and captures preserved under artifacts/revision-98. No visual gate passed. Installer remains validated revision93; no installer changes. Build restoration verified separately.

## Revision 99 — bounded cubic density transport evaluated and rejected

Tested capture-only Catmull-Rom reconstruction for forward/reverse scalar transport using64 FP16 donor loads and component bounds from the eight enclosing cells. Velocity, sources, density rendering and lighting stayed unchanged. Compared cubic plus existing correction and cubic forward-only (bypassing correction's result). Forward-only still executed the correction work in this prototype, so its timings are not an optimized algorithm comparison. Both used existing allocations.

Native analytic128-cubed one-revolution transport: cubic+correction mass1.05184,peak.73315,L2.20809; cubic forward-only mass1.02142,peak.70920,L2.19908. Both finite/nonnegative and zero invalid pigment cells. Earlier retained linear+correction benchmark peak.61535,L2.28398. This demonstrates improved scalar reconstruction on rigid rotation; it does not establish turbulent-artwork quality.

Matched native1080p/grid64/720step artworks passed GPUdebug: controlmean8.672ms/p9510.737; cubic+correction11.319/13.550; forward-only12.833/13.529. Same estimated101741568GPUbytes. Corrected cubic produces visible terraces/stepping; forward-only reduces those but remains soft, translucent and lacks the reference's multiscale folds. Neither retained. Source restored byte-for-byte, release rebuilt; no new visual gate or installer rebuild. Experiment snapshots/captures/field reports under artifacts/revision-99. Next investigation should isolate velocity-flow structure and its dissipative transport, rather than more scalar-only interpolation/noise/opacity tuning. The previous velocity correction experiment (revision80) also produced grid-aligned artifacts, so any new velocity reconstruction must include smooth analytic-flow diagnostics and artwork inspection.

## Revision 100 — velocity reconstruction and smooth-vortex evidence

Tested bounded Catmull-Rom sampling of each staggered velocity component at its midpoint departure; no extra volume allocations. A capture-only stationary2D Euler vortex (amplitude .08, MAC-face initialization) disabled sources/drag/confinement forces while retaining the bounded-domain projection and velocity transport. Atgrid48/360steps, interior central-plane energy ratio linear .954509 versus cubic .996720, relativeL2 .043039/.034222. Boundary discretization, pressure and FP16 also contribute; this is not an isolated advection or full energy-conservation proof.

Native1080p/grid64/720step cubic smoke exhibited severe grid-aligned breakup (p95GPU11.425ms). Finite-field check passed, projection remaining divergence ratio .02170, but that does not certify visual quality. Disabling confinement for cubic reduced breakup, yet still left soft sheets and axis-aligned streaks, without reference-quality folds. Neither visual variant retained. This confirms the current confinement amplifies high-frequency reconstruction artifacts; simply retaining more velocity energy is not enough.

A smaller diagnostic-only patch passed all five Ink CTests (56.59s), but its recompiled default smoke differed visibly from the previous native still (mean absoluteRGB differences4.91/5.98/7.57 on0..255). No byte-identical/default-invariant claim. To avoid changing the production renderer solely for a diagnostic, archived that patch and inspector too, then restored main.cpp/fluid.hlsl byte-for-byte. Final native diagnostic (with zero density) energy .954423,L2 .043074,finite. All experiment sources/captures/inspector under artifacts/revision-100; --vortex-test is NOT in the final executable. Installer remains revision93. No visual gate passed.

Next direction: replace the simple Gaussian-source plume structure with authored three-dimensional multiscale source/initial fields and evaluate their native density, lighting and temporal evolution together. Preserve true volumetric GPU simulation; do not substitute a warped concept image or accept another scalar/velocity interpolator solely for an analytic metric improvement.

## Revision 101 — authored volumetric initialization rejected

Tested a capture-only 3D initial concentration field: cyan/magenta winding columns, small gold column, multi-scale spatial noise and edge erosion. Initialized actual GPU density/pigment volumes, then let normal pressure-projected simulation and existing sources evolve them. Native1080p/grid64 captures at1and180steps passed D3D12 debug. The first variant reads as rounded separate columns, not the reference's interacting plumes.

A second variant curled the noise coordinates around four localized centers, narrowed the edge transition, increased density and restricted gold to thin noise ridges. Tested384x216x64 density/grid64 at1and180steps. Initial structure becomes repeated corrugated ridges rather than natural smoke; after3seconds it softens while retaining the obvious two-column silhouette. This combines field and density-layout changes, so it is not an isolated resolution comparison. Evolved native capture p95GPU13.119ms,private362614784bytes,estimatedGPU183530496bytes. Pigment readback:0invalidcells,726933activecells,maxsumrelativeerror.000968. Numerical consistency does not establish quality.

Rejected both initial-field variants. Archived field/include/main/CMake snapshots and captures under artifacts/revision-101. Restored original fluid.hlsl/main.cpp/CMakeLists.txt and removed the experimental include; release rebuilt. No active --sculpted-ink flag remains. Installer stays revision93. Visual goals remain unmet. Simple winding columns plus warped noise are insufficient, and increasing their density or curl is not a justified next tuning loop. A useful next validation would render a genuinely structured 3D smoke reference volume through the native lighting path, separating renderer limitations from simulation/source-design limitations before more source tuning.

## Revision 102 — retained frozen-volume rendering diagnostics

Implemented capture-only --ink-reference-volume. Strict exact-size finite/nonnegative RGBA16F input uploads through bounded D3D12 footprints, explicit copy transitions and fenced upload-resource lifetime. Uploaded density bypasses fluid evolution but uses normal lighting and ray integration. Metrics identify frozen imported data and zero simulation steps. Allocation estimates include the retained upload. Added white-lit diagnostic view separating lighting from pigment/emission. No production simulation shader changes.

Added ink_reference_volume_contracts: native byte-exact roundtrip, truncated/extended files,NaN and negative rejection. Passed new contract and existing empty-space contracts after final build (14.78s); existing GPU-stage contract passed earlier in this turn before the final diagnostic-view addition. No full-suite claim. A historical revision99control image is not byte-identical to this build; that historical capture used the experimental cubic-transport shader with its flag off, so it is not a same-shader default baseline. MeanabsoluteRGBdifference2.797/2.751/3.737, max161/107/111; no claim of pixel-identical default output.

Generated a local Blender5.2.2 reference bake (96base,2xnoise,40frames,three pigment sources) in222.96s. Direct API exports were empty after headless bake; recovered valid192cubed scalar density/color noise caches using validated MNT3 headers. Conversion axis remap and densityscale.1 documented. Native upload/readback SHA256exact match0F3A8ACB86B6256376D7CB26CAAA0D1D5705A396AD9E914F77480B5B9D3C13B9. Native capture includes fine small-scale structure, but flat/muddy mixing and an inadequate plume composition. Frozen native1080p30frames p95GPU12.521ms,private483659776bytes,allocations295334912bytes; these include diagnostic upload/unusedsolverresources and are NOT native simulation performance.

Also rendered same cached volume offline using BlenderCycles32samples,no denoising,whitevolume. Nativewhite-lit view preserves similar silhouette/detail but emphasizes surface-like small highlights. Lighting/exposure/transforms not calibrated identically; no numerical equivalence claim. Neither result matches target. This provides a fixed3D density fixture for isolating lighting from solver limitations; retain the diagnostic support, do not replace interactive Ink with this bake. Details/reproduction in docs/INK-REFERENCE-VOLUMES.md, artifacts/revision-102. All bake/render/capture processes completed. Installer remains validatedrevision93. Visual goal remains active.

## Revision 103 — selective gold emission retained; lighting variants rejected

Used revision102 frozen Blender volume to isolate lighting. Removing density-gradient form lighting reduced surface-like grain but flattened fine structure. Moving the directional shadow ray from(.6,.6,-.7) to(.85,.4,.35) deepened shadows in the frozen white volume; live native smoke nevertheless became smoother and less articulated. Rejected both lighting changes, restoring fluid.hlsl and ink_lighting.hlsli byte-for-byte. Fixed-volume comparison is useful evidence, not grounds to adopt a lighting model that worsens live output.

Retained only a narrow artistic emission change in ink.hlsl: gold self-emission now uses square(saturate((goldConcentration-.25)/.75)) instead of raw gold concentration. Pure gold retains its previous emission gain/fringe curve; diluted gold no longer adds as much orange glow to cooler mixed pigment. A stronger threshold/brighter pure-gold experiment was rejected. Native fixed-volume comparison shows less brown-orange haze; live720step and1440step GPU-debug captures pass. No density transport or allocation change. The24second composition still collapses into a low cluster, and fine folds remain absent; this is a color-separation improvement, not a passed visual gate.

Evidence in artifacts/revision-103 (lighting variants, selective-gold comparisons, live captures). Native1080p/grid64/1440step final capture p95GPU11.178ms,private236527616bytes,estimatedGPU101741568bytes; not sustained/per-display acceptance. Release build and targeted reference-volume/empty-space tests checked. Installer remains revision93. Next substantial gap remains the evolving multiscale plume structure and its composition; neither new lighting nor reduced emission resolves it.

## Revision 104 — boundary and circulation experiments rejected

Tested an open-top capture variant: final pressure row held at zero atmospheric pressure and its stored Yface allowed to flow, representing the outlet of the interior row below. Native1080p/grid64 at720and1440steps passed GPUdebug, but24second composition still collapsed low. This experiment does not establish a generally correct/open-domain solver; it failed its intended visual purpose and was removed.

Separately restored closed boundaries and multiplied prescribed toroidal and broad circulation forces by saturate(localSmoke*8), leaving buoyancy, source injection, pressure and detail parameters unchanged. Native12and24second captures passed GPUdebug. Height persisted better, but smoke became narrow separate columns with grid-aligned artifacts; no reference-quality multiscale folds or composition. Rejected rather than retaining taller output as a substitute for the target artwork.

Snapshots and captures under artifacts/revision-104. Restored fluid.hlsl/main.cpp byte-for-byte, rebuilt. Revision103selectivegold and102reference-volume diagnostics remain active. Installer remains93; no visual acceptance. Evidence points away from a top-wall-only explanation and shows prescribed circulation affects persistence, without identifying an acceptable replacement. Further fine-flow work should examine energy-scaled multiscale synthesis against a published method; the existing six-wave capture detail field is not a validated wavelet-turbulence implementation.

Revision115 foundation recheck: core_contracts, desktop_host_contracts, import_ui_contracts and video_gpu_contracts all passed on the current Release build (22.31 seconds total). Desktop-host evidence is private/synthetic, not the user's physical Explorer session. Visual gates remain open.


## Revision 159 - bounded cubic Ink transport

Live coarse density uses bounded cubic interpolation to preserve more detail in motion. Matched native rotation peak retention improves0.61535 to0.71017 and relativeL2density error falls0.28398 to0.19904. Added cubic capture diagnostic and two transport regressions; both pass after fixing its pigment bound. Original transport tests and all four empty-space pixel-equivalence cases pass. Native1080p motion capture/finite field readback in artifacts/revision-159/final; p95GPU13.324ms, GPU101741568bytes. Still below approved visual target and8ms target. No visual gate passed; installer unchanged. See revision159/RESULTS.md for evidence and limitations.


## Revision 160 - preserve cubic accuracy with less work

Rejected27hardware-filtered-tap transport because cumulative density error exceeded the unchanged regression bound. Retained separable arithmetic on64exact samples and skipped unused legacy correction work in live cubic mode. Correction bypass has pixel-identical native output; all four transport regressions pass. Short native1080p meanGPU9.718ms/p9513.153ms; no added allocations and no sustained performance claim. See artifacts/revision-160/RESULTS.md. Visual target and8ms target remain unmet; installer unchanged.


## Revision 161 - persistent gold tracer prototype

The optional --ink-filaments capture path now uses4096 persistent fluid-advected gold particles instead of instantaneous streamlines. Adds128KiB only when enabled; default remains off. Lifecycle/zero-dt redraw regression passes; default native image remains pixel-identical to revision160. Native60frame motion evidence in artifacts/revision-161/final. Flecks are a partial component, not accepted artwork; large-scale folds and fine volume detail remain unresolved. Higher-resolution cubic transport still fails visual target and costs28.28ms p95 at1080p. Final tracer capture p9513.119ms; installer unchanged.


## Revision 162 - curved gold history trails

Optional tracers now retain32history knots for128particles at15Hz, with bounded cubic curves and pixel-footprint coverage for subpixel gold lines. Adds64KiB (192KiB total optional storage). Native GPU-debug capture and lifecycle/zero-time redraw regression pass; default remains off. Final motion evidence in artifacts/revision-162/filtered. Fine gold detail is a component only; base smoke still fails approved visual target. GPU p9513.563ms; installer unchanged.


## Revision 163 - measured detail-coordinate distortion

Rejected deformation-filtered and higher-band detail candidates; restored shader and verified pixel-identical native control. Crucially, native coordinate readback at~12seconds shows89.77% of occupied cells in the active chart above the experiment strain threshold,4.57% nonpositive determinants. Filtering therefore suppresses most detail. Next address coordinate renewal before severe deformation; do not repeat amplitude/frequency-only tuning. artifacts/revision-163/RESULTS.md records primary references, metric limits and evidence. No visual gate or installer change.


## Revision 164 - earlier live detail renewal

Optional live detail charts now renew every second using a shared renewal/blend clock. New native regression rejects the old lifetime; both renewal and analytic-coordinate tests pass. At the matched 12-second snapshot, sampled active-chart excessive-strain fraction decreases from89.21% to24.34%; this is not whole-cycle or visual acceptance. Native motion remains soft and below the user reference. GPU p9516.139ms and private338.2MiB exceed targets; no new allocations. See artifacts/revision-164/RESULTS.md. Ordinary detail remains off; installer unchanged.


## Revision 165 - final sampling isolation

Rejected Catmull-Rom final lighting and density reconstruction: almost unchanged soft artwork, GPU p95 25.277/113.247ms respectively (second also disables empty-space skipping). Source restored, Release rebuilt, fresh native capture pixel-identical to revision164. Evidence narrows the missing detail to material structure rather than final sampling. No new visual acceptance or installer change. See artifacts/revision-165/RESULTS.md.


## Revision166 - two-grid residual energy

Tested restriction/prolongation residual RMS driving advected detail, then localized two-octave potential in place of plane waves. Neither supplies approved billows; both rejected. No new allocations. GPU-debug captures pass; source restored and fresh native output pixel-identical to revision164. See artifacts/revision-166/RESULTS.md. Next isolate fine-density retention under live nonlinear flow, not further velocity-amplitude tuning. Installer unchanged; no visual acceptance.


## Revision167 - captured-flow density retention audit

New CPU diagnostic replays native frozen velocity/detail readbacks with bounded cubic FP16 transport. After12steps each way, a16-cell-period pattern retains99.27% correlation amplitude; a5.33-cell-period pattern retains85.95%. Unbounded diagnostic improves to91.40% but overshoots seeded range, so no renderer change. Central-cube measurement includes low-density cells; not live GPU or visual acceptance. Identity check passes. See artifacts/revision-167/RESULTS.md for scope. Installer unchanged.


## Revision168 - moon crater geometry

Retained shared large-crater height in Orbit moon vertices and consistent pixel normals; fine craters remain shading-only. Explicit branch avoids crater vertex work on other objects. Native1080p/4K debug captures inspected and capture_time_contracts passes. GPU p959.686/26.133ms; no new allocations. Ink64-donor limiter experiment rejected after negligible analytic improvement and continued soft artwork. See artifacts/revision-168/RESULTS.md. No visual gate passed; installer unchanged.


## Revision169 - Orbit foreground focus

Retained depth-aware near-debris blur with sharp moon override, reusing existing depth buffers and no additional GPU allocations. Native1080p/4K captures and lifecycle/capture-time/tracer regressions pass. Scoped focused-region RGB comparisons are exact; explicit branch variant4K output is pixel-identical. Final4K GPU p9529.937ms, so performance target remains unmet. See artifacts/revision-169/RESULTS.md and final/orbit-motion.mp4. This is artistic screen-space focus, not physical lens simulation or display proof. No visual gate passed; installer unchanged.


## Revision170 - packed Orbit focus radius

Replaced depth-texture post reads with blur radius in existing opaque HDR alpha. Planet/moons/sky stay sharp; transparent Orbit layers preserve the radius channel. Removed extra depth SRVs/transitions and negative-alpha marker. No additional allocations. Native1080p/4K checks, capture-time and lifecycle/tracer tests pass; selected focused regions pixel-identical to revision168. GPU p9511.432/26.600ms, still above targets. See artifacts/revision-170/RESULTS.md. No visual acceptance or installer change.


## Revision171 - atmospheric scattering comparison

Rejected two numerical sun/view-path atmospheric candidates: added broad haze and reduced reference contrast. Restored planet shader, rebuilt, and fresh native output pixel-identical to revision170. Previous moon geometry and packed focus remain. Evidence in artifacts/revision-171/RESULTS.md. No visual acceptance or installer change.


## Revision172 - stationary Aurora fine points

Retained restrained fine gold stars concentrated toward the right, separate from moving artwork. Six-frame isolated-star regression has zero changed pixels across time/motion/audio/palette changes. Native1080p/4K debug captures inspected. Full-scene left-region difference recorded separately; exact identity was not achieved. No added resources. Source resolution and authored-motion limitations remain; no visual gate passed. See artifacts/revision-172/RESULTS.md. Installer unchanged.


## Revision173 - passive particle smoke prototype

Rejected131072particle Gaussian smoke reconstruction: sparse dots or blurred spots, not reference billows. Existing coarse density supplied approximate extinction and was not calibrated to particle coverage, so this does not rule out a consistent particle-volume method. All three experimental files restored byte-for-byte, Release rebuilt and tracer lifecycle/paused-redraw test passes. See artifacts/revision-173/RESULTS.md. No visual gate passed; installer unchanged.


## Revision174 - corrected particle seed audit

Found high-ID floating-hash duplication in the prior particle prototype; integer seeds improve distribution. A1,048,576particle native candidate is more continuous but still grainy and below reference, with p95GPU57.981ms. Both candidates rejected; three source files restored, release rebuilt, tracer lifecycle/paused test passes. CPU seed audit and native evidence in artifacts/revision-174/RESULTS.md. No production seed or installer change; no visual acceptance.

## Revision175 - consistent particle-volume reconstruction

Captured one million passive particles and deposited their positions into a frozen128-cubed volume with matching native lighting. Still broad tubes and granular edges, not reference billows; rejected. Diagnostic calibration is not conservation proof. Restored both production files, rebuilt Release, tracer contracts pass3.69s. Evidence and reproducible deposition in artifacts/revision-175/RESULTS.md. Installer unchanged; no visual gate passed.

## Revision176 - high-resolution flow and imposed-roll isolation

Velocity128/density192 cubic transport adds folds but remains soft and costly; removing imposed circulation yields narrow columns and striations. Both rejected. Snapshot projection removes about96-97.5% of interior divergence RMS; this does not prove full solver quality. Source restored and fresh default native pixels exactly match revision160. See artifacts/revision-176/RESULTS.md. No visual gate passed; installer unchanged.

## Revision177 - nonlinear vortex audit

Retained a native periodic staggered-grid diagnostic. First-order transport retains13.9% energy at2sec; cubic70.2%. Confinement3 overshoots221.5%; mild confinement improves energy at12sec but loses the exact stationary shape. Energy alone is not an acceptance criterion. Evidence and reproducible sources in artifacts/revision-177/RESULTS.md. Production shader restored/rebuilt; tracer contracts pass3.69s. No visual gate passed; installer unchanged.

## Revision178 - conservative momentum flux and midpoint audit

Stationary vortex error improves to0.26%, but translated forward-Euler version blows up. Pressure-extrapolated midpoint avoids that failure in the tested run, retains99.15% energy at12sec but has significant phase/shape error. Live output remains streaked and costly; rejected. Restored shader and fresh default native pixels match revision160 exactly. Evidence in artifacts/revision-178/RESULTS.md. No visual gate passed; installer unchanged.

## Revision179 - staged transport and long-run accuracy

Fourth-order skew split improves2sec error but loses the translated vortex by12sec; rejected. Reused curl scratch for a bounded-cost prediction pass, also tested original second-order control. Intermediate FP16 changes evolution, so no equivalence or visual improvement claimed. Restored both production files, rebuilt and tracer contracts pass3.76s. Evidence in artifacts/revision-179/RESULTS.md. Installer unchanged; no visual gate passed.

## Revision180 - projected stages and pressure reference

Three projected SSPRK3 stages with12Jacobi iterations still gain excess energy. Weighted96iterations removes most excess energy but retains large shape error. Independent CPU discrete FFT projection, including half-rounded velocity, behaves better. Evidence points toward a pressure convergence audit; no live visual improvement claimed. Restored both files, rebuilt and tracer contracts pass3.76s. See artifacts/revision-180/RESULTS.md. Installer unchanged; no visual gate passed.

## Revision181 - pressure-mode convergence audit

Native one-projection periodic test matches predicted Jacobi mode errors within2e-5. Twelve ordinary iterations retain98.1% broad error and100% alternating error;96weighted iterations still retain88.4% broad error. Confirms convergence limitation in the cold-start test, not a GPU binding bug or a visual fix. Sources/readbacks in artifacts/revision-181/RESULTS.md. Production restored/rebuilt; tracer contracts pass3.79s. Installer unchanged; no visual gate passed.

## Revision182 - multilevel CPU pressure prototype

New probe-pressure-multigrid.py passes independent periodic/closed-boundary oracles for manufactured and random64-cubed RHS in float32/64. On181native input, corrected-velocity error versus exact discrete projection is1.818e-6 after six CPU cycles. GPU port, wall-face handling, timing and live visual acceptance remain unverified. No renderer or installer changes. See artifacts/revision-182/RESULTS.md.

## Revision183 - native multilevel pressure component

Retained standalone HLSL/D3D12 pressure probe and two native CTests. Twelve32/64/128 periodic/closed manufactured/random cases pass independent oracles (max pressure error5.33e-5). Intel Arc confirmed; no GPU timing or visual acceptance claim. Application does not select this solver yet. Evidence in artifacts/revision-183/RESULTS.md. Installer unchanged.

## Revision184 - capture-only native pressure integration

Retained --ink-multigrid, pressure bridge/renderer component and lifecycle regression. Actual live pressure matches discrete reference closely, but boundary divergence remains and artwork is still soft; no visual gate. Four relevant CTests pass; default native pixels match revision160. GPU p9518.339ms at1080p, above target. See artifacts/revision-184/RESULTS.md. Installer unchanged.

## Revision185 - capture wall-face correction

Fixed implicit upper normal boundary and released incorrectly clamped last interior faces in a separate capture shader. Full-domain divergence RMS1.63e-5, compatible RHS mean0; new boundary regression rejects184. Initial shared-shader edit changed default pixels, corrected by compile-time pipeline isolation; default now matches baseline exactly. Four CTests pass. Native artwork still soft and GPU p9519.43ms; no visual gate. See artifacts/revision-185/RESULTS.md. Installer unchanged.

## Revision186 - sharp transport after projection correction

Tricubic velocity/no-confinement candidate passes boundary audit but remains soft and streaked; rejected. Native frozen opacity and density-slice diagnostics show streaks already in sampled density. Final snapshot CFLmax.609; not a whole-run bound. Restored capture shader and lifecycle/boundary test passes7.74s. See artifacts/revision-186/RESULTS.md. No visual gate; installer unchanged.

## Revision187 - monotone density trial rejected

Harmonic-slope cubic density interpolation remains soft and streaked in native output and direct density slices; boundary audit passes but artwork does not match the reference. Restored capture shader and rebuilt Release. GPU p95 23.84ms; no performance or visual acceptance. See artifacts/revision-187/RESULTS.md. Installer unchanged.

## Revision188 - higher-resolution corrected simulation rejected

Native128 velocity/192 density with corrected capture pressure produces more thin folds but remains soft and streaked. GPU p95 90.65ms, estimated287.5MiB GPU allocations and512.3MiB private memory exceed targets. Boundary audit passes; numerical validation does not establish visual quality. No source/default/installer change. See artifacts/revision-188/RESULTS.md. Next work must improve flow structure at affordable resolution; raising resolution is not a sufficient route.

## Revision189 - pulsed/colliding sources rejected

Two native captures change injection timing and jet direction. More localized structure but columns/ragged plumes still lack reference curled billows and gold detail. Boundary audits pass;16-17ms GPU p95 remains over target. Restored capture wrapper/rebuilt; no default/installer changes. See artifacts/revision-189/RESULTS.md. No visual gate passed.

## Revision190 - projected RK3 live/substep trials rejected

Conservative transport with corrected multigrid produces more small-scale motion but grid-like breakup. Four substeps do not resolve it and cost108ms GPU p95. Substep boundary relative-divergence check fails; finite fields alone are not acceptance. Restored renderer/shader and rebuilt. See artifacts/revision-190/RESULTS.md. Investigate spatial high-frequency behavior before more live parameter sweeps. No visual gate or installer change.

## Revision191 - grid-scale dissipation isolates breakup

Fourth-difference damping in projected RK3 greatly reduces grid-scale spectral accumulation and restores larger curls. Weak.1 version retains more detail but remains smooth and far from reference;32.9ms GPU p95 exceeds target. Boundary audits pass. Archived candidates, restored renderer/shader and rebuilt. See artifacts/revision-191/RESULTS.md. Candidate needs controlled rotational validation before adoption; no visual gate or installer change.

## Revision192 - small updates lost in half-precision transport

Periodic rotational test expects amplitude.945893 after12s; half storage produces.999561, while float storage produces.945871. Float full-field error remains.00649, so not exact shape validation. Diagnostic uses periodic boundaries, no density/artwork. Evidence supports selective FP32 velocity/RK storage for next live trial; not an art-quality claim. Restored three source files/rebuilt; see artifacts/revision-192/RESULTS.md. No visual gate or installer change.

## Revision193 - selective precision live prototype

FP32 velocity plus separate saved-RK volume adds8.06MiB and reduces native snapshot divergence to4.39e-9, but artwork still lacks reference detail.34.88ms GPU p95. One warm pressure cycle cuts to19.51ms but fails divergence threshold; rejected. Archived source variants, restored five source files/rebuilt. See artifacts/revision-193/RESULTS.md. No visual gate/installer change.

## 0.1.1 installable preview — 2026-10-25

User stopped the picture-parity effort and requested an installable three-scene application. Restored the retained fluid/Ink shaders from before revision194; no visual trials promoted. Version0.1.1 packages Aurora, Orbit, Ink, tray/background operation, per-display controls, optional audio reaction, managed video import and .world import. Installer's finish-page launch now opens the library; login startup remains quiet. Added an explicit library Exit Worlds action in place of the unpublished Community destination, and state-specific Pause/Resume and Reconnect desktop labels.

Release build passed core, capture-time, desktop-host recovery/single-instance, import-UI and video-GPU contracts (5/5). All three scenes separately rendered on a private desktop with D3D12 validation. This is application evidence, not physical display or installation proof. User explicitly reserved installer testing: no installer, upgrade or uninstaller execution performed for0.1.1. Existing Voltura signing configuration used for application and setup; local self-signed certificate, timestamped. No public publication or deployment.

The original visual-quality and broader release specification remain unmet. This deliverable is a hands-on Windows preview; README reflects implemented capabilities and outstanding requirements instead of obsolete implementation notes.
