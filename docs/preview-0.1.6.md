Voltura Worlds 0.1.6 refines lighting, materials and motion across all ten Worlds, with refreshed native recordings and social preview on the product site.

- Softer highlights and bloom, improved depth and reflections, and more distinct motion across the collection.
- Movement at zero freezes Smoke and prevents audio deformation in frozen Aurora and Bloom. Orbit sunlight now respects Brightness, including zero.
- Advanced settings makes the library translucent while you adjust your World. Tray menus follow the current display's DPI after reconnects and scale changes.
- A failed video or renderer recovers per display, while healthy displays keep animating. Switching scenes retries immediately.
- Cancelling imports keeps the UI responsive; video copies honour cancellation. Stale worker notifications cannot affect reopened dialogs or restarted renderers.
- Website animations stay paused offscreen when returning to the tab. All ten native videos, posters and the social card have been refreshed.
- All changed application code, shaders, tests, capture tools and build documentation are included in the source release. Python development requirements are documented and checked before compilation.

Install VolturaWorlds-Setup-0.1.6-win-x64.exe for Windows 11 x64. No administrator access is needed. Existing settings, imported videos and startup preferences are retained. Automatic updates remain available to users who enabled them.

The application and installer are signed and timestamped with the existing self-signed Voltura AB certificate. It is not publicly trusted; Windows may show a publisher or reputation warning. Verify the installer using SHA256SUMS.txt.

Validation: the full 31-test native suite, playback regression checks, signed updater staging/cancellation and isolated installer lifecycle checks. Native rendering evidence uses offscreen D3D12 readback on private Windows desktops. The existing 5% GPU timing target remains unconfirmed for Lattice Eco/Auto; see docs/CINEMATIC.md for measurements and limits.

