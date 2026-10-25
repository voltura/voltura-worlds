# Voltura Worlds 0.1.5

Six new native Worlds join Aurora, Orbit, Smoke and Bloom:

- **Silk:** flowing, semi-transparent ribbons with layered colour.
- **Eclipse:** a dark disc surrounded by an evolving solar corona.
- **Lattice:** a slowly rotating porous mathematical sculpture.
- **Drift:** layered bioluminescent particles moving through gentle currents.
- **Contour:** animated topographic lines.
- **Mercury:** smoothly merging metallic forms with moving reflections.

The new Worlds support the existing palette, brightness, movement, detail, flow, turbulence, density, audio reaction and audio strength controls, along with global quality and frame limits. They are included in the library, tray menu, saved display assignments and data-only `.world` packages. The product site shows recordings from the native renderer.

## Download and install

Download **VolturaWorlds-Setup-0.1.5-win-x64.exe** from the [release](https://github.com/voltura/voltura-worlds/releases/tag/v0.1.5) and run it. Windows 11 x64 and Direct3D 12 are required. Setup installs for your account without administrator access and offers Start with Windows. GitHub's source archives do not contain a built application.

The application and installer use the existing self-signed Voltura AB certificate with timestamping. This certificate is not publicly trusted; Windows may show a publisher or reputation warning. `SHA256SUMS.txt` provides the installer checksum.

## Updating

Run the installer to upgrade while retaining settings, imported videos and the startup choice. Users who enabled **Automatic updates** receive the release through the existing verified update channel. Choose **Restart to update** when available, or **Exit Worlds** to apply a downloaded update without relaunching. See [automatic updates](UPDATES.md).

## Validation

- Release application and test targets build. Core settings/package roundtrips, capture-time contracts, Bloom regression, native library UI, desktop hosting, imports, video and updater policy checks pass.
- All six new Worlds pass native control-response checks for brightness, detail, flow, turbulence, density, palette and quality; audio Off/strength zero isolation, Gentle/Expressive response, animation and exact Movement 0 freezing pass. The initial combined run had one Lattice pixel-equality comparison fail while media recording was running; the complete isolated rerun passed without source changes.
- Portrait and ultrawide captures at high detail/density/turbulence were visually checked for corner seams, edge artifacts and sculpture framing. Twenty switches/resizes across all ten Worlds pass D3D12 debug validation.
- At 1920 × 1080 High quality on Intel Arc Graphics, 90-frame offscreen runs measured GPU p95 between 1.28 ms (Eclipse) and 3.28 ms (Drift). These short samples do not establish sustained physical desktop performance or power use.
- The site uses eight-second, 1600 × 900 native recordings. All ten videos, posters, play/pause and reduced-motion behavior pass browser checks at widths from 320 to 1920 pixels. The social card uses the same native output.
- The signed updater fixture passes 62 staging/cancellation checks and rejects a modified signed executable.
- Isolated per-user installation, signed automatic upgrade from 0.1.3 to 0.1.5, startup opt-out preservation, all six new packaged shaders, video playback, host recovery and uninstall pass. The application and production installer are signed and timestamped with the existing Voltura publisher certificate.

Focused release checks were used; the unrelated full Smoke simulation suite was not rerun. Native offscreen captures demonstrate rendered output and animation; they do not establish physical desktop presentation or sustained performance on every GPU.
