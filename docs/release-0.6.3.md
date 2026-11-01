# Voltura Worlds 0.6.3

A verified automatic update now produces a Windows notification instead of relying only on the tray menu. The notification names the ready version and says to click to restart and install it. Clicking uses the existing restart/update flow, including the helper's signature, release-asset digest and installer identity verification.

Dismissing the notification does not install the update. Each ready version is announced once per app session. Notification requests defer while Windows reports quiet time, a presentation or user absence. Disabling automatic updates dismisses the current prompt and prevents its click action. A stale prompt cannot apply an invalidated update; the tray's **Restart to update** command remains available.

Older installations need one update through the existing tray menu before this notification behavior becomes available. Publishing this release does not install it locally. Settings and existing staged-update verification remain intact.

Validation uses code review and a Release build; no automated tests were added or run. Actual notification appearance and click behavior await user assessment. Existing download, signature verification and update-application code remains unchanged.
