# Voltura Worlds 0.1.8 for Windows

The per-user Windows 11 x64 app includes **22 built-in Worlds**: Aurora, Orbit, Smoke, Bloom, Silk, Eclipse, Lattice, Drift, Contour, Plastic metal, Ripples, Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune, Moon, Pluto and Sun. C++20, Direct3D 12, HLSL. MIT licensed.

Updating from 0.1.7 or earlier requires one manual installer download because the Solar System textures exceed the old updater’s size limit. Settings, imports and startup preferences are retained.

## Install and try it

Run `dist/VolturaWorlds-Setup-0.1.8-win-x64.exe`. No administrator account is required. The installer offers **Start with Windows** and opens the library when you choose **Start Voltura Worlds** on the final page.

Choose a display, then a World. Changes apply immediately behind the desktop icons. Choose **Off** in the visualization dropdown or tray menu to reveal your normal Windows desktop while Worlds stays running. The choice is saved per display. **Apply to all displays** copies the current selection and settings. If the desktop connection or a renderer fails, Worlds retries automatically and shows **Retry now** for an immediate attempt. Preview mode shows **Use on desktop** instead; the button is hidden during healthy desktop use. A failed video or renderer is retried per display; switching to another World recovers immediately while other displays keep animating.

Choose **Random** to shuffle through all built-in Worlds, showing each once before reshuffling. The interval beside the visualization selector defaults to **Every 5 minutes**, with choices from 30 seconds to 30 minutes. Each display saves its own Random mode and interval. The countdown pauses with playback. Selecting a specific World, Off, or an imported video stops the shuffle.

Choose **Random Solar System** to cycle through the eight planets, Moon, Pluto and Sun. Enable **Body statistics** for palette-matched facts in the lower-right corner, saved per display. See [statistics and sources](body-statistics.md).

Closing the library leaves the background running. Reopen it through the notification-area icon or the Start-menu shortcut. Use **Pause**, **Resume**, or **Exit Worlds** to control playback. Exit reveals your ordinary Windows wallpaper; Worlds does not overwrite it.

The library follows your Windows light/dark theme. **Advanced settings** reveals detailed artwork and playback controls and makes the library translucent so you can see the World while adjusting it. **Reset settings** restores the selected World’s adjustments and global playback preferences without deleting imports or changing assigned scenes.

Audio reaction defaults to Off. Choose Gentle or Expressive and adjust Audio strength to use local playback-device analysis. No audio is recorded or uploaded. Rendering pauses on battery by default; clear **Pause on battery** to animate while unplugged. Full display coverage, lock, sleep, suspend, and remote sessions also pause rendering.

Personal SDR H.264 MP4 videos can be imported, copied to the managed library, and looped silently with Fill or Fit. Data-only `.world` packages open an import preview before applying.

User settings and imported assets live in `%LOCALAPPDATA%\Voltura\Worlds`. Uninstall from Windows Settings; exit Worlds first. Uninstall asks whether to keep your library and preferences.

Bloom is an original animated folded-sheet World inspired by the blue Windows 11 wallpaper. It uses native 3D geometry with slow breathing, curling, and rocking movement, four palettes, and optional audio reaction.

**Mars** shows a complete globe using NASA/USGS Viking imagery and MOLA terrain data. Brightness controls exposure; Movement controls its 15-minute rotation and slow drift; Terrain relief adjusts surface normals; Camera drift adjusts the framing motion; Dust variation changes suspended-dust contrast; Atmosphere adjusts the thin haze. The four Mars palettes preserve natural surface colors. Gentle and Expressive audio modes modulate atmospheric scattering, scaled by Audio strength. Movement at zero freezes the scene and audio response. Sunlight uses a fixed mean distance of 1.524 AU and a fixed orbital composition. See [source data and reproduction](../assets/mars/ATTRIBUTION.md).

**Ripples** shows calm reflective water from above, with scattered drops producing expanding concentric waves that overlap and interfere. Overlaps share one displaced surface: crests reinforce, opposite phases cancel, and reflections and refraction follow the combined shape. Gentle wind produces drifting patches of small, curved ripples between the drops, while clouds move across the reflections and softly shade the water. A sandy, pebbled bottom slopes from clear foreground shallows into deeper tinted water; surface motion refracts its fixed features, and depth gradually reduces their visibility. Density adjusts drop frequency, Flow controls wave and wind speed, Detail controls drop-wave fineness, and Turbulence adjusts the wind disturbance. Movement at zero freezes the water and clouds together. It supports the shared palettes, quality settings, and optional audio reaction. Higher quality refines the surface intersection as well as the small water texture.

