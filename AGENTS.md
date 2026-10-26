# Working on Voltura Worlds

## Development policy: no automated tests

Do not add, restore, maintain or run automated tests, regression suites, test harnesses, fixtures or test-only dependencies in this repository. The user has chosen development through code review, compiler feedback, actual app use and visual feedback. Removed tests remain in Git history. Only change this policy if the user explicitly asks to change it; do not reintroduce tests as an assumed prerequisite for development or release.

Keep production input validation, error handling, signature verification and the updater's checksum verification against GitHub release-asset metadata. These are application behavior, not automated development tests. Do not introduce a custom release checksum file or duplicate manual checksum-validation workflow.

## Prefer scripts for routine operations

Use scripts, command-line tools or process APIs for app shutdown, builds, packaging and relaunches. Reserve desktop control for verification that requires UI interaction.

## Closing the running app

You are always authorized and encouraged to close an existing Voltura Worlds instance when needed to build, replace binaries, package or verify changes. Do not ask for confirmation. Prefer closing the app and rebuilding the normal output over creating an alternate build solely to work around locked files.

Exit through the app's Exit Worlds action or another graceful shutdown mechanism first; closing the library window alone leaves the app running. If graceful shutdown fails, terminate the relevant Voltura Worlds process. Leave unrelated applications and processes running. Relaunch the updated app when needed to verify the requested changes.

## Iterate through visual feedback

Implement a small, reviewable increment, build and launch it programmatically, and exercise the changed behavior through its normal UI entry point. Check nearby interactions in the running app. Present the result early enough for the user's visual feedback to shape the next iteration.

Do not treat appearance or interaction as settled before the user has assessed it. Incorporate feedback and directional changes before polishing or preparing a release around that direction. Silence is not visual approval. Once the user accepts the direction, complete the authorized work without repeatedly asking for the same approval.

## Release efficiently

Value the user's time. Review the changed code, use the smallest useful verification in the actual app, and stop when the evidence is sufficient. Do not turn visual verification into an exhaustive repeated checklist.

Follow [the release checklist](docs/RELEASING.md). Build once and package once with `-SkipBuild`. Sign the app and installer. GitHub provides each uploaded asset's SHA-256 digest, which the updater validates automatically; do not create `SHA256SUMS.txt` or add separate download/hash comparison steps to the release procedure.

Regenerate previews only for changed visuals. After publication, check release metadata and changed site content. Keep reports brief and distinguish code review, build results, observed app behavior and anything still awaiting the user's visual assessment. Do not claim a UI or physical-display result that was not observed.

## Keep the workspace small

Treat `artifacts/` as temporary scratch space. Capture only the frames and scenes needed for the current visual review, and remove intermediate recordings, raw GPU dumps and old revision outputs when finished. Keep final website media in `site/assets/`. Do not accumulate installer copies, historical build layouts or texture-download caches that are no longer needed. Preserve signing configuration, required build dependencies, source assets and Git history during cleanup.
