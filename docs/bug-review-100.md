# Production code review: target 100 bugs

Requested October 26, 2026. No automated tests are added or run. Count distinct
defects, grouping repeated instances of the same cause. This is an ongoing audit;
the target is not evidence that 100 defects exist or have been fixed.

User time limit: stop at 100 confirmed fixes or October 26, 2026 at 21:10
Europe/Stockholm (20:10 UTC), whichever comes first. This cutoff was requested at
20:10 local time. Check the clock before further batches and during long work;
do not continue the hunt past the cutoff or mark 100 fixes achieved if fewer were
confirmed. The user explicitly authorized ending unfinished work at that time.

## Fixed — 81 defects

| ID | Defect and concrete trigger | Fix |
| --- | --- | --- |
| 001 | `loadSettings` narrows JSON numbers before checking ranges: fractional enums/palettes and large integers can become valid, different values. | Check integer type and JSON range before converting. |
| 002 | `loadSettings` accepts an object in `displays`, iterating its values as monitor assignments despite the array format. | Require an array. |
| 003 | Duplicate monitor IDs load successfully; lookup and editing affect the first assignment while duplicate stale entries remain saved. | Reject empty, embedded-null, and duplicate IDs on load/save. |
| 004 | `saveSettings` can replace a readable file with one exceeding the loader's 1 MiB/32-display limits. | Enforce the same limits before replacing the file. |
| 005 | Version-1 World metadata accepts embedded nulls; the import UI truncates titles, descriptions, or credits at the null. | Reject embedded nulls before presenting/importing metadata. |
| 006 | World/package writers check `write` but not final `close`; a late buffered write failure can replace the destination or install incomplete media. | Check close results in both package exporters and legacy video installation. |
| 007 | Version-1 package exports use a shared `<destination>.pending` filename, allowing concurrent exports to truncate or rename each other's output. | Give each operation a unique staging filename and clean it up on failure. |
| 008 | Video speed, center, and position handlers access `selectedSurface()` after desktop surfaces disappear, producing out-of-range errors instead of safely ignoring unavailable controls. | Match the existing empty-surface guard used by other adjustment controls. |
| 009 | Apply to all replaces assignments but leaves older pending video speeds and status on destination surfaces, allowing the render worker to override the newly applied speed. | Replace destination requests with the source display's current pending speed (or clear them when none exists), and clear obsolete messages. Preserve the source request until render-worker validation. |
| 010 | A cancelled Windows shutdown leaves `sessionEnding` true and a permanently cancelled update state, preventing further automatic checks. | Restore session state and discard the cancelled updater when `WM_ENDSESSION` reports cancellation. |
| 011 | Audio device enumeration dereferences a null endpoint if `IMMDeviceCollection::Item` fails during a device change. | Check the result before accessing the endpoint; also guard null friendly names. |
| 012 | Audio analysis retains the previous ring buffer across capture discontinuities and idle periods, mixing old music into resumed audio. | Reset analysis history on discontinuity or first packet after idle. |
| 013 | Audio sample decoding silently yields zero for 8-bit PCM and 64-bit IEEE float mix formats. | Decode both formats and keep float/integer branches separate. |
| 014 | Audio capture ignores `ReleaseBuffer` failure and overwrites it with `GetNextPacketSize`, losing the error that should restart a broken capture stream. | Preserve release failures and enter the existing recovery path. |
| 015 | GPU enumeration handles only `DXGI_ERROR_NOT_FOUND`; another failure repeatedly appends invalid entries and continues allocating instead of recovering. | Stop and report failed adapter enumeration. |
| 016 | Renderer setup owns native latency/event handles before many operations that can throw; C++ does not run the renderer destructor after constructor failure, leaking handles on each retry. | A local guard closes handles on partial construction failure. |
| 017 | Fence completion value `UINT64_MAX` signals GPU removal, but renderer waits and video import treat it as completed work and may reuse/read invalid GPU resources. | Reject device removal before proceeding after a fence wait/readback. |
| 018 | Shutdown during an in-progress settings write can discard a newer pending snapshot: the save worker exits immediately after writing the older snapshot. | Drain pending settings before exiting the worker. |
| 019 | EBML container identification reads element IDs and lengths against the entire file prefix rather than the declared EBML header boundary. An element header can cross that boundary, underflow `end-pos`, and permit an out-of-bounds DocType string read. | Bound each variable-length integer to the declared header end and reject invalid reads before subtracting offsets. |
| 020 | H.264 colour detection returns the first nonzero transfer characteristic. A stream advertising an SDR SPS followed by a PQ/HLG SPS bypasses the app's HDR rejection. | Inspect all advertised sequence parameter sets and give PQ/HLG precedence. |
| 021 | Cancelling a legacy/personal-video import after installation has produced its managed directory but before the completion message applies it leaves unused videos or source packages in the library. | Track the new directory, join the cancelled worker, and remove that operation's unused direct library child. Retain files once the apply callback begins because it may have changed settings before reporting an error. |
| 022 | The preset digest helper destroys BCrypt handles before allocating its result string; an allocation exception then enters cleanup and destroys the same handles again. | Own the algorithm/hash with one noncopyable RAII object, keeping cleanup valid across all exits. |
| 023 | A video changed between export's SHA-256 naming pass and CRC pass can be written with a valid CRC but an obsolete content-addressed name, producing a saved preset that its own loader rejects. | Compute SHA-256 from the actual exported bytes while streaming and reject identity changes before committing the archive. |
| 024 | The audio packet-draining loop does not check its stop token. A continuously replenished capture backlog can prevent thread shutdown and hang device changes or app exit while joining that thread. | Check cancellation between packets, while still releasing each acquired packet. |
| 025 | Several catch handlers decode `std::exception::what()` as strict UTF-8 even though Windows/system error text has no such guarantee. An encoding error while reporting a filesystem failure escapes the handler and can terminate a worker or window callback. | Use a shared error-text conversion with a safe fallback for undecodable messages in main/import/preset error paths. |
| 026 | Owner-drawn preset/device names and exported summary labels interpret literal ampersands as mnemonic prefixes, so a name such as `A&B` is displayed as `AB`. | Disable mnemonic-prefix processing for list/combo content and summary labels, retaining button/menu mnemonic behavior. |
| 027 | The audio output list is populated only when the library is created, so devices connected later cannot be selected, while a disconnected saved device is falsely displayed as system output. | Refresh choices when opening the dropdown, preserve selection by endpoint ID, and label an unavailable saved device explicitly. |
| 028 | Random uses the full `GetTickCount64` interval since the preceding timer tick. Resume/unlock clears the pause state before the next tick, allowing sleeping time to advance the active-playback countdown and immediately change Worlds. | Reset the timer baseline when resuming, unlocking, or turning the display back on. |
| 029 | Holding a scrollbar page area continues paging after its thumb reaches the pointer, scrolling beyond the intended position toward the end. | Stop page-repeat when the thumb reaches or passes the pointer; retain ordinary arrow repeat and thumb dragging. |
| 030 | While replacement confirmation pumps messages, a second import can replace `pending` and refresh `presets`; the original confirmation then resumes using an invalidated preset reference or applies a different import. | Guard confirmations against new import requests and preset commands until the current decision finishes. |
| 031 | Saved-preset discovery and update validation compare `.world` case-sensitively, hiding valid `.WORLD` files on Windows and preventing their updates. | Match package extensions case-insensitively. |
| 032 | Presets updates fonts on DPI change but retains the old fixed list/combo row heights, clipping enlarged text and leaving dropdown rows at the previous scale. | Update list rows and both combo selection/dropdown heights with the new DPI. |
| 033 | The preset apply callback can throw while copying assignment data or synchronously refreshing controls after committing settings. The importer interprets that as apply failure and removes installed videos which the active settings now reference. | Avoid allocating assignment copies after commit and post the UI refresh separately, keeping rollback-triggering failures before the settings commit. |
| 034 | A settings load failure resets the app to defaults while retaining the failed file as the next save destination. A later edit overwrites the only copy of the user's old configuration. | Move the unreadable file to a unique `settings-recovery-*.json` in the profile before continuing with defaults. Stop startup if preservation fails. |
| 035 | A transient settings write/rename failure clears the dirty flag permanently, so the last edits are never saved unless another edit happens. | Requeue the latest settings while the app is running and retry with an interruptible one-second backoff; do not retry indefinitely during shutdown. |
| 036 | Preset summary drawing creates its DC, bitmap, and font before entering its cleanup `try` block. A string allocation/conversion failure while preparing text leaks all three handles, worsening resource exhaustion on retries. | Own drawing handles with a local RAII guard from their first allocation through text preparation and PNG encoding. |
| 037 | Once saving fails, its error remains displayed after a later successful save; ordinary rendering does not clear it, so the app continues to claim settings are unsaved. | Track the save worker's error and clear it on successful recovery. Finding 053 later gives that error a dedicated status field. |
| 038 | Cancelled import cleanup runs on detached reaper threads. Exiting Worlds immediately afterward can terminate them before they remove staged videos/temporary validation files, leaving incomplete imports behind. | Track reapers, retire completed ones, and close/join all remaining import work during application shutdown. |
| 039 | A second launch with both `--wallpaper` and `--import` silently drops the import because existing-instance forwarding excludes every wallpaper launch. A first launch with the same flags imports correctly. | Forward explicit import requests regardless of the background-launch flag. |
| 040 | Startup does not check command-line allocation or controller-window creation failure. It can perform pointer arithmetic on null launch arguments or continue without the controller needed to receive Exit and timer messages. | Fail startup explicitly when either required startup resource cannot be created. |
| 041 | If background thread creation fails, Presets remains busy with no completion message, import startup can throw through its window callback, and rendering startup can remain marked healthy with automatic retry disabled. | Restore Presets controls before propagating the error, report import startup failure through its normal completion path, and mark failed rendering startup for the existing retry path. |
| 042 | Coverage detection subtracts a covering window's entire frame rectangle even when its explicit window region has holes, pausing wallpaper that remains visible. | Intersect each covering rectangle with its window region translated into screen coordinates before subtracting it. |
| 043 | Setup accepts 32-bit Windows and installs the x64 application, which cannot launch there. | Reject unsupported 32-bit operating systems before installing files or registry entries. |
| 044 | Uninstall removes the entire `.world` extension key when Worlds owns its default value, deleting other applications' Open With entries and extension metadata. | Remove only Worlds' default association and delete the extension key only when empty. |
| 045 | Only setup takes the installation mutex; uninstall can run concurrently with setup or another uninstaller and remove files or registrations while they are being installed. | Acquire the same process-lifetime mutex in uninstaller initialization and abort when another operation holds it. |
| 046 | Loopback audio averages signed channel samples before the FFT/RMS calculation. Equal opposite stereo signals cancel, making audible audio produce zero reaction; quiet surround channels also dilute the signal. | Keep separate channel histories and combine each analyzed feature by its strongest channel, preserving phase-independent reactions. |
| 047 | Update cache reuse checks release version but ignores the current GitHub asset digest. Replacing an asset under the same release version leaves the previously cached installer selected. | Reuse a cached candidate only when its digest also equals the current release-asset metadata; otherwise download and verify the replacement. |
| 048 | Parameter slider refresh truncates a floating-point percentage. Representational error can put a selected integer step just below itself, moving the thumb down one step on the next refresh despite unchanged settings. | Round parameter percentages to the nearest slider step, matching video-position controls. |
| 049 | Library and Presets wheel handlers discard fractional pixels on each message. Repeated small high-resolution wheel deltas can produce no scrolling at all. | Carry each window's fractional wheel remainder into the next message. |
| 050 | Display discovery abandons `QueryDisplayConfig` when topology growth invalidates its earlier buffer sizes. Monitor enumeration can then lose adapter/refresh information, and HDR discovery treats the same transient race as absent outputs. | Share a bounded query helper that obtains fresh sizes and retries `ERROR_INSUFFICIENT_BUFFER` in both callers. |
| 051 | Editable multiline preset metadata fields omit `ES_WANTRETURN`. Under the app's `IsDialogMessage` navigation, Enter can trigger the default Save action instead of inserting a line break. | Request Return input for the editable Description and Attribution fields. |
| 052 | Settings stream extraction accepts a valid JSON object followed by garbage or another document, silently accepting only the first configuration and discarding trailing content on a later save. | Strictly parse the complete settings bytes so trailing content reaches the existing preservation/recovery path. |
| 053 | Rendering recovery clears an unresolved save failure in the shared status string. With multiple failing displays, recovery can instead leave the recovered display's stale error showing while another display still fails. | Give settings failures independent status ownership and retain per-display rendering errors, selecting an error from a display that is still failing. |
| 054 | Saved-preset rollback ignores failures and forgets a new saved file when deletion fails. A failed backup restore also leaves unrelated actions enabled, allowing subsequent edits to conflict with the retained rollback. | Retain pending rollback paths on failure, report their recovery locations, block further preset operations, and retry cleanup when closing. Cancellation completion reports failed rollback instead of silently closing. |
| 055 | Settings accept Random with Video/Off, which the startup cycle immediately replaces. Settings and desktop presets also accept a non-Solar-System scene with Solar System Random; applying such a preset keeps that out-of-pool scene until the interval elapses. | Validate that an enabled Random assignment's scene belongs to its selected pool on settings load/save and preset read/write. |
| 056 | C++ exceptions during audio capture escape the worker thread and terminate the application; the manual cleanup path is bypassed. Endpoint string/property allocations also leak their COM-owned buffers if a string/vector operation throws. | Guard capture resources, COM apartments and endpoint buffers with scoped cleanup; catch capture-attempt exceptions and retry with the existing backoff. Pass the captured device string by reference into the worker body. |
| 057 | Import staging ignores `create_directory` returning false for an existing directory. On a staging-name collision, imports can overwrite files in a directory they did not create and failure cleanup can remove that directory's contents. | Require successful creation before entering the write/cleanup region in legacy World, desktop preset media, and personal-video installation. |
| 058 | Recreating wallpaper surfaces resets the library's monitor selection to index zero. Explorer/graphics recovery can silently redirect subsequent edits from the user's chosen display to the first display. | Retain the selected monitor's identity across rebuilds and failed attempts, restore its new index when present, and fall back when it is no longer available. |
| 059 | Later reads of a mutable preset archive trust earlier validation: metadata is re-read without checking the returned bytes against its ZIP CRC, and repacking loaded media accepts bytes that no longer match its original content-addressed identity. Stable replacement bytes can therefore be silently parsed or repacked. | Check metadata CRC on the bytes returned to the parser and require loaded media to retain its expected SHA-256 identity before repacking. Fresh desktop snapshots still receive new content identities normally. |
| 060 | A child-control/setup failure leaves an already-created Presets window registered as active and partially initialized; legacy imports silently continue when child creation fails. Later opening can expose an unusable window or missing action/status controls. | Check required import/status controls and destroy the partial owner window if post-creation setup throws, allowing its normal state cleanup and a fresh opening attempt. |
| 061 | Field scrollbar binding ignores failed property/subclass registration, then clips the native scrollbar anyway. Without the destruction hook, the sibling scrollbar and binding can outlive the field, leaving a callback to a dead or reused window handle. | Install all links before transferring binding ownership or clipping the field; on failure remove partial registration, destroy the sibling, and free the binding. |
| 062 | Preset Save/Delete/Export/Apply can commit successfully, then report cancellation or failure when their list refresh fails. The UI retains stale entries, allowing later actions to use obsolete metadata or archive offsets. | Preserve the committed outcome separately, treat refresh cancellation/failure as a list-refresh problem, and clear stale entries with instructions to reload. Preserve Apply success even if its refresh worker cannot start. |
| 063 | Preset/list and combo population ignores failed string insertion and continues adding later entries. A missing item shifts visible indices away from their backing records, so selections can address the wrong preset, display, device, or enum value. | Check every insertion and stop before indices diverge. Clear failed preset lists/mappings, reject missing selections, retry incomplete audio lists on reopening, and unwind partial library setup. Prevent nested timer errors from stacking dialogs while a failure is being reported. |
| 064 | Library layout mode is retained after DPI, resolution, and work-area changes. A side-by-side Adjustments window can remain too wide for the new monitor and leave controls off-screen. DPI changes also retain pixel-based scroll offsets rather than the same logical position. | Recompute/clamp library layout after those changes and rescale library/Presets scroll offsets with DPI. |
| 065 | MOV detection requires `ftyp` at a fixed header offset even though QuickTime permits files without that atom. Such files are rejected before native codec validation; extended-size `ftyp` headers also read the wrong major-brand offset. | Recognize legacy QuickTime top-level atom candidates and locate the brand after the actual atom header, retaining native metadata/codec/HDR validation before installation. |
| 066 | The Matroska/WebM colour scanner interprets short unknown-size Segment encodings as literal byte counts (for example, 127), stopping before later colour/codec metadata and potentially missing an HDR transfer declaration. | Distinguish the all-one unknown-size marker from literal lengths and parse it to the parent bound at every supported VINT width; reject known sizes that exceed that bound. |
| 067 | Thumbnail sizing rounds an extremely narrow video's width up to one pixel, then computes an unbounded height from its aspect ratio; valid tall/non-square-pixel videos can request huge image allocations. | Clamp both output dimensions to 540 by 280 before converting to integer sizes and allocating the WIC scaler. |
| 068 | Runtime artwork, licenses, and D3D12 dependency copies run only on executable relink; a normal incremental build after an asset-only change keeps stale runtime files. | Make runtime synchronization an explicit regular build dependency, independent of linking, alongside the existing shader synchronization. |
| 069 | Bootstrap downloads JSON directly into its permanent path, so an interrupted download leaves a corrupt file that every future invocation refuses to repair; SDK setup also treats one extracted header as proof extraction finished. | Download into a unique temporary path, verify the pinned digest before promotion, retry corrupt JSON, and require the SDK headers and both runtime DLLs before skipping extraction. |
| 070 | Legacy/personal imports capture a display before asynchronous copying and validation, then assign to it even if disconnected or moved to another GPU; the dialog closes as if the import reached an active destination. | Re-enumerate and require the destination ID and preflighted adapter to still match before changing settings. |
| 071 | Tray icon registration ignores failure and is attempted only once per Explorer startup; a transient rejection can leave a background instance without its tray controls indefinitely. | Track successful registration, tolerate an existing icon through modification, and retry missing registration on the existing periodic timer. |
| 072 | Disconnected-display assignments accumulate indefinitely; after 32 remembered display IDs, selecting or importing onto a new display exceeds the save limit and all subsequent saves fail. | On topology rebuild, reserve assignment space for connected displays and remove the oldest disconnected entries only when the 32-entry cache needs room. Connected-display preferences are retained. |
| 073 | Monitor enumeration allocates strings/vectors inside its Windows callback without an exception boundary and ignores enumeration failure, risking an exception through the Win32 callback boundary or use of a partial display list. | Capture callback exceptions, stop enumeration, and rethrow from the ordinary C++ caller; reject unsuccessful native enumeration before publishing monitors. |
| 074 | The power-status query's return value is ignored; failure leaves a zero-initialized AC status that is interpreted as battery power, incorrectly pausing Worlds on mains power. | Update battery pause state only when Windows successfully reports the power status; retain the previous state on query failure. |
| 075 | Desktop attachment checks reparenting and positioning but ignores failed conversion to the required child/no-activate window styles and failed screen-to-parent coordinate mapping; it can report an improperly attached surface as healthy. | Check both style changes and coordinate conversion, preserving the distinction between a valid zero return and a Windows error, so existing attachment recovery handles failure. |
| 076 | Rejected video-speed changes ignore rollback failures and always report that the prior speed was retained; an unexpected resulting rate can be accepted into settings despite failed recovery. | Require both rollback calls and the observed restored rate to succeed before reporting retained playback; otherwise restart through the existing playback-error path. Use a finite, consistent rate-match check. |
| 077 | Rebuilding desktop surfaces creates an unstarted Random cycle, so the first timer tick immediately replaces the current World regardless of the chosen interval. | Initialize recovered Random surfaces with their current scene retained and a fresh full interval. Explicitly selecting Random still starts a new choice immediately. |
| 078 | After a cancelled Windows shutdown, an old updater/helper can keep its signaled named session event alive. A fresh check reuses that signaled event, so its helper immediately cancels and the verified update cannot install normally. | Reset a live check's session event under the same state lock used to publish and signal its handle, after checking cancellation/closing state. |
| 079 | Legacy installation publishes its owned destination before renaming the stage. If promotion fails because the destination already exists, import cleanup can recursively delete that pre-existing directory. | Prepare the destination path beforehand but publish ownership with a non-allocating move only after the stage rename succeeds; failure cleanup owns only the stage. |
| 080 | Legacy import sizes its client area using its hidden controller owner's DPI, while laying out controls at the dialog's own DPI; different monitor scales can leave controls clipped or the dialog oversized. | Size the dialog using its own DPI, matching its fonts, margins, and layout. |
| 081 | Switching from an audio-reactive World to imported video or Off retains the reaction preference and displays “Audio gentle/expressive,” even though reaction controls and capture are disabled for those scenes. | Report Audio off for Video/Off while preserving the saved reaction choice for returning to an animated World. |

