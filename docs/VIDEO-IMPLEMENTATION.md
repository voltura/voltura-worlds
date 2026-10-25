# Native video playback core

`VideoPlayer` uses the Windows Media Engine in frame-server mode, with a D3D11 device on the same adapter as the D3D12 renderer. Windows manages decoding; GPU-shared BGRA textures and a shared fence transfer frame ownership between the APIs. Hardware acceleration is enabled through the DXGI device manager, but hardware-decoder selection has not yet been measured.

All public methods run on the rendering thread. Start/resume with `setPaused(false)` and pause with `setPaused(true)`. `update()` asks for a new frame and places a producer wait on the D3D12 queue. When a frame exists, callers must transition it from COMMON to their read state, return it to COMMON, submit that work, and call `consumed()` after every use, including reused frames. This prevents decoder writes from racing GPU reads. Playback is muted and looping.

Metadata validation currently checks an MP4 container, H.264 video, bounded dimensions, and rejects reported PQ/HLG transfer functions. More HDR and damaged-stream fixtures remain necessary for release acceptance. Codec packs are not installed.

Validation executable: `build/Release/worlds_video_tests.exe`, with an SDR H.264 test MP4 and an unsupported-codec MP4 as arguments. Run through `scripts/test_desktop.py`. Repeatable synthetic fixtures and regeneration instructions are in `tests/fixtures/`; CTest runs the GPU contract automatically. It checks changing D3D12 readback pixels, a loop boundary, paused playback position, resume, and unsupported-codec rejection. It does not measure audible/display latency or prove decoder inactivity with GPU instrumentation.

The core is connected to native scene rendering, Fit/Fill positioning, managed personal video import, and library controls. Video World imports validate metadata before confirmation. CTest covers personal import cancellation, confirmed copying, and use after moving the original. Additional HDR/damaged-stream and endurance coverage remains open.

API references: [TransferVideoFrame](https://learn.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengine-transfervideoframe), [OnVideoStreamTick](https://learn.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengine-onvideostreamtick), and [D3D12 queue/fence synchronization](https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/direct3d12/user-mode-heap-synchronization.md).
