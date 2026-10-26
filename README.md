# Voltura Worlds

**Your desktop, alive.** Native animated worlds for Windows 11.

[Product site](https://worlds.voltura.se/) · [Releases](https://github.com/voltura/voltura-worlds/releases) · [Using Worlds](docs/USING.md)

![Earth rendered by Voltura Worlds](site/assets/earth.jpg)

| Aurora | Orbit | Smoke | Bloom |
| --- | --- | --- | --- |
| Flowing ribbons of light | A ringed planet in motion | Evolving coloured fluid | Sculpted, unfolding curves |
| [Watch](https://worlds.voltura.se/assets/aurora.mp4) | [Watch](https://worlds.voltura.se/assets/orbit.mp4) | [Watch](https://worlds.voltura.se/assets/smoke.mp4) | [Watch](https://worlds.voltura.se/assets/bloom.mp4) |

| Silk | Eclipse | Lattice |
| --- | --- | --- |
| Translucent flowing ribbons | A slowly evolving solar corona | A rotating porous sculpture |
| [Watch](https://worlds.voltura.se/assets/silk.mp4) | [Watch](https://worlds.voltura.se/assets/eclipse.mp4) | [Watch](https://worlds.voltura.se/assets/lattice.mp4) |

| Drift | Contour | Plastic metal |
| --- | --- | --- |
| Bioluminescent currents | Living topographic lines | Merging metallic forms |
| [Watch](https://worlds.voltura.se/assets/drift.mp4) | [Watch](https://worlds.voltura.se/assets/contour.mp4) | [Watch](https://worlds.voltura.se/assets/plastic_metal.mp4) |

| Ripples |
| --- |
| Reflective water, intersecting drops, wind ripples and a translucent pebbled bottom |
| [Watch](https://worlds.voltura.se/assets/ripples.mp4) |

The eleven new Solar System Worlds each have an eight-second native preview:

| World and preview | Explore |
| --- | --- |
| [Mercury](https://worlds.voltura.se/assets/mercury.mp4) | Sunlit craters. A world of extremes. |
| [Venus](https://worlds.voltura.se/assets/venus.mp4) | Golden clouds. A hidden world. |
| [Earth](https://worlds.voltura.se/assets/earth.mp4) | Blue oceans. Clouds in motion. Home. |
| [Mars](https://worlds.voltura.se/assets/mars.mp4) | Red deserts. A different horizon. |
| [Jupiter](https://worlds.voltura.se/assets/jupiter.mp4) | Cloud belts. A storm that keeps turning. |
| [Saturn](https://worlds.voltura.se/assets/saturn.mp4) | Golden light. Rings full of detail. |
| [Uranus](https://worlds.voltura.se/assets/uranus.mp4) | Pale cyan. A world on its side. |
| [Neptune](https://worlds.voltura.se/assets/neptune.mp4) | Blue winds. White clouds. A distant storm. |
| [Moon](https://worlds.voltura.se/assets/moon.mp4) | Cratered light. A tour of the near and far sides. |
| [Pluto](https://worlds.voltura.se/assets/pluto.mp4) | A bright heart. At the edge of the familiar. |
| [Sun](https://worlds.voltura.se/assets/sun.mp4) | Ultraviolet light. Plasma in motion. |

The product site shows recordings from the native Direct3D renderer, at their original animation speed. 

**Sun** adds an offline NASA/SDO ultraviolet observation with animated plasma, loops, and adjustable corona. See [Sun settings and source details](docs/sun.md).

## What it does

- Animates behind your desktop icons, with a World and settings for each display.
- Shuffles all built-in Worlds with **Random**, with a saved interval for each display.
- Cycles through the eight planets, Moon, Pluto and Sun with **Random Solar System**.
- Offers optional palette-matched **body statistics** in the lower-right corner, saved per display. See [facts, sources and distance conventions](docs/body-statistics.md).
- Gives Earth, Mars, Moon, Jupiter, Saturn, Mercury, Venus, Uranus and Neptune a shared NASA catalog-star background, with no extra resident sky texture memory.
- Offers optional Gentle or Expressive audio reaction using local playback analysis. Audio is not recorded or uploaded.
- Pauses on battery by default, and for fully covered displays, lock, sleep and remote sessions.
- Imports personal SDR H.264 MP4 videos and data-only `.world` packages.
- Saves named whole-desktop presets, with display mapping and portable `.world` export including videos. See [saved presets](docs/PRESETS.md).
- Provides a themed Windows library, advanced controls and a notification-area menu.
- Leaves your ordinary Windows wallpaper intact. Choose **Off** to reveal it, or **Exit Worlds** to stop the app.

**Mars** adds an 8K NASA/USGS globe with MOLA terrain relief, thin atmospheric scattering, distance-based sunlight and calm rotation. Its advanced controls adjust terrain relief, camera drift, dust and atmosphere, with natural color grades and subtle audio response. See [Mars imagery and controls](assets/mars/ATTRIBUTION.md).

**Earth** adds a separate realistic 8K globe with NASA Blue Marble geography, independent cloud motion and shadows, ocean glints, Black Marble night lights and blue atmospheric scattering. It shares Mars's calm 15-minute rotation and adds Earth-specific cloud, camera and atmosphere controls. See [Earth sources and rendering](assets/earth/ATTRIBUTION.md).

**Moon** adds an 8K NASA LRO globe with LOLA crater relief and a calm 15-minute camera tour of the near and far sides. A naturally scaled Earth briefly appears beside the lunar limb; a fixed Sun produces changing phases. See [Moon sources and rendering](assets/moon/ATTRIBUTION.md).

**Jupiter** uses NASA/ESA Hubble OPAL December 2025 cloud imagery, matched methane-band atmospheric data and estimated zonal winds. Its oblate globe rotates calmly, with flowing cloud belts, a gently swirling Red Spot and a thin neutral haze. Compressed 4K maps preserve the source detail efficiently. See [Jupiter sources and rendering](assets/jupiter/ATTRIBUTION.md).

**Saturn** combines a flattened gold cloud globe with translucent, finely banded rings, mutual shadows and the shared stationary star background. Its full ring system stays within the frame on landscape and portrait displays. Rotation takes 15 minutes at default Movement. See [Saturn sources and rendering](assets/saturn/ATTRIBUTION.md).

**Venus** shows opaque cream-and-gold cloud decks, slow retrograde atmospheric motion and a warm scattering limb against the shared stationary stars. Cloud detail, camera drift, cloud drift and atmosphere controls preserve its thick cloud cover. This is a procedural artistic reconstruction; see [Venus sources and rendering](docs/venus.md).

**Uranus** shows a pale cyan, subtly banded cloud globe rotating around its 97.8° tilted axis, with fixed sunlight, a centered camera and stationary stars. Planet-specific palette, cloud, atmosphere and audio controls preserve the calm framing. See [Uranus sources and rendering](docs/uranus.md).

**Neptune** shows a cinematic blue ice giant with fast white cloud streaks, latitude-dependent winds and a swirling dark storm. Four palettes include a natural blue-green interpretation. The opaque globe and narrow atmosphere sit against stationary stars; see [Neptune sources and rendering](docs/neptune.md).

**Mercury** adds an 8K MESSENGER globe with measured terrain relief, a natural gray-brown surface, strong sunlight and no atmospheric halo. It rotates in 15 minutes and shares the stationary catalog-star sky. Controls adjust terrain relief, camera drift, Sun angle and surface contrast. The older metallic World is now **Plastic metal**. See [Mercury sources and rendering](assets/mercury/ATTRIBUTION.md).

**Pluto** combines the original New Horizons heart-region imagery with a detailed artistic reconstruction of the under-resolved far side and unobserved south, a restrained blue limb and stationary catalog stars. A rotation takes 15 minutes at default Movement. See [Pluto imagery and rendering limits](assets/pluto/ATTRIBUTION.md).

## Current release

**Version 0.1.9** is available. [Download the Windows installer](https://github.com/voltura/voltura-worlds/releases/download/v0.1.9/VolturaWorlds-Setup-0.1.9-win-x64.exe) from [Releases](https://github.com/voltura/voltura-worlds/releases/tag/v0.1.9). Includes all **22 Worlds** and personal MP4 video wallpapers. Setup requires no administrator access. GitHub's source ZIP does not contain a built app.

This release repairs Pluto's lower texture seam and reconstructs sharper far-side terrain to match the encounter side's visual clarity. The original heart region is retained. Reconstructed areas are artistic imagery, not additional spacecraft measurements. Includes all 22 Worlds and the existing Solar System features.

**Upgrading from 0.1.7 or earlier:** download and run this installer manually. The larger texture collection exceeds those versions' 64 MiB automatic-update limit. Existing settings, imported videos and startup preferences are retained. Version 0.1.8 supports larger future update downloads.

Enable **Automatic updates** in the notification-area menu to download verified future releases in the background. Updates install on **Exit Worlds**; **Restart to update** appears when a download is ready. See [automatic updates](docs/UPDATES.md) for verification and behavior.


## Build and run

Requires Windows 11 x64, Visual Studio 2026 C++ desktop tools, the Windows SDK, PowerShell and Python 3.12 or newer with pip. NSIS is needed for packaging. The bootstrap script downloads and checks the native dependencies; install the Python test and media tools before building.

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