Verification: source review completed. All 18 fixes compiled successfully
in Release. Normal app use opened the updated library and Presets window, loaded
an existing preset, and displayed its saved details. No malformed-file fixtures, diagnostic regression modes, test
harnesses, or automated tests are used. Hardware disconnects, low-disk failures,
and cancelled Windows shutdown are source-reviewed, not physically reproduced.

Additional change: validate version-2 palette range before narrowing it, matching
the settings fix in 001. This is not counted as a separate bug.

User feedback during review requested uniform form margins in the library,
expanded Adjustments, and Presets windows. That layout work is tracked separately
from the confirmed bug count and awaits the user's visual assessment.

Review continues through source inspection and compiler feedback without desktop
automation or changing focus. Worlds may be closed programmatically for builds,
as explicitly confirmed by the user. Findings 019–020 are source-reviewed and
compiled in Release. Finding 021 is also source-reviewed and compiled in Release.
No malformed-media fixtures or tests are created.

Findings 022–026: source-reviewed; Release build passed. Export identity
checking hashes the bytes already being streamed, without additional files or
a separate pass. No desktop automation is used.

Findings 027–029: source-reviewed; Release build passed. Audio hot-plug,
suspend/resume, and held mouse input have not been exercised on the desktop.

Findings 030–033: source-reviewed; Release build passed. Re-entrant
imports and cross-monitor DPI changes are not claimed as runtime observations.

