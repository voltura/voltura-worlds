# Voltura Worlds 0.1.4 for Windows

A per-user Windows 11 x64 desktop application with four built-in Worlds: **Aurora**, **Orbit**, **Smoke**, and **Bloom**. C++20, Direct3D 12, HLSL. MIT licensed.

## Install and try it

Run `dist/VolturaWorlds-Setup-0.1.4-win-x64.exe`. No administrator account is required. The installer offers **Start with Windows** and opens the library when you choose **Start Voltura Worlds** on the final page.

Choose a display, then Aurora, Orbit, Smoke, or Bloom. Changes apply immediately behind the desktop icons. Choose **Off** in the visualization dropdown or tray menu to reveal your normal Windows desktop while Worlds stays running. The choice is saved per display. **Apply to all displays** copies the current selection and settings. If the desktop connection fails, Worlds retries automatically and shows **Retry now** for an immediate attempt. Preview mode shows **Use on desktop** instead; the button is hidden during normal desktop use.

Closing the library leaves the background running. Reopen it through the notification-area icon or the Start-menu shortcut. Use **Pause**, **Resume**, or **Exit Worlds** to control playback. Exit reveals your ordinary Windows wallpaper; Worlds does not overwrite it.

The library follows your Windows light/dark theme. **Advanced settings** reveals detailed artwork and playback controls. **Reset settings** restores the selected World’s adjustments and global playback preferences without deleting imports or changing assigned scenes.

Audio reaction defaults to Off. Choose Gentle or Expressive and adjust Audio strength to use local playback-device analysis. No audio is recorded or uploaded. Rendering pauses on battery by default; clear **Pause on battery** to animate while unplugged. Full display coverage, lock, sleep, suspend, and remote sessions also pause rendering.

Personal SDR H.264 MP4 videos can be imported, copied to the managed library, and looped silently with Fill or Fit. Data-only `.world` packages open an import preview before applying. There is no community sharing service.

User settings and imported assets live in `%LOCALAPPDATA%\Voltura\Worlds`. Uninstall from Windows Settings; exit Worlds first. Uninstall asks whether to keep your library and preferences.

Bloom is an original animated folded-sheet World inspired by the blue Windows 11 wallpaper. It uses native 3D geometry with slow breathing, curling, and rocking movement, four palettes, and optional audio reaction. Select **Bloom** in the visualization list or tray menu. The 0.1.4 installer includes Bloom.

Enable **Automatic updates** in the notification-area menu for background update downloads. Choose **Restart to update** when available, or **Exit Worlds** to install a verified download on exit. Updates are off by default. See [UPDATES.md](UPDATES.md).

## Current limitations

The current release includes all four built-in Worlds and personal video wallpapers. A community sharing service, full adaptive-quality system and shared multi-monitor rendering device are not included.

Automated validation covers native rendering, app contracts, signed automatic updates and isolated installer lifecycle. Physical desktop, mixed-DPI multi-monitor and endurance acceptance remain unverified.

## Build

Windows 11 x64, Visual Studio 2026 C++ desktop tools, Windows SDK, PowerShell, and NSIS are required for development. Application dependencies are packaged locally; Python and build tools are not needed by users.

```powershell
./scripts/build.ps1
./scripts/package.ps1 -SkipBuild
```

`package.ps1` optionally accepts `-SigningConfig` pointing to a local JSON configuration with `certificateThumbprint`, `timestampUrl`, and `allowSelfSigned`. It builds the installer; it never executes it. `dist/release-manifest.json` records the final hash and signing status.

Normal launch attaches to the desktop and opens the library. `--wallpaper` starts quietly; `--preview` opens a separate development preview surface.

Native rendering diagnostics and historical visual experiments are documented in `docs/STATUS.md`. Capture-only experimental flags are not product quality settings.
