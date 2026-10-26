"""Body-card compositing, palette, viewport and GPU lifecycle regressions."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
BODIES = ('mercury', 'venus', 'earth', 'moon', 'mars', 'jupiter', 'saturn',
          'uranus', 'neptune', 'pluto', 'sun')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'artifacts/body-statistics')
    args = parser.parse_args()
    results = {}

    def capture(name, scene, enabled=True, width=640, height=360, palette=0, extra=()):
        folder = args.output / name
        command = [sys.executable, str(ROOT / 'scripts/test_desktop.py'), '--',
                   str(args.executable.resolve()), '--capture-dir', str(folder),
                   '--scene', scene, '--width', str(width), '--height', str(height),
                   '--frames', '1', '--palette', str(palette), '--debug-gpu', *extra]
        if enabled:
            command.append('--show-body-statistics')
        run = subprocess.run(command, capture_output=True, text=True, timeout=240)
        if run.returncode:
            raise RuntimeError(f'{name}: {run.stdout}\n{run.stderr}')
        with Image.open(folder / f'{scene}.png') as image:
            return image.convert('RGB')

    for scene in BODIES:
        off = capture(f'{scene}-off', scene, enabled=False)
        on = capture(f'{scene}-on', scene)
        bbox = ImageChops.difference(off, on).getbbox()
        assert bbox and 320 < bbox[0] < 640 and bbox[1] > 0, (scene, bbox)
        assert 610 < bbox[2] < 640 and 325 < bbox[3] < 360, (scene, bbox)
        # Only the lower-right card changes; the underlying scene is untouched.
        assert ImageChops.difference(off.crop((0, 0, 320, 360)),
                                    on.crop((0, 0, 320, 360))).getbbox() is None
        results[scene] = {'changedBounds': bbox}

    for scene in ('orbit', 'silk', 'off'):
        off = capture(f'{scene}-off', scene, enabled=False)
        on = capture(f'{scene}-on', scene)
        assert ImageChops.difference(off, on).getbbox() is None, scene

    # Visual QA artifacts include every palette at full HD, plus tiny/portrait/4K.
    for palette in range(4):
        capture(f'earth-1080p-palette-{palette}', 'earth', width=1920, height=1080, palette=palette)
        capture(f'sun-1080p-palette-{palette}', 'sun', width=1920, height=1080, palette=palette)
    capture('neptune-aqua', 'neptune', width=1920, height=1080, palette=1)
    capture('uranus-aqua', 'uranus', width=1920, height=1080, palette=1)
    capture('pluto-4k', 'pluto', width=3840, height=2160)
    capture('venus-portrait', 'venus', width=480, height=800)
    capture('moon-small', 'moon', width=320, height=240)
    capture('sun-minimum', 'sun', width=64, height=64)
    capture('earth-lifecycle', 'earth', extra=('--validate-lifecycle',))
    (args.output / 'results.json').write_text(json.dumps(results, indent=2))
    print('Body statistics: all bodies, palettes, viewport sizes, ineligible scenes and GPU lifecycle passed.')


if __name__ == '__main__':
    main()