Findings 034–035: source-reviewed; Release build passed. Follow-up review
also extended 001's integer checks to legacy World manifests and 018's shutdown
ordering so the save worker stays alive until renderer workers have stopped
producing settings changes. These extensions are not counted as new bugs.

Compiler static analysis (`RunCodeAnalysis=true`, Microsoft native recommended
rules) completed across application/core sources. Warnings were reviewed against
their owning source; conservative nullability/stack-size warnings and SDK/vendor
warnings are not counted as confirmed bugs. No test execution occurred.
Findings 036–037 are source-reviewed; normal Release build passed.

Findings 038–040: source-reviewed; Release build passed. No app windows
were launched or operated to verify these failure paths.

Finding 041: source-reviewed; Release build passed. Worker creation failures
were not exercised at runtime. Follow-up review extended existing 003/005 null
validation to desktop preset identifiers and metadata; these are not new counts.
No tests, desktop automation, or app launches were used for this batch.

Findings 042–045: source-reviewed. Release app build passed. NSIS preprocessing
passed, and the final installer script compiled successfully in an uncompressed
scratch copy (only compression directives changed). The earlier full asset
compression build was deliberately stopped; no installer was executed, and all
scratch installers/sources were removed. Shaped-window coverage, 32-bit rejection,
registry preservation, and concurrent install/uninstall were not exercised at
runtime. No automated tests or desktop control were used.

