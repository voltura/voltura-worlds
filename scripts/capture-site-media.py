"""Record the actual native renderer at its original 30 fps for the product site.

Development prerequisites: Pillow and imageio-ffmpeg (or the local .deps/python).
Frames stay in ignored artifacts/; only encoded clips and posters go in site/.
"""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.deps/python'))
import imageio_ffmpeg
from PIL import Image

destination = ROOT / 'site/assets'
destination.mkdir(parents=True, exist_ok=True)
for scene in ('aurora', 'orbit', 'smoke', 'bloom'):
    folder = ROOT / 'artifacts/site-media' / scene
    subprocess.run([
        sys.executable, str(ROOT / 'scripts/test_desktop.py'), '--',
        str(ROOT / 'build/Release/VolturaWorlds.exe'),
        '--capture-dir', str(folder), '--scene', scene,
        '--width', '1600', '--height', '900', '--frames', '180', '--record', '240',
    ], check=True)
    subprocess.run([
        imageio_ffmpeg.get_ffmpeg_exe(), '-hide_banner', '-loglevel', 'error', '-y',
        '-framerate', '30', '-i', str(folder / 'frame-%05d.png'),
        '-c:v', 'libx264', '-crf', '22', '-preset', 'slow', '-pix_fmt', 'yuv420p',
        '-an', '-movflags', '+faststart', str(destination / f'{scene}.mp4'),
    ], check=True)
    with Image.open(folder / 'frame-00120.png') as picture:
        picture.convert('RGB').save(destination / f'{scene}.jpg', quality=90)
    print(f'Captured {scene}: 8 seconds, 1600 x 900, native 30 fps.', flush=True)
