# Voltura Worlds 0.6.1

Performance and resource improvements across wallpaper rendering, texture loading and audio reaction:

- Renderers create graphics and compute pipelines only for Worlds that are actually used, sharing them across displays on the same adapter. Adapter acquisition and root-signature setup reuse existing objects.
- Shared bloom blur pairs adjacent Gaussian taps, reducing seventeen texture samples to nine at aligned quarter-size resolutions. Jupiter, Uranus and Moon reject empty pixels before expensive shading.
- Solar System Journey rasterizes each body within conservative screen bounds, including rings, corona and edge coverage. Its visible-body list no longer allocates on every frame. Fully transparent arrival cards skip drawing.
- Video wallpapers no longer allocate unused FP16 scene and bloom intermediates. At 3840 × 2160 this removes about 71 MiB of texture payload per display; actual allocation savings depend on GPU alignment.
- Artwork texture uploads use at most 4 MiB of staging memory and release decoded mip storage as copying proceeds. Compressed textures batch small mip copies into fewer GPU submissions while retaining the same 4 MiB staging cap, textures and mip levels.
- Audio analysis reuses its Hann window, bit-reversal permutation and FFT weights, and scans frequency bands once. Channel magnitudes still combine after analysis to preserve opposite-phase audio and surround-channel behavior.
- Audio-setting polling drops from 50 to 10 times per second. Renderers avoid audio mutex reads when audio reaction is off, reuse their performance-input list, and poll paused adapters half as frequently. Timer fallback now sleeps until the requested deadline.
- Hidden library controls skip periodic refreshes and refresh when reopened.
- Settings saving and the desktop-menu server wait for events instead of idle polling. Desktop coverage enumerates windows once for all displays and reuses region objects. Monitor enumeration shares one display-topology snapshot.
- Render workers retain assignment and monitor string storage between frames. Non-Journey frames avoid an extra assignment copy.
- Exactly silent audio channels skip FFT analysis, while quiet and opposite-phase audio retain their existing behavior.
- Package and preset CRC-32 processing uses a shared lookup table with the same polynomial and initial/final XOR. Legacy package exports borrow media buffers and reserve the final ZIP once rather than duplicating full video payloads.

Settings, presets, imported videos, HDR output and automatic update verification are retained. No automated tests were added or run. Matched native profiles on Intel Arc graphics measured 28% lower median GPU frame time for Jupiter (1080p), 21% lower process private memory for a 4K video workload and 51% lower estimated renderer GPU allocation for that video workload. These are workload-specific diagnostic results, not a 20% guarantee across every World or a physical-display/FPS measurement. See [measurement details](performance-0.6.1.md).

The app, shell components and installer are signed and timestamped using Voltura AB's existing self-signed certificate.
