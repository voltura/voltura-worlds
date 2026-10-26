# Bloom visualization

Bloom is an original procedural folded-sheet World inspired by the blue Windows 11 wallpaper. No Microsoft wallpaper pixels or external image assets are bundled.

The native D3D12 renderer builds a continuous rolled and pleated sheet in the vertex shader, with smooth surface normals, two-sided satin shading, depth occlusion, and 2x MSAA where supported. Movement controls slow breathing and rocking; Flow changes its pace. Palette, brightness, detail, turbulence, and optional local audio reaction use the existing controls. Audio remains off by default. Bloom is available in the library and tray menu and persists as `bloom` in settings and World packages.

Density changes the single sheet’s curl spacing from 7.2 to 10.2 radians; Density 1 uses an 8.7-radian form. There are no instanced copies.

`bloom_render_contracts` checks distinct Density 0/1/2 output, continuing animation and identical pixels over time at Movement 0. Run `python scripts/test-bloom.py build/Release/VolturaWorlds.exe` for the native capture checks.
