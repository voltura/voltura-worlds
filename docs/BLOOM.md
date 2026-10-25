# Bloom visualization

Bloom is an original procedural folded-sheet World inspired by the blue Windows 11 desktop graphic supplied in chat. No Microsoft wallpaper pixels or external image assets are bundled.

The native D3D12 renderer builds a continuous rolled and pleated sheet in the vertex shader, with smooth surface normals, two-sided satin shading, depth occlusion, and 2x MSAA where supported. Movement controls slow breathing and rocking; Flow changes its pace. Palette, brightness, detail, turbulence, and optional local audio reaction use the existing controls. Audio remains off by default. Bloom is available in the library and tray menu and persists as `bloom` in settings and World packages.

Native evidence: `artifacts/bloom/delivery/bloom.png`, `bloom-motion.mp4`, `metrics.json`, and `motion-analysis.json`. The clip contains 180 original 1080p native frames at 30 fps (six seconds), with no synthetic frames. On the Intel Arc adapter, the 120-frame offscreen warmup measured GPU p95 2.45 ms, approximately 97.5 MiB estimated GPU allocation and 272.5 MiB process private memory. These are offscreen measurements, not physical desktop presentation or endurance results.

Validation: Release application and library-test targets build; core settings/package contracts, capture-time contracts, and native library UI contracts pass. Bloom native capture and 18 existing-scene switch/resize cycles followed by a return to Bloom pass D3D12 debug validation. The initial full suite was interrupted by concurrent Ink-to-Smoke source changes (15/27 passed); that run is not a full-suite success. The final targeted tests above were rerun against the updated source.

This is a first visual implementation for user review, not an exact reproduction of the Windows artwork. The existing installer has not been repackaged. Multi-monitor, physical desktop, and audio-listening acceptance remain unverified for Bloom.

## Restored original baseline

The stacked-sheet experiments were rejected. Bloom again uses the original single continuous rolled sheet, original framing, lighting, and animation. Density now gently changes its curl spacing from 7.2 to 10.2 radians; Density 1 preserves the original 8.7-radian form. There are no instanced copies. Current native capture: `artifacts/bloom/restored/bloom.png`. The native `bloom_render_contracts` check verifies distinct Density 0/1/2 output, continuing animation, and identical pixels over time at Movement 0. Release build and all six debug-enabled native captures pass.
