# Aurora authored layers, candidate v1

Generated for Voltura Worlds on 2026-10-23 with the built-in image-generation tool. These are new transparent texture assets, not screenshots or exports of the approved concept image. Original alpha is preserved.

- `teal-silk-v1.png`: luminous translucent emerald/teal/cyan silk membrane, wispy filaments, sparse warm caustics, quiet upper-left, rising folds on the right. Source generated file: `exec-038b8b09-0158-4fd1-be6c-186f93ecec5c.png`.
- `violet-silk-v1.png`: sparse amethyst/violet translucent ribbon wisps with fine champagne highlights, sweeping from lower-left towards right. Source generated file: `exec-754091d5-b761-4898-84ea-0b3b2dcd20b8.png`.

Both requests specified a wide transparent VFX layer, fine irregular detail, no text, no desktop UI, no background, no literal objects, and natural alpha edges. Full generation prompts are recorded in the implementation chat. Runtime rendering independently deforms and composites the layers, animates illumination, and applies shader-based palette, brightness, detail, movement, and optional audio modulation.

This changes Aurora's implementation from exclusively procedural bands to authored 2D artwork animated on the GPU. It is a candidate for visual review. Do not describe the bitmap detail as runtime-generated geometry or fluid simulation. Orbit and Ink retain their separate geometry and compute-fluid implementations.

## Reference-derived asset, revision 5

`approved-flow-v2.png` was produced with the built-in image-generation editor on 2026-10-23, using `docs/concepts/aurora.png` as its edit target. Source: `exec-8a8f6bd4-deb6-45a2-9f87-c0d6fa041bd3.png`. Actual dimensions: 1672 x 941; the request for 3840 x 2160 was not fulfilled by the tool. Do not describe this as native 4K artwork.

Full prompt:

> Use case: precise-object-edit. Edit target: attached approved Aurora artwork. Production asset for an animated wallpaper. Preserve the exact artistic composition, luminous fine flowing folds, translucent spectral teal/emerald and deep violet-blue light ribbons, sparse delicate golden threads, near-black negative space on the left, and crisp tiny structure. ONLY remove all desktop interface elements: the three icons and their labels at upper left, the entire bottom taskbar including every icon and text, and the Aurora Concept text upper right. Seamlessly reconstruct artwork beneath these UI elements. Extend the artwork through the bottom edge. No new text or icons. Do not reinterpret this as opaque silk, cloth, marble, plastic, smoke blobs or milky sheets. Preserve rich saturated dark luminous translucency and detailed hairline fibers from the input. Deliver clean 16:9 wallpaper artwork at 3840x2160 if supported, highest available native detail; no border, no UI, no watermark.

The runtime uses this clean authored image as the source of fine detail. Native HLSL supplies continuous bounded coordinate deformation with regional phase variation, travelling illumination, palette and audio modulation, and nine-tap reconstruction. This is animated artwork, not a fluid simulation or procedurally regenerated folds. The old silk assets are retained as rejected experiments and are no longer loaded by Aurora.

SHA256: `0A2AEA0E0DB6228B27AB80113775D03D8C3C651CDF04C11B554DF3A156D43D7D`.

## Starless artwork, revision 7

`starless-flow-v3.png` is a built-in image-generation edit of `approved-flow-v2.png` made on 2026-10-23. Source generated file: `exec-d274812f-91db-4e7f-8067-14b11b8619ba.png`. The original is retained. The purpose is to separate distant celestial points from the moving auroral ribbons. The native shader now generates a fixed star field independently; time, movement, palette and audio do not enter its coordinate or brightness calculations. Global user brightness still applies to the complete scene.

Full editing prompt:

> Use case: precise-object-edit. Edit target: attached Aurora runtime artwork. Preserve composition, exact ribbon curves, flowing hairline filaments, rich teal emerald blue violet colors, black left negative space, bright gold continuous threads, and original fine texture. ONLY remove ALL discrete stars, gold specks, dots, isolated particles, pinprick lights, sparkles and dust motes EVERYWHERE including on top of the ribbons and within dark areas. Inpaint each dot using the immediately surrounding ribbon or dark sky. The resulting image must contain ONLY continuous flowing auroral light sheets and continuous curved filaments on a clean dark background; absolutely zero individual luminous points or scattered dust. Do not blur or smooth the fine continuous lines. Preserve the source overall brightness and palette. No text, icons, UI, taskbar, new objects or borders. This is a starless motion layer: stars will be rendered separately in fixed coordinates by the application. Keep original 16:9 framing.
`starless-flow-v3.png` SHA256: `067AC306B7E3D1CE68BBF8B0BEBA940F5CA03A859C22F9D882DDA2948E3D8CC4`.

## Irregular density and illumination, revision 8

`organic-flow-v4.png` is a built-in image-generation edit of `starless-flow-v3.png`, generated on 2026-10-23. Source: `exec-9b3997b3-1d6e-41ac-b1f6-62dadc84555e.png`. Earlier assets remain preserved. This is the active runtime texture.

Full editing prompt:

> Use case: precise-object-edit. Edit this starless Aurora artwork. Preserve its exact overall composition, dark left negative space, vibrant translucent emerald teal cyan blue violet palette, broad flowing silhouettes, and fine continuous gold curves. Correct ONE defect: the gas currently contains repeating evenly spaced alternating light/dark ribs, like corrugated fabric or regular ripples. Remove that repeated ribbing throughout, especially lower-left/central green and blue ribbons. Replace with irregular natural translucent wisps: broad quiet transparent areas interspersed with asymmetric clusters of fine filaments, varied spacing and thickness at several scales, a few localized soft luminous folds instead of repeating stripes. Make gradients continuous and organic. Do NOT blur the whole image, turn it milky/opaque, add a regular noise texture, remove gold curves, or introduce new large shapes. Keep rich dark saturated translucency. Absolutely no stars, dots, dust, particles, text or UI: stars are rendered separately. Preserve 16:9 framing.

The runtime also removes sinusoidal brightness sweeps, anisotropic stripe noise, and periodic gold glints. Restrained unequal-scale noise modulates existing illuminated folds in material coordinates instead. Smooth geometric flow and the independent fixed sky are retained.


## Revision 86: current authored candidate

Earlier runtime statements above are historical. `approved-clean-v1.png` is the current runtime candidate, edited from `docs/concepts/aurora.png` using the built-in image generation tool. Original generated file: exec-624eb7fc-1d92-4510-bf98-174164fd4b8f.png. Actual dimensions 1672 x 941, not requested 4K. Native HLSL adds bounded smooth deformation, independent stationary stars, palette/audio controls and inverse display mapping to preserve authored contrast. This is animated artwork, not simulated gas; visual and motion acceptance remain open.

Full prompt:

Edit the supplied approved Aurora concept into a clean production wallpaper artwork layer. Preserve the exact composition, sweeping naturally curved translucent cyan/teal/violet auroral gas, fine golden flowing filaments, saturated luminous colors, dark navy-black negative space on the left, and extraordinary crisp intricate detail. Remove EVERY desktop icon, icon label, taskbar, UI, text and Aurora Concept label, reconstructing continuous artwork behind them. Remove ALL stars and isolated bright point particles throughout, including gold dust speckles: the renderer adds a separate stationary sky. Preserve continuous fine golden filaments, not isolated speckles. No fabric weave, regular ripples, repeating bands, straight angular bends or milky haze. Artwork fills entire frame including former taskbar. Deliver highest possible resolution landscape 16:9 image ideally 3840x2160. This is a clean texture layer, no desktop mockup. Closely preserve approved artwork rather than redesigning.
