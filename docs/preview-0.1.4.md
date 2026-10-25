# Voltura Worlds 0.1.4 — Windows preview

The first public GitHub installer includes Aurora, Orbit, Smoke and Bloom, plus personal H.264 MP4 video wallpapers with silent looping and Fill/Fit.

## Download and install

Download **VolturaWorlds-Setup-0.1.4-win-x64.exe** from this release and run it. Windows 11 x64 and Direct3D 12 are required. Setup installs for your account without administrator access and offers Start with Windows. GitHub's source archives do not contain a built application.

Both the app and installer are signed with the existing self-signed Voltura AB certificate and timestamped. This certificate is not publicly trusted; Windows may show a publisher or reputation warning. `SHA256SUMS.txt` provides the installer checksum.

## Automatic updates

Enable **Automatic updates** in the notification-area menu. Worlds checks after five seconds and hourly, downloads newer normal GitHub releases in the background, and verifies the checksum, product/version and the same signing certificate. It is off by default. Uninstalled development copies do not update automatically.

Choose **Restart to update** when it appears, or **Exit Worlds** to install a ready update without restarting. Closing the library keeps Worlds running. Shutdown and sign-out cancel automatic installation. Your settings, library, imported videos and startup choice are retained. Details: [automatic updates](UPDATES.md).

## Preview limitations

This remains an early preview. Full adaptive quality, a shared multi-monitor rendering device, the community service and broader performance/endurance acceptance remain unfinished. Automated native tests and isolated installer checks do not establish physical desktop, audio-listening or mixed-DPI multi-monitor acceptance.

## Validation

- Release build and all 29 CTest checks passed.
- All 62 signed updater policy/staging/cancellation checks passed; modified signed payload rejected.
- Non-elevated isolated install, automatic signed 0.1.3-to-0.1.4 upgrade, startup opt-out, packaged native rendering/video, synthetic host recovery and uninstall passed.
- Both production executable and installer have verified Authenticode digests, the selected Voltura certificate and timestamps.

The installer lifecycle used an isolated product identity and private Windows desktop. Physical desktop and mixed-DPI acceptance remain unverified.
