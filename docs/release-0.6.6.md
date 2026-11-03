# Voltura Worlds 0.6.6

Updated Pillow from 12.3.0 to 13.0.0 in both optional tooling manifests, resolving the vulnerable dependency versions behind 82 Dependabot alerts (10 critical, 46 high and 26 medium). Pillow is used by texture and media tooling; the native application and installer do not bundle it.

The website now deploys from `main` through GitHub Actions, allowing the repository to use a single branch.

All 26 Worlds, personal video wallpapers and the sunlit Journey Moon from 0.6.5 are retained. No visual assets were regenerated.

Validation: dependency installation and Pillow imports, independent caller review, Release build, and signed packaging with `-SkipBuild` passed. GitHub reports all 82 alerts fixed and zero open alerts. The app, shell integration and installer use the existing timestamped self-signed certificate. No automated tests were added or run; no app UI or physical-display assessment was performed, and the release was not installed locally.
