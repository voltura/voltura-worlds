# Preview 0.1.3

- Dropdown fields, arrows, borders, open lists, highlights and list scrollbars now use the application palette in both light and dark themes. Native input and accessibility remain in place.
- Adjustment sliders use themed tracks, progress fills, circular thumbs, disabled colors and keyboard-focus borders.
- Advanced settings opens beside the main controls when the current monitor has enough width. Narrow displays retain the stacked, scrollable layout. Expansion and collapse clamp the entire window to the current monitor's work area, even when opened near a screen edge.
- Layout updates use a deferred positioning batch, redraw suppression during resize, and buffered child painting to address expand/collapse flicker.

The native library regression checks all seven dropdowns, theme changes, keyboard selection, list scrolling, slider keyboard input and mouse dragging, reset, and repeated expansion/collapse near monitor edges. Light/dark screenshots are in `artifacts/preview-0.1.3/ui`. Evidence is from a private Windows desktop, not the user's physical display; physical perception of transition smoothness remains for hands-on testing.

The installer is built and signed with the existing local Voltura certificate. Installation, upgrade and removal testing remain with the user; the installer was not executed by the agent.