Finding 046: source-reviewed; Release build passed without new warnings after
correcting a float literal. Review followed sample decoding, per-channel ring
history, discontinuity resets, FFT/RMS analysis, and feature publication. Playback
and additional per-channel CPU cost have not been observed in the running app.
No automated tests, audio playback, app launches, or desktop control were used.

Finding 047: source-reviewed; Release build passed. Review followed current
release metadata, cached candidate selection, fresh download, SHA-256 validation,
and publisher verification. No release assets were changed, no update was applied,
and the replacement-asset scenario was not exercised at runtime. No automated
tests or desktop control were used.

Findings 048–049: source-reviewed; Release build passed. Review covered slider
round-tripping and positive/negative fractional wheel accumulation. Existing
finding 030 was also extended to guard preset Open/Save file dialogs against
re-entrant imports while their nested message loops run; this is not a new count.
Slider behavior, high-resolution wheels, and re-entrant dialogs were not exercised
in the running app. No automated tests, app launches, or desktop control were used.

Finding 050: source-reviewed; Release build passed. Review covered buffer-size
refresh on insufficient capacity, success-path count trimming, other-error
fallbacks, and the four-attempt bound in monitor and HDR discovery. No physical
hot-plug, HDR transition, automated tests, app launches, or desktop control were
used to verify this batch.

