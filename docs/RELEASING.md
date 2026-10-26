# Release checklist

Development uses code review, compiler feedback, the running app and the user's visual feedback. This repository does not use automated tests. See [repository instructions](../AGENTS.md).

## 1. Review and visually confirm the change

Compare the intended release against the previous published release, including pending changes. Review affected code and dependencies and build the app. Never use computer use or desktop/UI automation for a release. Use scripts, command-line tools and process APIs for release operations. Visual assessment belongs to the user and is not an automated release gate; incorporate any feedback before publication.

Use the smallest useful verification and stop when there is sufficient evidence. Do not add regression suites, test fixtures, screenshot assertion matrices or repeated exhaustive app walkthroughs. Existing visual acceptance remains useful when relevant behavior has not changed.

## 2. Build, sign and package once

Update version references and release notes. Build the final Release output if needed, then package it:

```powershell
./scripts/build.ps1
./scripts/package.ps1 -SkipBuild -SigningConfig <local-config.json>
```

Build reports its elapsed time and compiles only the application and its required libraries/shaders. Packaging reports its own elapsed time, signs the application and installer, and writes version, filename, size and signing information to `dist/release-manifest.json`. Without `-SkipBuild`, packaging builds first. It never installs the app.

Authenticode signing and timestamp verification are handled by the signing scripts. A timestamped self-signed certificate is not publicly trusted. Keep production validation and updater verification intact.

## 3. Publish the accepted result

Publish when authorized by the user. Upload the signed installer and release manifest. GitHub automatically supplies the uploaded asset's `sha256:` digest; the updater compares its download against that digest and verifies its signature and product/version identity. No separate checksum file, compatibility path or manual checksum comparison is required.

Regenerate previews/posters only for changed visuals, then update relevant site content and download links following [website maintenance](PAGES.md). Check the published release metadata and changed site content after propagation. Do not add repeated full downloads or unrelated media review as routine release gates.

Report what changed, build/signing results, what was actually observed in the running app, and any visual feedback still needed. Do not substitute automated output or generated screenshots for the user's acceptance of the app.
