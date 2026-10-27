# Tropical Island

`environment.png` is original AI-generated artwork created with the built-in OpenAI image generation tool for Voltura Worlds. It depicts a fictional tropical beach and island.

The native shader retains the sky, distant island, rocky headland and palm crowns from the artwork. The ocean and beach are rendered in world space by `shaders/tropical_island_ocean.hlsli`: ray intersections with a displaced water surface and sloping seabed, depth-dependent wave arrivals, generated foam, refraction and reflection. Water and foam are not sampled from the photograph. The shore treatment is an analytic approximation, not a stateful fluid solver or an overturning-wave simulation.

`clean-sand.png` is a registered edit generated with the built-in OpenAI image generation tool. It removes shadows from dry sand. The shader extracts shadow transmission from the artwork and clean plate and moves it with the palm gust over the rendered beach. Palm breeze controls both leaves and shadows.

## Generation prompt

Photorealistic premium desktop wallpaper photograph, wide landscape 16:9. Sunny tropical island beach viewed from the beach, fixed camera at human eye level. Pale ivory sand, transparent turquoise lagoon, small distant palm-covered island near the horizon. Lush coconut palms frame the left edge and upper corners, with clearly separated detailed fronds and realistic slender trunks. Restful open blue sky, soft white clouds, fine gentle shoreline foam, subtle shallow-water ripples and sunlight on submerged sand. Natural bright soft daylight, believable color and sophisticated travel photography. No people, boats, buildings, text, logos, illustration or fantasy. Clearly defined shoreline and crowns for localized animation. The generated composition is the source of truth for the shader masks.

## Clean sand editing prompt

Edit target: this exact tropical beach photograph. Produce a registered clean lighting plate for a desktop wallpaper shader. ONLY change the DRY SAND in the LOWER LEFT of the photograph: remove all dark palm-frond and palm-trunk cast shadows from the sand, replacing those shadowed regions with naturally sunlit ivory sand. Preserve fine sand grain, small scattered sand specks, and the same natural gradual exposure as nearby sunlit sand. ALL other pixels must remain unchanged: palm trees and fronds, trunks, sky, clouds, island, rocks, shoreline, foam, wet sand and sea. Keep the EXACT camera, crop, proportions, pixel dimensions and registration of the input image. Do not add anything. Do not remove the actual palm trees. Do not change the shape of the beach or water or any colors outside the dry sand. The purpose is to reconstruct the existing palm shadows separately and animate their light over a stationary clean sand surface. No text, logos or borders.

## Clean surf plate

`clean-surf.png` is a registered generated edit that removes photographic surf. It is retained as a source asset but is not sampled by the 3D ocean shader.

### Editing prompt

Edit target: the supplied exact tropical beach photograph. Create an exactly registered CLEAN SURF PLATE for animation. Remove ALL the white surf foam, the raised breaking wave ridge, and foamy wash along the diagonal water/sand boundary across the entire lower half of the photograph. Replace them with smooth clear shallow turquoise water transitioning naturally to damp pale sand, as if in the quiet moment between two waves. There must be NO fixed white foam ribbons, NO breaking wave ridges, NO frozen rolling wave along the beach. Continue the submerged sandy bottom naturally with very subdued caustics. Also soften the pronounced dark isolated wave ridges farther offshore into calm low contrast turquoise water texture; keep subtle natural surface detail. Preserve EXACT composition, image dimensions, camera, horizon position, island, sky, all palms, rocks, and the dry sand with its ORIGINAL PALM SHADOWS. Do not remove palm shadows. Do not move the shoreline. Do not alter any pixels outside water, foam and wet shoreline sand. No text or border. This is a background plate, the original photographic surf will be extracted by the native shader and moved over this clean background.