Finding 051: source-reviewed; Release build passed. Review followed editable
multiline control styles through the modeless `IsDialogMessage` loop and the
default `IDOK` to Save mapping. Enter/newline behavior was not exercised in the
running app. No automated tests, app launches, or desktop control were used.

Finding 052: source-reviewed against the bundled parser implementation; Release
build passed. The stream extraction path passes strict=false, while the chosen
parse API passes strict=true. Existing finding 004 was extended to bound the
actual read if the file grows after its size check; this is not a new count.
Malformed settings and concurrent file growth were not exercised at runtime.
No automated tests, app launches, or desktop control were used.

Finding 053: source-reviewed; Release build passed. Review followed save failure
and success, desktop recreation, single-display recovery, and remaining failures
on other displays. Error fields remain protected by the existing state mutex.
Concurrent save/render failure scenarios were not exercised in the running app.
No automated tests, app launches, or desktop control were used.

Finding 054: source-reviewed; Release build passed. Review followed backup
restoration, new-file deletion, cancellation, close-after-cancellation, apply
failure, subsequent commands/imports, and destruction-time retry. Pending paths
are read by the UI only after the worker is joined or when it is not busy.
Locked-file rollback failures were not exercised at runtime. No automated tests,
app launches, or desktop control were used.

Finding 057: source-reviewed; Release build passed. All three staging creation
checks precede their cleanup regions, so an existing directory is neither written
nor removed by that attempt. GUID-name collision is a rare trigger and was not
exercised at runtime. Existing finding 009 was refined to propagate the source's
pending video-speed request rather than erase it; older destination requests are
still replaced, and each render worker validates its new request. This is not a
new count. No automated tests, app launches, or desktop control were used.

