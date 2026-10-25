# Automatic updates

Worlds adapts the native updater from Voltura Markdown, the closest sister app: C++, a per-user NSIS installation, a notification-area lifetime, and the same Voltura signing certificate. Web-only sister apps and Voltura Air's different runtime were less suitable.

Enable **Automatic updates** in the Worlds tray menu. It is off by default and saved separately in `%LOCALAPPDATA%\Voltura\Worlds\preferences.ini`, so the ordinary settings writer cannot overwrite the choice. Disabling cancels downloads and prevents installation.

The first background check runs after five seconds, then hourly while Worlds is open. It follows the latest published, non-draft, non-prerelease release in `voltura/voltura-worlds`. Numeric versions must be newer. Installed identity requires an exact match between the executable path and the HKCU installation registration. Uninstalled development copies never update automatically.

Downloads and verification run below normal thread priority. Only HTTPS GitHub release hosts are accepted, with bounded redirects, timeouts, a two-minute transfer deadline and a 64 MiB installer limit. Errors remain quiet and are retried on a later check. There is no service or scheduled task.

The installer must match its entry in `SHA256SUMS.txt`, product/company/version metadata, expected original filename and the running application's signing certificate. Authenticode verifies the signature and timestamp. The only trust exception is the same pinned self-signed publisher's untrusted root. Modified payloads, another publisher, unsigned builds and downgrades cannot update. Certificate changes require a manual installation.

Verified downloads are staged in `%LOCALAPPDATA%\Voltura\Worlds\Updates`, separated by executable path. Partial downloads cannot install. **Restart to update** appears in the tray menu when ready; it exits normally, applies the verified installer and relaunches quietly behind the desktop. **Exit Worlds** also applies a ready update, without relaunching. Closing the library keeps Worlds running and does not apply the update.

The signed helper waits for the app to exit and verifies the candidate again. Reopening before installation cancels the waiting update; an installation already applying briefly delays startup. The installer preserves startup opt-out and the selected installation directory. Executable replacement uses staging and rollback on failure. User scenes, imported videos and settings are retained.

Windows shutdown/sign-out cancels installation, including a subsequently cancelled shutdown. The helper and installer check the inherited cancellation event and Windows shutdown state. No elevation or reboot is requested. Tests of session messages use isolated test windows rather than shutting down the computer.

For publication, upload the matching installer and lowercase SHA-256 `SHA256SUMS.txt` together. Worlds follows the normal numbered release channel; explicitly marked GitHub prereleases are excluded from automatic updates.

## Verification

`./scripts/build.ps1` runs native rendering, desktop, imports, video, UI and updater policy checks. `./scripts/test-updates.ps1 -SigningConfig <local-config.json>` tests signed staging, superseding releases, interrupted downloads, cancellation and modified-payload rejection.

`./scripts/test-installer-release.ps1 -SigningConfig <local-config.json>` compiles a separately identified signed NSIS installer and tests a signed 0.1.3 baseline upgrading to 0.1.6 through the real helper. It also checks installation, ordinary upgrades, startup opt-out, all ten packaged Worlds and video, host recovery and uninstallation. Its private desktop, registration, shortcut, file association and installation directory are isolated from the regular installation. The temporary opt-in is restored afterward.
