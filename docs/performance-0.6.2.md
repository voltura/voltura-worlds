# Full-set measurements for 0.6.2

## Outcome

Measured all 26 built-in Worlds plus personal video: **27/27 workloads**. The 20% rendering-throughput target was reached in **5/27**, so it was **not achieved across all visualizations**. Process private memory decreased by **27.9–57.6%**, exceeding the 20% memory target in **27/27** measured workloads. This does not mean every type of resource decreased by 20%: estimated GPU texture allocation is mostly unchanged on the native swap-chain path outside Journey; power was not measured.

## Method and limits

Intel Arc graphics, SDR, 1920 × 1080, 120 frames with 30 warmup frames, default scene parameters and GPU stage timestamps. Video uses the same bundled Aurora site clip. Each process ran separately. Both columns use the existing native profiler through a hidden composition window, with the same 16 ms per-frame profiling sleep.

The control column is the retained composition-copy backend, selected by `--composition-profile --composition-copy`. The final column is the native D3D12 composition swap chain selected by `--composition-profile`; `presentationPath` confirmed this for every workload. These are backend comparisons inside the optimization run, not a claim that an installed 0.6.1 physical desktop was measured end to end. A separate actual 0.6.1 offscreen baseline was captured for all 27 workloads, but it cannot be directly compared with composition timing because it excludes presentation. The copy profiles include the initial shader-specialization changes and retain the original Journey texture residency; the final profiles include bounded Journey residency.

GPU throughput change is `(copy median / native median - 1) × 100`; it is not presented FPS. Private memory is Windows process private committed memory measured at the end of the capture, not physical resident RAM or total system memory. GPU clocks, temperature, driver caching, DWM work and short profile duration affect timings. No resolution, frame cap or detail setting was reduced. These results do not guarantee a 20% gain on other settings or hardware, or sustained behind-icons behavior.

| Visualization | Copy GPU ms | Native GPU ms | GPU throughput change | Copy private MiB | Native private MiB | Private reduction |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Aurora | 2.479 | 2.173 | +14.1% | 384.8 | 242.3 | 37.0% |
| Orbit | 10.934 | 11.749 | -6.9% | 493.7 | 350.2 | 29.1% |
| Smoke | 10.536 | 10.088 | +4.4% | 433.6 | 288.6 | 33.4% |
| Bloom | 2.776 | 2.511 | +10.5% | 426.5 | 282.7 | 33.7% |
| Silk | 2.714 | 2.371 | +14.4% | 371.6 | 227.1 | 38.9% |
| Eclipse | 1.831 | 1.467 | +24.8% | 371.6 | 227.8 | 38.7% |
| Lattice | 2.937 | 2.605 | +12.7% | 371.3 | 228.0 | 38.6% |
| Drift | 5.997 | 5.641 | +6.3% | 371.6 | 227.9 | 38.7% |
| Contour | 2.585 | 2.273 | +13.7% | 371.4 | 227.7 | 38.7% |
| Plastic Metal | 2.030 | 1.684 | +20.5% | 372.0 | 227.9 | 38.8% |
| Ripples | 9.232 | 9.683 | -4.7% | 374.7 | 231.1 | 38.3% |
| Mercury | 1.896 | 1.571 | +20.7% | 487.5 | 344.9 | 29.2% |
| Venus | 3.922 | 3.583 | +9.5% | 387.8 | 244.6 | 36.9% |
| Earth | 5.456 | 5.136 | +6.2% | 514.2 | 370.9 | 27.9% |
| Mars | 3.709 | 3.389 | +9.5% | 487.8 | 344.8 | 29.3% |
| Jupiter | 2.282 | 1.956 | +16.6% | 413.6 | 270.1 | 34.7% |
| Saturn | 2.677 | 2.320 | +15.4% | 400.3 | 257.0 | 35.8% |
| Uranus | 2.497 | 2.132 | +17.1% | 388.1 | 244.6 | 37.0% |
| Neptune | 2.933 | 2.609 | +12.4% | 387.8 | 244.7 | 36.9% |
| Moon | 2.112 | 1.774 | +19.0% | 489.8 | 346.4 | 29.3% |
| Pluto | 1.750 | 1.435 | +21.9% | 413.4 | 271.1 | 34.4% |
| Sun | 2.551 | 2.179 | +17.1% | 422.6 | 279.8 | 33.8% |
| Forest Water | 5.551 | 5.250 | +5.7% | 384.9 | 243.0 | 36.9% |
| Tropical Paradise | 2.611 | 2.248 | +16.1% | 384.3 | 243.0 | 36.8% |
| Tropical Island | 10.619 | 10.616 | +0.0% | 413.9 | 272.0 | 34.3% |
| Solar System Journey | 2.150 | 1.803 | +19.2% | 937.0 | 397.5 | 57.6% |
| Video | 0.636 | 0.316 | +101.0% | 437.1 | 294.3 | 32.7% |

## Follow-up observations

Orbit and Ripples were repeated after their first profiles showed slower medians. Orbit's repeat measured 12.674 ms native versus 11.625 ms copy; a subsequent direct-surface run measured 12.797 ms. An earlier direct-surface run measured 8.255 ms. Ripples' repeat measured 8.467 ms native versus 11.815 ms copy, reversing the first comparison. These variations are preserved here instead of replacing table entries with favorable repeats. They prevent a claim of a reliable speedup in every World. The native backend remains shared across the full library; the surface and copy backends remain compatibility options.

Native composition, HDR Earth with its statistics card, and a Journey arrival with bounded residency completed application GPU debug diagnostics. The older copy backend emitted a compatibility-layer debug validation message while reflecting a D3D12-created wrapped resource; its timing comparisons ran without GPU debug validation. The native path completed those diagnostics without that message. Builds and code review were used; no automated tests, fixtures or new test harnesses were added or run.

Physical desktop appearance, physical HDR, monitor reconnection and driver-specific fallback behavior remain for user assessment. The diagnostic windows were hidden and no automated UI interaction was used.
