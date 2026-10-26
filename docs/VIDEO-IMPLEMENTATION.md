# Native video playback core

`VideoPlayer` uses the Windows Media Engine in frame-server mode, with a D3D11 device on the same adapter as the D3D12 renderer. Windows manages decoding; GPU-shared BGRA textures and a shared fence transfer frame ownership between the APIs. Hardware acceleration is enabled through the DXGI device manager, but hardware-decoder selection has not yet been measured.

All public methods run on the rendering thread. Start/resume with `setPaused(false)` and pause with `setPaused(true)`. `update()` asks for a new frame and places a producer wait on the D3D12 queue. When a frame exists, callers must transition it from COMMON to their read state, return it to COMMON, submit that work, and call `consumed()` after every use, including reused frames. This prevents decoder writes from racing GPU reads. Playback is muted and looping.

Personal imports inspect supported native video containers, bounded landscape/portrait geometry and reported or encoded PQ/HLG color metadata. Known HDR/protected video is rejected. Version-1 World packages retain their H.264 MP4 restriction. Codec packs are not installed.

The core is connected to native scene rendering, Fit/Fill positioning, managed personal video import, and library controls. Video World and personal imports require native GPU preflight before confirmation. Physical monitor presentation and hardware-decoder selection remain separate verification work.

API references: [TransferVideoFrame](https://learn.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengine-transfervideoframe), [OnVideoStreamTick](https://learn.microsoft.com/en-us/windows/win32/api/mfmediaengine/nf-mfmediaengine-imfmediaengine-onvideostreamtick), and [D3D12 queue/fence synchronization](https://github.com/MicrosoftDocs/win32/blob/docs/desktop-src/direct3d12/user-mode-heap-synchronization.md).


## Broader personal-video import, framing and rate controls

Personal import uses content-based container inspection rather than the filename extension. The container allowlist is MP4/M4V, QuickTime MOV, Matroska/WebM, ASF/WMV and AVI. The native Windows Media Foundation reader supplies stream metadata; no application codec whitelist substitutes for actual decoding. Inspection bounds landscape and portrait to a 7680-pixel long edge and 4320-pixel short edge. Known PQ/HLG transfer metadata is rejected, including explicit ISO/Matroska color information and H.264 SPS VUI where the reader omits it. Encrypted ISO video sample entries are rejected before preflight. Only supported installed native decoders are used; there is no runtime FFmpeg dependency.

Preflight creates an independent D3D12 queue matched to the selected adapter and uses the production VideoPlayer's muted frame-server path, D3D11/D3D12 shared texture and fence synchronization. It requires a frame and GPU readback before producing the PNG preview, with cancellation and a 15-second first-frame/readback deadline. Decoder teardown and GPU cleanup run off the UI thread. Metadata reads themselves use the synchronous Windows source reader; its return time is governed by the native parser and local filesystem. No unbounded frame-scanning loop is used. The managed copy is separately preflighted before atomic installation; generated directory names and a detected canonical media extension preserve the actual container bytes.

Media Engine applies rotation and pixel-aspect correction. Thumbnail dimensions use the resulting display aspect ratio. Fill offsets use normalized position, Fit retains its centered bars, and video-specific FrameData spare values do not modify the HDR b1 output block. The default centered shader path retains its previous calculation.

The optional display `videoSettings` object stores positionX/positionY in [0,1] and one of seven whitelisted speeds. Missing fields default to 0.5/0.5/1. The renderer uses SetDefaultPlaybackRate and SetPlaybackRate together because Play restores the default rate. Changes do not seek or restart the decoder. UI speed requests are held outside persisted settings until the adapter worker acknowledges them; rejected requests restore the last accepted rate. Source changes and Restore defaults clear pending requests. Saved unsupported rates fall back to the newly created player's accepted 1x rate with status feedback. No experimental scene constants or animation-clock multipliers implement video speed.

The HDR resolver still forces every video format to SDR, retaining Auto/SDR and performance preferences. Shared artwork/cache ownership, independent adapter workers and local renderer recovery are unchanged. Version-1 `.world` validation is strict: H.264 MP4 only, with no nondefault positioning/speed written as version 1.

Version-2 desktop presets bundle accepted video containers and preserve framing/speed; see [PRESETS.md](PRESETS.md). Preset application uses independent GPU preflight on each mapped adapter and verifies the saved playback rate before committing assignments. An explicitly requested adapter must be available; validation does not silently substitute another GPU.
