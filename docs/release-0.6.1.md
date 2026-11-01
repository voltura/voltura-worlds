# Voltura Worlds 0.6.1

Performance and resource improvements across wallpaper rendering, texture loading and audio reaction:

- Solar System Journey rasterizes each body within conservative screen bounds, including rings, corona and edge coverage. Its visible-body list no longer allocates on every frame. Fully transparent arrival cards skip drawing.
- Video wallpapers no longer allocate unused FP16 scene and bloom intermediates. At 3840 × 2160 this removes about 71 MiB of texture payload per display; actual allocation savings depend on GPU alignment.
- Artwork texture uploads use at most 4 MiB of staging memory and release decoded mip storage as copying proceeds. Compressed textures batch small mip copies into fewer GPU submissions while retaining the same 4 MiB staging cap, textures and mip levels.
- Audio analysis reuses its Hann window, bit-reversal permutation and FFT weights, and scans frequency bands once. Channel magnitudes still combine after analysis to preserve opposite-phase audio and surround-channel behavior.
- Audio-setting polling drops from 50 to 10 times per second. Renderers avoid audio mutex reads when audio reaction is off, reuse their performance-input list, and poll paused adapters half as frequently. Timer fallback now sleeps until the requested deadline.
- Hidden library controls skip periodic refreshes and refresh when reopened.

Settings, presets, imported videos, HDR output and automatic update verification are retained. No automated tests were added or run. Performance gains are inferred from removed work and allocation sizes; no measured FPS or physical-HDR improvement is claimed.

The app, shell components and installer are signed and timestamped using Voltura AB's existing self-signed certificate.