Finding 058: source-reviewed; Release build passed. Review followed selection
capture before teardown, changed enumeration order, temporary discovery failure,
missing-monitor fallback, and selection restoration. Existing finding 041 was
extended to preserve rendering retry state when coordinator thread creation
fails; this is not a new count. Explorer restart, physical display changes, and
thread-start failures were not exercised at runtime. No automated tests, app
launches, or desktop control were used.

Finding 059: source-reviewed; Release build passed. Review followed version
dispatch, manifest/preview reads, loaded-media export and rename, fresh desktop
snapshots, and existing written-byte hash checks from 023. The new checks preserve
previously validated identities; 023 separately checks changes during export's
own passes. Concurrent archive mutation was not exercised at runtime. No automated
tests, app launches, or desktop control were used.

Finding 055: source-reviewed; Release build passed. Review compared the built-in
random and Solar System catalogs with settings load/save, desktop preset
validation, UI selection, and preset application (`reset(true, pool)`). Invalid
assignment import/startup scenarios were not exercised at runtime. No automated
tests, app launches, or desktop control were used.

Finding 056: source-reviewed; Release build passed. Review followed allocation
failure paths, capture Stop/release order, event/format ownership, COM apartment
balancing, endpoint ID/property ownership, stop checks, and retry backoff. Resource
exhaustion and endpoint recovery were not exercised at runtime. No automated
tests, audio playback, app launches, or desktop control were used.

