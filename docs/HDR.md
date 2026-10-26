# HDR desktop output

Auto selects FP16 scRGB (`R16G16B16A16_FLOAT`, `RGB_FULL_G10_NONE_P709`) only for a built-in World on an actively HDR Windows output. SDR uses the original eight-bit path; imported H.264 video always uses that path. Output changes affect only the selected renderer's back buffers and output PSO. Immutable artwork and scene pipelines stay shared per adapter. Settings remain version 1 with an optional display `output` string (`auto`/`sdr`). Runtime measurements and fallback decisions are never saved.

Discovery matches HMONITOR and adapter LUID, then active QueryDisplayConfig targets, rather than Explorer's spanning wallpaper parent. DXGI's active PQ signal is required; Advanced Color support or SDR automatic color management is insufficient. Target HDR state is also checked (including the newer Advanced Color query when available). Clones require every target to be eligible and use minimum luminance limits. SDR white uses the documented value times 0.08 nits. Invalid/missing white and peak data fall back to 203/400 nits, with white bounded by peak. Discovery runs once per second during active playback and on preference/video changes. Resume and recreated renderers recheck before drawing. Unchanged measurements do not resize targets. Presentation failures roll back to SDR and are cached until configuration changes or Retry now recreates renderers. Device removal uses adapter recovery.

The existing 32-float FrameData is unchanged. Four pixel root constants at b1 supply HDR mode, W/80 and P/W. HDR preserves scene/exposure/bloom/Orbit composition, then adds a smooth, bounded highlight shoulder to the existing linear tone map. It omits SDR gamma, clipping and eight-bit dither. Cards decode sRGB and blend in linear light at reference white. The SDR branches remain unchanged.

## Diagnostics

Normal capture commands always use SDR, regardless of monitor state. For explicit synthetic FP16 diagnostics:

```powershell
./build/Release/VolturaWorlds.exe --capture-dir artifacts/hdr/orbit --scene orbit --width 1920 --height 1080 --frames 180 --hdr-output --hdr-white 203 --hdr-peak 400 --debug-gpu
```

This writes `orbit.rgba16f` and `orbit.rgba16f.json`, not a PNG. Raw bytes are tightly packed, row-major, little-endian RGBA binary16, eight bytes per pixel. Metadata includes dimensions, color space, reference-white and peak nits and a false physical-verification flag. Values are linear scRGB, with 1.0 representing 80 nits. Video and recorded PNG sequences reject HDR diagnostics. Synthetic values are accepted only by capture/test harnesses.

`worlds_hdr_tests` compares GPU FP16 readbacks to a separate double-precision CPU oracle, including neutral/color ramps and synthetic card decoding/blending. It tests mixed-format resources, repeated transitions, resizing, pause/resume and actual test-device removal/recovery with D3D12 validation. Its synthetic fixtures never change Windows HDR. `scripts/verify-hdr-sdr.py before|after` records fixed 320x180 captures of every built-in World and requires exact PNG hashes. The before phase must run on the current checkout before rendering changes.

Evidence is stored under `artifacts/hdr/`. Offscreen timings are short native diagnostic measurements; they do not establish physical presentation performance. FP16 readback proves the application's numeric output, not the panel's actual brightness. Physical behind-icons HDR, mixed HDR/SDR monitors, live HDR toggles, sleep/resume, Explorer restart, cable reconnection and ten minutes of HDR desktop playback require suitable hardware and separate observation. They must not be inferred from a PNG, private desktop or synthetic swap chain.

## API references

- [Microsoft Advanced Color guidance](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range)
- [SDR reference-white units](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_sdr_white_level)
- [SetColorSpace1](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_4/nf-dxgi1_4-idxgiswapchain3-setcolorspace1)
