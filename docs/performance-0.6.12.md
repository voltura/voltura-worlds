# Voltura Worlds 0.6.12 performance

## Built-in Worlds

Native D3D12 offscreen profiles on Intel Arc graphics covered all 27 built-in Worlds at 1920 × 1080 in SDR. The comparison used the previous local build, which already included the Swedish Christmas and video changes. Each pair used the same scene settings, 108 frames and 32 warmup frames. Smoke used 120 frames and a 64-cell velocity grid. The Journey started 110 seconds into the tour, and Swedish Christmas started 540 seconds into its cycle. Each run used a separate process, and the before/after order alternated by World.

Median whole-frame GPU time fell for 26 of 27 Worlds in this sweep. The median change across Worlds was **3.2% less whole-frame GPU time** and **6.6% less post-processing time**. Smoke's twelve pressure iterations took 0.566 ms before and 0.402 ms after, a **29% reduction** for that stage. GPU clocks and driver activity can move short-run results. In three longer Orbit follow-ups, whole-frame medians were 6.171–6.196 ms before and 6.159–6.191 ms after.

| World | Frame before (ms) | Frame after (ms) | Change | Post before (ms) | Post after (ms) |
| --- | ---: | ---: | ---: | ---: | ---: |
| Aurora | 1.855 | 1.392 | −24.9% | 0.680 | 0.580 |
| Bloom | 1.547 | 1.517 | −1.9% | 0.841 | 0.807 |
| Contour | 0.980 | 0.946 | −3.4% | 0.476 | 0.444 |
| Drift | 2.135 | 2.103 | −1.5% | 0.474 | 0.444 |
| Earth | 2.264 | 2.186 | −3.4% | 0.492 | 0.457 |
| Eclipse | 0.728 | 0.697 | −4.2% | 0.479 | 0.456 |
| Forest Water | 2.134 | 2.052 | −3.9% | 0.647 | 0.554 |
| Jupiter | 1.017 | 0.986 | −3.1% | 0.468 | 0.435 |
| Lattice | 1.077 | 1.049 | −2.5% | 0.458 | 0.429 |
| Mars | 1.420 | 1.377 | −3.0% | 0.475 | 0.437 |
| Mercury | 0.858 | 0.827 | −3.6% | 0.467 | 0.435 |
| Moon | 0.895 | 0.853 | −4.7% | 0.484 | 0.441 |
| Neptune | 1.163 | 1.123 | −3.5% | 0.474 | 0.437 |
| Orbit | 6.209 | 7.116 | +14.6% | 1.219 | 1.299 |
| Plastic metal | 0.793 | 0.750 | −5.4% | 0.464 | 0.433 |
| Pluto | 0.797 | 0.750 | −5.9% | 0.499 | 0.454 |
| Ripples | 4.842 | 4.711 | −2.7% | 0.458 | 0.439 |
| Saturn | 1.036 | 1.003 | −3.1% | 0.470 | 0.437 |
| Silk | 1.005 | 0.977 | −2.8% | 0.463 | 0.438 |
| Smoke | 9.799 | 9.610 | −1.9% | 0.470 | 0.425 |
| Solar System Journey | 1.245 | 1.200 | −3.6% | 0.494 | 0.461 |
| Sun | 0.970 | 0.939 | −3.2% | 0.460 | 0.430 |
| Swedish Christmas | 0.886 | 0.849 | −4.2% | 0.498 | 0.468 |
| Tropical Island | 4.745 | 4.716 | −0.6% | 0.510 | 0.481 |
| Tropical Paradise | 1.187 | 1.157 | −2.5% | 0.680 | 0.659 |
| Uranus | 0.996 | 0.964 | −3.3% | 0.463 | 0.432 |
| Venus | 1.473 | 1.446 | −1.9% | 0.464 | 0.436 |

Frame columns use GPU medians. Post-processing columns use stage means. These metrics describe offscreen rendering.

## Desktop playback and memory

- Pausing a 4K built-in World reduced the renderer's estimated GPU allocation from 216.50 MiB to 145.31 MiB. Playback resumed and the renderer recreated the buffers. The median process private commit after active SDR captures was 1.68 MiB higher in this sweep.
- Artwork loading reuses its decoded pixel buffer for mip generation. Each Swedish Christmas image avoids about 2 MiB of temporary mip storage, and leaving the World can free 18.75 MiB of artwork textures.
- In an earlier 12-second comparison against 0.6.11, the same clip used 31% less process CPU time and 73% less renderer GPU time as a 4K wallpaper at half speed. The desktop frame cap was 60 FPS. Savings depend on the video's frame rate, playback speed and display cadence.

## Image comparison

The sampled Aurora, Bloom, Forest Water, Orbit, Swedish Christmas and Tropical Island frames matched pixel for pixel. Earth, Ripples and Solar System Journey had some one-code-value differences in SDR. The evolving Smoke frame differed by up to six code values in 2.6% of its pixels after 120 simulation frames. Earth also completed an odd-sized FP16 capture with its statistics card enabled, and Smoke completed the D3D12 debug capture.
