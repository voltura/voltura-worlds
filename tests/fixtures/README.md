# Video fixtures

These are synthetic FFmpeg `testsrc2` patterns generated locally, with no external artwork or audio. Both clips are 320x180 at 30 FPS for two seconds.

Regenerate using FFmpeg:

```powershell
ffmpeg -y -f lavfi -i testsrc2=size=320x180:rate=30 -t 2 -an -c:v libx264 -pix_fmt yuv420p -movflags +faststart h264.mp4
ffmpeg -y -f lavfi -i testsrc2=size=320x180:rate=30 -t 2 -an -c:v mpeg4 -pix_fmt yuv420p unsupported.mp4
```

The second fixture intentionally uses an unsupported codec inside an MP4 container.
