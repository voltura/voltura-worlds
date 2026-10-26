# Video fixtures

These are synthetic FFmpeg `testsrc2` patterns generated locally, with no external artwork or audio. Both clips are 320x180 at 30 FPS for two seconds.

Regenerate using FFmpeg:

```powershell
ffmpeg -y -f lavfi -i testsrc2=size=320x180:rate=30 -t 2 -an -c:v libx264 -pix_fmt yuv420p -movflags +faststart h264.mp4
ffmpeg -y -f lavfi -i testsrc2=size=320x180:rate=30 -t 2 -an -c:v mpeg4 -pix_fmt yuv420p unsupported.mp4
```

The second fixture uses MPEG-4 Part 2 inside MP4. It was the historical unsupported-codec test; broader native inspection now accepts its metadata, while actual playback still depends on the Windows decoder path.

Additional synthetic container, codec, rotation, aspect, rate, HDR and protected fixtures are generated on demand into `artifacts/broader-video/fixtures` by `scripts/generate-video-fixtures.py`. FFmpeg is used only for development/testing. No runtime codec binaries are shipped.