Version 0.1.5 adds Silk's translucent flowing ribbons, Eclipse's glowing corona, Lattice's porous rotating sculpture, Drift's bioluminescent currents, Contour's living topography, and Plastic metal's merging metallic forms. Each supports the existing palettes, brightness, movement, detail, flow, turbulence, density, and optional audio reaction with Audio strength. Quality adjusts rendering detail; Frame limit controls the shared 30/60 FPS cadence. Movement at zero freezes procedural motion. Lattice and Plastic metal use bounded mathematical surfaces rather than physical material simulation.

Enable **Automatic updates** in the notification-area menu for background update downloads. Choose **Restart to update** when available, or **Exit Worlds** to install a verified download on exit. Updates are off by default. See [UPDATES.md](UPDATES.md).

## Build

Windows 11 x64, Visual Studio 2026 C++ desktop tools, Windows SDK, PowerShell, Python 3.12 or newer with pip, and NSIS are required for development. Install the Python test and capture tools before building. Application dependencies are packaged locally; Python and build tools are not needed by users.

```powershell
python -m pip install -r requirements-dev.txt
./scripts/build.ps1
./scripts/package.ps1 -SkipBuild
```

`package.ps1` optionally accepts `-SigningConfig` pointing to a local JSON configuration with `certificateThumbprint`, `timestampUrl`, and `allowSelfSigned`. It builds the installer; it never executes it. `dist/release-manifest.json` records the final hash and signing status.

Normal launch attaches to the desktop and opens the library. `--wallpaper` starts quietly; `--preview` opens a separate development preview surface.

Native rendering diagnostics and historical visual experiments are documented in `docs/STATUS.md`. Capture-only experimental flags are not product quality settings.

**Moon** uses NASA LROC 2025 color and LOLA terrain for an 8K lunar globe. Its camera circles the tidally locked Moon in 15 minutes, revealing the far side and changing phases under a fixed Sun. Earth appears briefly at its natural scale and distance. Advanced controls adjust Terrain relief, Camera drift, Sun angle and Earth brightness. Audio gently affects exposure; Movement zero freezes the scene and audio. See [Moon sources and rendering limits](../assets/moon/ATTRIBUTION.md).

**Jupiter** shows the observed December 2025 Hubble OPAL cloud deck on its measured oblate shape. The Great Red Spot starts facing the viewer; a complete turn takes 15 minutes at Movement 1. Cloud detail, Camera drift, Cloud drift and Atmosphere adjust image contrast, framing, wind-derived cloud flow with a gently swirling Red Spot, and neutral haze. Methane-band observations guide a high-cloud proxy, rather than fabricated terrain. Movement zero freezes motion/audio. See [Jupiter sources and limitations](../assets/jupiter/ATTRIBUTION.md).

**Saturn** shows a muted gold cloud globe with translucent rings, fine radial bands, the Cassini Division and shadows in both directions. The complete tilted ring system remains inside the frame on portrait and landscape displays. A full rotation takes 15 minutes at Movement 1. Cloud detail, Camera drift, Cloud drift and Atmosphere adjust cloud contrast, bounded framing drift, subtle cloud motion and haze. Its four palettes start with Natural Saturn. Audio gently changes planet/ring illumination while stars remain stationary; Movement zero freezes motion and audio. See [Saturn imagery, ring model and attribution](../assets/saturn/ATTRIBUTION.md).

Earth, Mars, Moon, Jupiter and Saturn include a restrained NASA catalog-star foreground. Stars stay fixed while Earth, Mars, Jupiter and Saturn rotate; the Moon’s stars follow its orbiting camera. The shared layer adds no extra runtime sky texture allocation.

## Pluto

Pluto starts on its heart region and rotates over 15 minutes. Brightness, Movement, Surface detail, Camera drift, Sun angle, Atmosphere and Audio strength tune the view. Surface detail changes photographic contrast, without simulated terrain relief. Four palettes start with Natural Pluto. Movement zero freezes motion and audio. Select Pluto in the library or tray; per-display selection, Apply to all, Random and World import/export work as for the other planets. See [Pluto sources and coverage limits](../assets/pluto/ATTRIBUTION.md).
