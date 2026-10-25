# Installer validation

Build the development installer with `./scripts/package.ps1`.

From a non-elevated PowerShell session, run `./scripts/test-installer.ps1`.
The test requires that Worlds is not already installed or running. It refuses to overwrite existing associations, startup settings, shortcuts or installation paths. It creates a temporary per-user installation, checks two upgrades and the packaged renderer, then uninstalls while verifying that personal library data remains intact.

Evidence is written to `artifacts/installer-lifecycle/results.json`; native captures are under `installed-runtime/`. These are separate from physical desktop interaction and visual acceptance.

Silent installation supports `/S /NOSTARTUP`. Without `/NOSTARTUP`, a fresh installation enables startup and an upgrade preserves the existing choice. NSIS `/D=` can select a directory and must be the last argument. Ordinary upgrades remember the previously installed directory.

The current executable is an unsigned development build. Signing and installed desktop release-candidate validation remain outstanding.

The lifecycle test also runs `worlds_desktop_tests.exe` against the installed application. It creates a private synthetic Explorer-style host, verifies delayed attachment and recovery after host destruction, and requests a clean application exit. Build the test executable with `scripts/build.ps1` before standalone installer validation. This does not restart or manipulate the user's real Explorer session.
