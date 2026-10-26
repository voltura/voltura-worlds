# Voltura Worlds 0.1.11

This maintenance release includes 81 documented fixes across settings, presets,
video imports, audio, display recovery and the installer.

- Preserve unreadable settings for recovery, retry failed saves and finish pending
  settings writes on exit.
- Improve preset import, export, cancellation and rollback; strengthen package
  validation and retain the correct display selection after recovery.
- Improve audio capture recovery and react to opposite-phase stereo channels
  without cancelling their signal. Refresh audio output choices as devices change.
- Correct Random timing after sleep and display recovery, and improve monitor,
  graphics and tray recovery.
- Improve DPI handling, scrolling, metadata editing and consistent outer spacing
  in the library and Presets windows. Show Audio off for video and Off.
- Use “1 display” and “n displays” for saved preset display counts.
- Recognize more valid QuickTime files, improve video metadata handling and bound
  thumbnails for extreme aspect ratios.
- Preserve other applications' file associations during uninstall and prevent
  concurrent setup/uninstall operations.
- Use GitHub's release-asset SHA-256 metadata for updater checksum verification,
  alongside signature and product/version verification. No custom checksum file
  is required.

All 22 Worlds remain included. Scene artwork and website previews are unchanged.

The app and installer are signed and timestamped with Voltura's self-signed
certificate; this certificate is not publicly trusted.

Development uses source review, compiler feedback and focused use of the running
app. Automated tests have been removed under the repository's development policy.
The [audit](bug-review-100.md) records the individual fixes and verification limits;
hardware, failure and shutdown scenarios were not all exercised.

Release verification: the final Release build passed. The rebuilt library opened
and Presets loaded the existing saved preset list through its normal library
button. The spacing is visible in the running app; the user's visual assessment
remains pending.
