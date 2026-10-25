# Voltura Worlds

**Your desktop, alive.** Native animated worlds for Windows 11.

[Product site](https://worlds.voltura.se/) · [Releases](https://github.com/voltura/voltura-worlds/releases) · [Using Worlds](docs/USING.md)

![Aurora rendered by Voltura Worlds](site/assets/aurora.jpg)

| Aurora | Orbit | Smoke | Bloom |
| --- | --- | --- | --- |
| Flowing ribbons of light | A ringed planet in motion | Evolving coloured fluid | Sculpted, unfolding curves |
| [Watch](https://worlds.voltura.se/assets/aurora.mp4) | [Watch](https://worlds.voltura.se/assets/orbit.mp4) | [Watch](https://worlds.voltura.se/assets/smoke.mp4) | [Watch](https://worlds.voltura.se/assets/bloom.mp4) |

| Silk | Eclipse | Lattice |
| --- | --- | --- |
| Translucent flowing ribbons | A slowly evolving solar corona | A rotating porous sculpture |
| [Watch](https://worlds.voltura.se/assets/silk.mp4) | [Watch](https://worlds.voltura.se/assets/eclipse.mp4) | [Watch](https://worlds.voltura.se/assets/lattice.mp4) |

| Drift | Contour | Mercury |
| --- | --- | --- |
| Bioluminescent currents | Living topographic lines | Merging metallic forms |
| [Watch](https://worlds.voltura.se/assets/drift.mp4) | [Watch](https://worlds.voltura.se/assets/contour.mp4) | [Watch](https://worlds.voltura.se/assets/mercury.mp4) |

| Ripples |
| --- |
| Reflective water, intersecting drops, wind ripples and a translucent pebbled bottom |
| [Watch](https://worlds.voltura.se/assets/ripples.mp4) |

The product site shows recordings from the native Direct3D renderer, at their original animation speed. 

## What it does

- Animates behind your desktop icons, with a World and settings for each display.
- Shuffles all sixteen Worlds with **Random**, with a saved interval for each display.
- Gives Earth, Mars, Moon, Jupiter and Saturn a shared NASA catalog-star background, with no extra resident sky texture memory.
- Offers optional Gentle or Expressive audio reaction using local playback analysis. Audio is not recorded or uploaded.
- Pauses on battery by default, and for fully covered displays, lock, sleep and remote sessions.
- Imports personal SDR H.264 MP4 videos and data-only `.world` packages.
- Provides a themed Windows library, advanced controls and a notification-area menu.
- Leaves your ordinary Windows wallpaper intact. Choose **Off** to reveal it, or **Exit Worlds** to stop the app.

Current source builds add **Mars** beside Orbit: an 8K NASA/USGS globe with MOLA terrain relief, thin atmospheric scattering, distance-based sunlight and calm rotation. Its advanced controls adjust terrain relief, camera drift, dust and atmosphere, with natural color grades and subtle audio response. See [Mars imagery and controls](assets/mars/ATTRIBUTION.md).

**Earth** adds a separate realistic 8K globe with NASA Blue Marble geography, independent cloud motion and shadows, ocean glints, Black Marble night lights and blue atmospheric scattering. It shares Mars's calm 15-minute rotation and adds Earth-specific cloud, camera and atmosphere controls. See [Earth sources and rendering](assets/earth/ATTRIBUTION.md).

**Moon** adds an 8K NASA LRO globe with LOLA crater relief and a calm 15-minute camera tour of the near and far sides. A naturally scaled Earth briefly appears beside the lunar limb; a fixed Sun produces changing phases. See [Moon sources and rendering](assets/moon/ATTRIBUTION.md).

**Jupiter** uses NASA/ESA Hubble OPAL December 2025 cloud imagery, matched methane-band atmospheric data and estimated zonal winds. Its oblate globe rotates calmly, with flowing cloud belts, a gently swirling Red Spot and a thin neutral haze. Compressed 4K maps preserve the source detail efficiently. See [Jupiter sources and rendering](assets/jupiter/ATTRIBUTION.md).

**Saturn** combines a flattened gold cloud globe with translucent, finely banded rings, mutual shadows and the shared stationary star background. Its full ring system stays within the frame on landscape and portrait displays. Rotation takes 15 minutes at default Movement. See [Saturn sources and rendering](assets/saturn/ATTRIBUTION.md).

## Current release

**Version 0.1.7** is available. [Download the Windows installer](https://github.com/voltura/voltura-worlds/releases/download/v0.1.7/VolturaWorlds-Setup-0.1.7-win-x64.exe) from [Releases](https://github.com/voltura/voltura-worlds/releases/tag/v0.1.7). Includes all eleven Worlds and personal MP4 video wallpapers. Setup requires no administrator access. GitHub's source ZIP does not contain a built app.

This release adds **Ripples**: reflective water with overlapping drop waves, gentle wind ripples, moving clouds, and a shallow-to-deep sandy bottom with varied pebbles. **Random** rotates through all eleven Worlds without immediate repeats, with a configurable interval for each display. The product site includes a native Ripples recording and an updated social preview.

Enable **Automatic updates** in the notification-area menu to download verified future releases in the background. Updates install on **Exit Worlds**; **Restart to update** appears when a download is ready. See [automatic updates](docs/UPDATES.md) for verification and behavior.


## Build and run

Requires Windows 11 x64, Visual Studio 2026 C++ desktop tools, the Windows SDK, PowerShell and Python 3.10 or newer with pip. NSIS is needed for packaging. The bootstrap script downloads and checks the native dependencies; install the Python test and media tools before building.

```powershell
git clone https://github.com/voltura/voltura-worlds.git
cd voltura-worlds
python -m pip install -r requirements-dev.txt
./scripts/build.ps1
./build/Release/VolturaWorlds.exe
```

The build script runs CTest after compilation. To produce a per-user installer:

```powershell
./scripts/package.ps1 -SkipBuild
```

Packaging does not run the installer. See [Using Worlds](docs/USING.md) for installation, local settings, playback and signing details. Closing the library keeps Worlds running; use **Exit Worlds** in the tray menu to quit.

## Product site

The independent, buildless product site lives in [`site/`](site/). It has no external fonts, tracking or runtime dependencies. GitHub Pages publishes it from the `gh-pages` branch at [worlds.voltura.se](https://worlds.voltura.se/).

See [website maintenance](docs/PAGES.md) for local preview, native media capture and publication.

## License and credits

[MIT](LICENSE), copyright © 2026 Voltura AB. Third-party dependencies and artwork retain their [own licenses and credits](THIRD-PARTY-NOTICES.md), including [Orbit's source imagery](assets/orbit/ATTRIBUTION.md).
