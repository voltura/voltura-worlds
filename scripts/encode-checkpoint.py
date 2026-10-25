"""Encode native capture frames without retiming motion or synthesizing frames."""
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / '.deps' / 'python'))
import imageio_ffmpeg

ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
jobs = [(scene, root / 'artifacts' / 'native' / scene) for scene in ('aurora', 'orbit', 'smoke')]
if len(sys.argv) == 3:
    scene = sys.argv[2]
    if scene not in ('aurora', 'orbit', 'smoke', 'bloom'):
        raise SystemExit('Expected aurora, orbit, smoke, or bloom')
    jobs = [(scene, pathlib.Path(sys.argv[1]).resolve())]
for scene, folder in jobs:
    subprocess.run([
        ffmpeg, '-hide_banner', '-loglevel', 'error', '-y',
        '-framerate', '30', '-i', str(folder / 'frame-%05d.png'),
        '-c:v', 'libx264', '-crf', '18', '-pix_fmt', 'yuv420p',
        '-movflags', '+faststart', str(folder / f'{scene}-motion.mp4')
    ], check=True)
    print(folder / f'{scene}-motion.mp4')
