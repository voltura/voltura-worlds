# Preview 0.1.2

- Installer: Worlds-specific sidebar artwork, application/setup icons, DPI-aware wizard, clearer welcome and startup-option pages. Uses Markdown's high-resolution bitmap scaling helper.
- Library: follows Windows light/dark application theme; icon and DPI-scaled controls; compact initial view with expandable Advanced settings and vertical scrolling on smaller displays.
- Notification area: bounded, per-executable icon-promotion request modeled on Air; custom themed menu. Explorer retains control of final icon placement.
- Dropdowns: only selection notifications update settings; open lists are not reset by control synchronization. Native mouse-message regression exercises all seven dropdowns and verifies model changes.
- Reset settings: restores selected display's artwork parameters, audio reaction and video fit, plus global quality, FPS, audio output and battery preferences. Preserves scene/video assignments and imported files.

Validation evidence: native library windows captured on a private Windows desktop in `artifacts/preview-0.1.2/ui`, including light/dark and expanded/collapsed layouts. These are not the user's physical display. Core, import UI, desktop host and library UI checks pass. Installer execution, upgrades, removal, and actual notification-area placement remain for the user's testing. No installer was executed by the agent.