Finding 060: source-reviewed; Release build passed. Review followed required
child/status creation failures, partial-window destruction, active-window reset,
and normal state destruction. Existing finding 041 was extended to import cleanup
thread creation: ownership remains local until that thread starts, and allocation
or thread-start failure falls back to local joining/cleanup. That rare fallback
can block the UI while the worker returns. These failures were not exercised at
runtime. No automated tests, app launches, or desktop control were used.

Finding 061: source-reviewed; Release build passed. Review followed each failed
binding-registration step, sibling destruction, successful ownership transfer,
native field destruction, and scrollbar creation failure before/after window
messages begin. Scrollbar state now remains locally owned until window creation
succeeds, refining setup cleanup from 060 without another count. Registration
failures were not exercised at runtime. No automated tests, app launches, or
desktop control were used.

Finding 062: source-reviewed; Release build passed. Review followed cancellation
before Delete, cancellation/error after file commit, Apply's UI commit, refresh
worker startup failure, and stale-list invalidation. Existing finding 030 was
extended across the UI-side Apply/cleanup step because COM endpoint enumeration
can pump messages while State references are in use; this is not a new count.
Commit/refresh timing scenarios were not exercised at runtime. No automated
tests, app launches, or desktop control were used.

Finding 063: source-reviewed; final Release build passed without new warnings
after correcting a shadowed local name. Review covered every combo/list insertion
site, index/model alignment, failed mapping/list cleanup, missing-selection guards,
audio-list retry, and prevention of nested error dialogs. Setup rollback from 060
was also extended to the library and dynamic preset mapping; no extra count.
Insertion/resource failures were not exercised at runtime. No automated tests,
app launches, or desktop control were used.

Finding 064: source-reviewed; Release build passed. Review followed DPI changes,
work-area notifications, display changes, side/stacked layout selection, window
clamping, and logical scroll preservation. Cross-monitor appearance and physical
display changes were not observed; visual assessment remains outstanding. No
automated tests, app launches, or desktop control were used.

