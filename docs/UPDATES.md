# Automatic updates

Enable **Automatic updates** in the Worlds tray menu. It is off by default and saved separately in `%LOCALAPPDATA%\Voltura\Worlds\preferences.ini`, so the ordinary settings writer cannot overwrite the choice. Disabling cancels downloads and prevents installation.

The first background check runs after five seconds, then hourly while Worlds is open. It follows the latest published, non-draft, non-prerelease release in `voltura/voltura-worlds`. Numeric versions must be newer. Installed identity requires an exact match between the executable path and the HKCU installation registration. Uninstalled development copies never update automatically.

Downloads and verification run below normal thread priority. Only HTTPS GitHub release hosts are accepted, with bounded redirects, timeouts, a ten-minute transfer deadline and a 512 MiB installer limit. Errors remain quiet and are retried on a later check. There is no service or scheduled task.

Install manually when upgrading from 0.1.7 or earlier: its 64 MiB download limit is too small for the Solar System texture collection.

The installer must match the `sha256:` digest supplied by GitHub for the release asset, product/company/version metadata, expected original filename and the running application's signing certificate. Authenticode verifies the signature and timestamp. The only trust exception is the same pinned self-signed publisher's untrusted root. Modified payloads, another publisher, unsigned builds and downgrades cannot update. Certificate changes require a manual installation.

Verified downloads are staged in `%LOCALAPPDATA%\Voltura\Worlds\Updates`, separated by executable path. Partial downloads cannot install. **Restart to update** appears in the tray menu when ready; it exits normally, applies the verified installer and relaunches quietly behind the desktop. **Exit Worlds** also applies a ready update, without relaunching. Closing the library keeps Worlds running and does not apply the update.

The signed helper waits for the app to exit and verifies the candidate again. Reopening before installation cancels the waiting update; an installation already applying briefly delays startup. The installer preserves startup opt-out and the selected installation directory. Executable replacement uses staging and rollback on failure. User scenes, imported videos and settings are retained.

Windows shutdown/sign-out cancels installation, including a subsequently cancelled shutdown. The helper and installer check the inherited cancellation event and Windows shutdown state. No elevation or reboot is requested.

For publication, upload the signed installer. GitHub computes its SHA-256 digest automatically and exposes it in release-asset metadata. Worlds uses that metadata directly; no separate checksum file is required. Worlds follows the normal numbered release channel; explicitly marked GitHub prereleases are excluded from automatic updates.

## Development workflow

Use code review and focused verification of the running app. Build and sign through the release scripts; follow the [release checklist](RELEASING.md). Automatic updates still verify the downloaded SHA-256 against GitHub metadata, Authenticode signature/timestamp, signing identity and product/version metadata before staging and applying a payload.
