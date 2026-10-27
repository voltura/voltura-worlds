"""Record the actual native renderer at its original 30 fps for the product site.

Development prerequisites: Pillow and imageio-ffmpeg (or the local .deps/python).
Frames stay in ignored artifacts/; only encoded clips and posters go in site/.
"""
from pathlib import Path
import argparse
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.deps/python'))
import imageio_ffmpeg
from PIL import Image, ImageDraw, ImageFont, ImageOps

SCENES = ('aurora', 'orbit', 'smoke', 'bloom', 'silk', 'eclipse',
          'lattice', 'drift', 'contour', 'plastic_metal', 'ripples',
          'mercury', 'venus', 'earth', 'mars', 'jupiter', 'saturn', 'uranus', 'neptune', 'moon', 'pluto', 'sun', 'forest_water')
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--scenes', nargs='+', choices=SCENES, default=list(SCENES),
                    help='Capture only these Worlds, retaining existing media for the others.')
parser.add_argument('--social-only', action='store_true',
                    help='Regenerate the social preview from existing native posters.')
options = parser.parse_args()

destination = ROOT / 'site/assets'
destination.mkdir(parents=True, exist_ok=True)
for scene in (() if options.social_only else options.scenes):
    folder = ROOT / 'artifacts/site-media' / scene
    subprocess.run([
        sys.executable, str(ROOT / 'scripts/run-private-desktop.py'), '--',
        str(ROOT / 'build/Release/VolturaWorlds.exe'),
        '--capture-dir', str(folder), '--scene', scene,
        '--width', '1600', '--height', '900', '--frames', '180', '--record', '240',
    ], check=True)
    subprocess.run([
        imageio_ffmpeg.get_ffmpeg_exe(), '-hide_banner', '-loglevel', 'error', '-y',
        '-framerate', '30', '-i', str(folder / 'frame-%05d.png'),
        '-c:v', 'libx264', '-crf', '22', '-preset', 'medium', '-pix_fmt', 'yuv420p',
        '-an', '-movflags', '+faststart', str(destination / f'{scene}.mp4'),
    ], check=True)
    with Image.open(folder / 'frame-00120.png') as picture:
        picture.convert('RGB').save(destination / f'{scene}.jpg', quality=90)
    print(f'Captured {scene}: 8 seconds, 1600 x 900, native 30 fps.', flush=True)

# Social cards use the same native frames as the public recordings.
missing = [scene for scene in SCENES if not (destination / f'{scene}.jpg').exists()]
if missing:
    raise SystemExit('Social preview needs native posters: ' + ', '.join(missing))
preview = Image.new('RGB', (1200, 630), '#080c12')
draw = ImageDraw.Draw(preview)
font_dir = Path('C:/Windows/Fonts')
def font(size, bold=False):
    return ImageFont.truetype(str(font_dir / ('segoeuib.ttf' if bold else 'segoeui.ttf')), size)
draw.text((28, 19), 'voltura worlds', font=font(25), fill='#b5c2cc')
draw.text((26, 58), 'Your desktop, alive.', font=font(67), fill='#f1f4f7')
draw.text((30, 153), '23 animated Worlds. A whole Solar System to explore.', font=font(25), fill='#98eddc')
# Eleven new native scenes, with Earth as the full-frame feature.
with Image.open(destination / 'earth.jpg') as source:
    tile = ImageOps.contain(source.convert('RGB'), (584, 329), Image.Resampling.LANCZOS)
    preview.paste(tile, (28, 220))
draw.text((28, 566), 'Your world. And beyond.', font=font(28), fill='#f1f4f7')
for index, scene in enumerate(('mercury', 'venus', 'mars', 'jupiter', 'saturn', 'uranus', 'neptune', 'moon', 'pluto', 'sun')):
    x = 644 + (index % 2) * 268
    y = 207 + (index // 2) * 79
    with Image.open(destination / f'{scene}.jpg') as source:
        tile = ImageOps.contain(source.convert('RGB'), (106, 60), Image.Resampling.LANCZOS)
        preview.paste(tile, (x, y))
    draw.text((x + 117, y + 17), scene.capitalize(), font=font(19), fill='#d5dce0')
preview.save(destination / 'social-preview.jpg', quality=93, subsampling=0)
print('Updated social-preview.jpg: 1200 x 630, native Solar System scenes.', flush=True)