Finding 065: source-reviewed; Release build passed. Apple's [file type atom
documentation](https://developer.apple.com/documentation/quicktime-file-format/file_type_compatibility_atom)
states that `ftyp` is optional; its [atom layout documentation](https://developer.apple.com/documentation/quicktime-file-format/atoms)
defines extended-size headers and legacy top-level atom structures. Review
followed bounded header reads through native metadata, codec, geometry, and HDR
validation. Actual legacy MOV playback was not observed. No automated tests,
media playback, app launches, or desktop control were used.
Findings 066–067: source-reviewed; Release build passed. Unknown-size EBML
handling follows the all-one marker in [RFC 8794 section 6.2](https://datatracker.ietf.org/doc/html/rfc8794#section-6.2).
Review also followed thumbnail dimensions through integer conversion and WIC
allocation, including very narrow and wide aspect ratios. Actual unknown-size
streams and extreme-aspect video imports were not exercised. No automated tests,
media playback, app launches, or desktop control were used.
Finding 068: source-reviewed; normal incremental Release build passed without
recompiling C++ or relinking the executable. The generated runtime dependency
ran as part of that build. Review covered licenses, artwork, D3D12 dependencies,
and the separate existing shader dependency. No automated tests, app launches,
or desktop control were used.
Finding 069: source-reviewed; the normal Release build passed using the existing
verified dependencies. Download failure and partial extraction were reviewed
without changing or corrupting dependency files. No automated tests, app launches,
or desktop control were used.
Finding 070: source-reviewed; Release build passed. Review followed asynchronous
legacy/personal import through fresh monitor enumeration and the assignment
commit. Physical display removal or GPU reassignment was not exercised. No
automated tests, app launches, media playback, or desktop control were used.
Finding 071: source-reviewed; Release build passed. Registration failure, later
success, duplicate registration, and Explorer restart paths were followed in
source. Explorer was not restarted and tray behavior was not exercised.

Follow-up review also extended existing findings without increasing the count:
015 now stops tray-registry enumeration on errors; 056 guards rendering-worker
resources, COM initialization, and settings snapshot creation; 060 now retains
local ownership until Presets window creation succeeds and destroys a dynamic
control if adding it to the layout fails. These resource-failure paths remain
source-reviewed. No automated tests, app launches, or desktop control were used.
Finding 072: source-reviewed; Release build passed. Review covered a full cache
with a newly connected ID, multiple new IDs, connected-entry retention, and no
need to evict while below capacity. This was source reasoning; no simulated
monitor topology or physical display changes were run. The follow-up changes to
015, 056, and 060 also compiled successfully. No automated tests, app launches,
or desktop control were used.
Finding 073: source-reviewed; Release build passed. Callback allocation failures
now leave the callback normally, and failed enumeration is not published as a
successful topology. Enumeration/resource failures were not injected or observed
at runtime. No automated tests, app launches, or desktop control were used.
Finding 074: source-reviewed; Release build passed. Query success, query failure,
and last-known battery state were followed in source. Power-query failure was
not induced. No automated tests, app launches, or desktop control were used.
Finding 075: source-reviewed; Release build passed. Review covered legitimate
zero returns, native errors, caller cleanup, and existing desktop retry. The
monitor-information failure path in 073 now also rejects partial enumeration,
and topology setup handles those errors through its retry path. Follow-ups to
005 reject embedded nulls in persisted audio/video identifiers; 053 keeps setup
errors separate from per-display recovery; 060 covers surface-registration
failure cleanup. Those changes compiled successfully without increasing the
count. No automated tests, app launches, or desktop control were used.
Finding 076: source-reviewed; Release build passed. Review covered rejected rate
changes, both rollback calls, observed restored rate, non-finite rates, and the
renderer restart path. Windows media rollback failures were not induced. The
073 follow-up also preserves automatic retry after failed display-change
notifications. No automated tests, app launches, playback, or desktop control
were used.
Finding 077: source-reviewed; Release build passed. Review covered recovery of
both Random pools and the separate explicit-Random selection path. The current
World is retained after rebuild; the interval restarts rather than preserving
an exact prior countdown. Physical reconnects were not exercised. No automated
tests, app launches, or desktop control were used.
Finding 078: source-reviewed; Release build passed. Review covered an existing
signaled event, fresh event creation, cancellation/close checks, and ordering
with this state's sessionEnd signal. Windows shutdown and helper installation
were not exercised. No automated tests, update installations, app launches, or
desktop control were used.
Finding 079: source-reviewed; Release build passed. Review followed failed
promotion, pre-existing destinations, successful promotion, and later assignment
allocation failures through import cleanup ownership. No collision or destructive
filesystem scenario was exercised. No automated tests, app launches, or desktop
control were used.
Finding 080: source-reviewed; Release build passed. Review compared the dialog's
own DPI with its controller owner's DPI and followed sizing through layout.
Cross-monitor appearance remains unobserved and needs visual assessment. No
automated tests, app launches, or desktop control were used.

The 079 ownership review also covered preset replacement backups: backup output
is now claimed with CREATE_NEW before copying, and only owned output is removed
on failure. Copying remains bounded, cancellable, and flushed before the backup
is recorded. Existing backup paths are never opened for overwrite or cleanup.
This is the same cleanup-ownership defect, not an additional counted finding.
Finding 081: source-reviewed; Release build passed. Review followed switching
from a reactive scene to Video/Off and back, comparing status text with capture
and control enablement. The saved reaction choice remains available. These
interactions were not exercised in the running app. The final 079 backup-copy
follow-up also compiled successfully. No automated tests, app launches, or
desktop control were used.
Final compiler state: the Release build passed with finding 081 and the backup
ownership follow-up included. The audit contains 81 distinct numbered findings;
related follow-ups remain grouped under their original finding. Changes remain
in the working tree. No release was prepared or published. The requested uniform
30-DIP outer spacing in the main library and Presets is implemented and compiled;
its appearance still awaits the user's visual assessment. Recent verification
used source review and compiler feedback, without automated tests or desktop
control. Hardware, shutdown, failure-injection, and playback scenarios not
observed in the app are identified above rather than claimed as verified.
Stopped October 26, 2026 at 21:10 Europe/Stockholm (20:10 UTC), as requested.
Final count: 81 documented fixes. The 100-fix target was not reached. Work ended
at the user's time limit; no further hunt or release work is scheduled.
