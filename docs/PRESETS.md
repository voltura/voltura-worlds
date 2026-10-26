# Desktop presets and World version 2

The native library's Presets window owns one cancellable worker at a time. It snapshots connected display assignments under the application state mutex, then performs hashing, package I/O, media installation and decoder/GPU preflight off the UI thread. Completion returns to the UI thread, rechecks mapped monitor IDs and adapter LUIDs, and commits all mapped assignments and global preferences under one mutex. The existing settings-save queue persists that consistent state. Playback continues during save/export; transient playback state is excluded.

## Persistence

`%LOCALAPPDATA%\Voltura\Worlds\presets\{GUID}.world` is a self-contained version-2 archive. The GUID is independent of the user-visible name and remains stable across updates and renames. Name comparison uses Windows ordinal case-insensitive Unicode comparison. Archive replacement uses a unique sibling pending file and atomic Windows replacement. Cancel/error removes the pending file and preserves the previous archive. Cancellation applies until the final commit boundary.

Applying a preset copies mapped media into a fresh managed library directory, preserving the detected container's file extension. All mapped videos must pass GPU preflight and saved-speed validation before settings change. Failed applications remove the newly installed media; successful applications retain their managed media independently of the preset archive. Deleting a preset therefore cannot break the active desktop. Original video files are never referenced by persisted package metadata.

A damaged preset is listed as unavailable and can be deleted; it does not prevent browsing or saving other presets.

The normal Presets form uses a compact preset list so all fields and actions fit without an outer scrollbar. The list scrolls independently when needed. Smaller windows and long display mappings use an app-owned, buffered scrollbar rather than repainting a native scrollbar after Windows draws it. Scrollable edit/list fields retain native text and selection behavior, but their native scrollbar regions are clipped out and themed sibling controls provide wheel, keyboard and thumb-drag interaction. This follows Voltura Markdown's approach and prevents native scrollbar colors appearing transiently during scrolling.

Status and progress messages occupy a fixed footer outside the scrolling viewport. The footer paints its background and text in one buffered operation and suppresses native erasure. Scrolling invalidates only the content viewport; wheel input at either boundary does not relayout or repaint unchanged content.

## Archive contract

The format is a plain stored ZIP, limited to 100 MiB overall and 34 entries: `manifest.json`, `preview.png`, and up to 32 media assets. Compression, encryption, extra fields, archive comments, duplicate entries, directory traversal, trailing data and inconsistent local/central metadata are rejected. CRCs cover every entry. Media entry names are exactly `media/` followed by a lowercase 64-character SHA-256 identity, checked against streamed content. Referenced assets must exist and unreferenced assets are rejected. Media reads/copies use 64 KiB buffers; the entire archive is never loaded into memory.

The UTF-8 JSON manifest has `format: "voltura-world"`, `version: 2`, `title`, `description`, `attribution`, `pauseBattery`, `audioDevice` and `displays`. Limits are 120 bytes for title, 4,000 for description, 2,000 for attribution, and 64 KiB for the manifest. A preset contains 1–32 unique source display IDs. Each display carries its `id`, descriptive `label`, `assignment` and `media` reference (empty for non-video Worlds).

Assignments encode scene, all eight parameters, audio reaction, quality/frame limit, output preference, Fill/Fit, video position/speed, Random enabled/interval/pool, and body-statistics visibility. Enum values use the application's stable numeric values; scene names use existing stable string identities. Unknown fields, unsupported values and invalid ranges are rejected. The preview is a validated, fully decodable PNG configuration summary, not a live-render screenshot.

Version-1 single-World packages retain their existing reader, import preview and validation rules. New exports always use version 2; older Worlds versions cannot import them. Version 2 bundles existing video bytes without adding codecs or transcoding. Import previews do not write presets or change settings. Apply preset validates mapped media and applies only the reviewed mappings without modifying saved presets. Save and apply additionally saves the complete imported preset (including skipped source entries). Existing names require themed replacement confirmation; replacement keeps a backup until application succeeds, restoring it on failure or cancellation. Unavailable saved audio devices fall back to system output.

## Checks
