# Performance measurements for 0.6.1

Compared the installed 0.6.0 release with the 0.6.1 Release build on the same Intel Arc graphics adapter, using the app's existing native GPU profiling mode. No automated tests or new profiling harnesses were added. Each process ran separately to avoid overlapping GPU workloads.

Planet captures use 1920 × 1080, 600 frames and 120 warmup frames with GPU stage profiling. Journey uses 1920 × 1080, 240 frames, the default 11-frame warmup and a 110-second start offset. Video uses the same bundled Aurora site clip at 3840 × 2160 for 120 frames. All use SDR output. A separate ordinary desktop launch configured HDR successfully; physical HDR appearance was not assessed.

| Workload | Old median GPU ms | New median GPU ms | GPU time reduction | Old private MiB | New private MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Jupiter | 1.3817 | 0.9916 | 28.2% | 291.5 | 277.5 |
| Uranus | 0.9820 | 0.9529 | 3.0% | 266.1 | 252.3 |
| Moon | 0.9842 | 0.8551 | 13.1% | 368.7 | 354.5 |
| Journey | 1.2818 | 1.1939 | 6.9% | 812.0 | 800.8 |
| Video | 1.0948 | 1.0986 | -0.3% | 410.0 | 322.9 |

The 4K video renderer's estimated GPU allocation fell from 146,968,576 to 72,318,976 bytes (50.8%). Video process private memory fell by 21.2% in these diagnostic runs. Jupiter exceeded the 20% GPU frame-time reduction target. Other sampled Worlds improved less; these results do not establish a 20% app-wide gain or a guarantee for every visualization.

GPU timestamps measure offscreen rendering work, not presented FPS, display latency or power consumption. Private memory is the process counter recorded after native capture, including driver and capture overhead; it is not a steady desktop-memory claim. GPU allocation is the renderer's estimate, not total system GPU memory. GPU clocks and driver caching can affect short profiles.

Changes also remove recurring work outside these captures: settings and desktop-menu idle polling, redundant per-display coverage enumeration, duplicate pipeline creation, silent-channel FFTs and package-buffer copies. Their individual savings were reviewed in code but were not separately timed. The shared Gaussian blur retains its kernel, with the original sampling path retained for non-integral quarter-size targets. Visual assessment remains with the user.
