# Voltura Worlds

**Your desktop, alive.** Native animated worlds for Windows 11.

[Product site](https://worlds.voltura.se/) · [Releases](https://github.com/voltura/voltura-worlds/releases) · [Using Worlds](docs/USING.md)

![Aurora rendered by Voltura Worlds](site/assets/aurora.jpg)

| Aurora | Orbit | Smoke | Bloom |
| --- | --- | --- | --- |
| Flowing ribbons of light | A ringed planet in motion | Evolving coloured fluid | Sculpted, unfolding curves |
| [Watch](https://worlds.voltura.se/assets/aurora.mp4) | [Watch](https://worlds.voltura.se/assets/orbit.mp4) | [Watch](https://worlds.voltura.se/assets/smoke.mp4) | [Watch](https://worlds.voltura.se/assets/bloom.mp4) |

The product site shows recordings from the native Direct3D renderer, at their original animation speed. These are native offscreen captures, not recordings of physical desktop interaction.

## What it does

- Animates behind your desktop icons, with a World and settings for each display.
- Offers optional Gentle or Expressive audio reaction using local playback analysis. Audio is not recorded or uploaded.
- Pauses on battery by default, and for fully covered displays, lock, sleep and remote sessions.
- Imports personal SDR H.264 MP4 videos and data-only `.world` packages.
- Provides a themed Windows library, advanced controls and a notification-area menu.
- Leaves your ordinary Windows wallpaper intact. Choose **Off** to reveal it, or **Exit Worlds** to stop the app.

## Current release

**Version 0.1.4** is available. [Download the Windows installer](https://github.com/voltura/voltura-worlds/releases/download/v0.1.4/VolturaWorlds-Setup-0.1.4-win-x64.exe) from [Releases](https://github.com/voltura/voltura-worlds/releases/tag/v0.1.4). Includes all four Worlds and personal MP4 video wallpapers. Setup requires no administrator access. GitHub's source ZIP does not contain a built app.

Enable **Automatic updates** in the notification-area menu to download verified future releases in the background. Updates install on **Exit Worlds**; **Restart to update** appears when a download is ready. See [automatic updates](docs/UPDATES.md) for verification and behavior.

The community site, complete adaptive quality, shared multi-monitor rendering device and broader reliability/performance acceptance remain unfinished. Physical desktop and mixed-DPI multi-monitor verification remain open. Historical checkpoints and their limitations are in [docs/STATUS.md](docs/STATUS.md).

## Build and run

Requires Windows 11 x64, Visual Studio 2026 C++ desktop tools, the Windows SDK and PowerShell. NSIS is needed for packaging. Dependencies are downloaded and checked by the bootstrap script.

```powershell
git clone https://github.com/voltura/voltura-worlds.git
cd voltura-worlds
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
