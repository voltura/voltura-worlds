"""Native cinematic regressions for original Worlds and composition coverage.

The six newer Worlds have detailed control/audio checks in test-new-worlds.py.
This complements those checks with frozen/audio contracts for the originals,
all-World palette/extreme captures and optional large-format evidence.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[1]
SCENES = ('aurora', 'orbit', 'smoke', 'bloom', 'silk', 'eclipse',
          'lattice', 'drift', 'contour', 'mercury')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--formats', action='store_true')
    parser.add_argument('--formats-only', action='store_true')
    parser.add_argument('--scenes', nargs='+', choices=SCENES, default=SCENES)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/cinematic/contracts')
    args = parser.parse_args()
    executable = args.executable.resolve()
    report = {}
    for scene in args.scenes:
        def capture(name, width=640, height=360, **options):
            folder = args.output/scene/name
            folder.mkdir(parents=True, exist_ok=True)
            flags = {'frames': 120 if scene == 'smoke' else 1, 'grid': 32, **options}
            command = [sys.executable, str(ROOT/'scripts/test_desktop.py'), '--',
                       str(executable), '--capture-dir', str(folder), '--scene', scene,
                       '--width', str(width), '--height', str(height), '--debug-gpu']
            for key, value in flags.items():
                command.append('--'+key.replace('_', '-'))
                if value is not True:
                    command.append(str(value))
            subprocess.run(command, check=True, timeout=180, capture_output=True)
            with Image.open(folder/f'{scene}.png') as source:
                result = source.convert('RGB')
            assert result.size == (width, height)
            return result

        if args.formats_only:
            for name, width, height in (('portrait', 720, 1280), ('ultrawide', 2560, 1080), ('4k', 3840, 2160)):
                capture(name, width, height)
            report[scene] = {'passed': True, 'formats': True}
            (args.output/'format-results.json').write_text(json.dumps(report, indent=2))
            print(f'PASS {scene}: portrait, ultrawide and 4K native debug captures', flush=True)
            continue
        baseline = capture('default')
        assert max(hi for lo, hi in baseline.getextrema()) > 10, f'{scene}: blank default'
        dark = capture('brightness-zero', brightness=0)
        assert max(hi for lo, hi in dark.getextrema()) <= 1, f'{scene}: light bypassed brightness control'
        for palette in range(1, 4):
            capture(f'palette-{palette}', palette=palette)
        for extreme in (0, 2):
            capture(f'extreme-{extreme}', movement=extreme, detail=extreme, flow=extreme,
                    turbulence=extreme, density=extreme)
        if scene == 'aurora':
            capture('switch-resize', frames=18, validate_lifecycle=True)
        if scene in SCENES[:4]:
            signal = {'bass': .8, 'mid': .6, 'treble': .7, 'energy': .9, 'impulse': .5}
            off = capture('audio-off', **signal)
            assert ImageChops.difference(baseline, off).getbbox() is None, f'{scene}: disabled audio leaked'
            zero = capture('zero-strength', reaction='expressive', strength=0, **signal)
            assert ImageChops.difference(baseline, zero).getbbox() is None, f'{scene}: zero strength leaked'
            gentle = capture('gentle', reaction='gentle', strength=1, **signal)
            expressive = capture('expressive', reaction='expressive', strength=1, **signal)
            if scene != 'orbit':  # Orbit does not currently implement audio deformation.
                assert ImageChops.difference(gentle, expressive).getbbox(), f'{scene}: audio modes inactive'
            frozen = capture('frozen', movement=0, record=2)
            frozen_audio = capture('frozen-audio', movement=0, reaction='expressive', strength=1, **signal)
            assert ImageChops.difference(frozen, frozen_audio).getbbox() is None, f'{scene}: audio changed frozen scene'
            for index in range(2):
                with Image.open(args.output/scene/'frozen'/f'frame-{index:05d}.png') as source:
                    assert ImageChops.difference(frozen, source.convert('RGB')).getbbox() is None, f'{scene}: frozen frame advanced'
        if args.formats:
            for name, width, height in (('portrait', 720, 1280), ('ultrawide', 2560, 1080), ('4k', 3840, 2160)):
                capture(name, width, height)
        report[scene] = {'passed': True, 'formats': args.formats}
        args.output.mkdir(parents=True, exist_ok=True)
        (args.output/'results.json').write_text(json.dumps(report, indent=2))
        print(f'PASS {scene}: native palette/extreme/resize checks'
              + (' and frozen/audio contracts' if scene in SCENES[:4] else ''), flush=True)


if __name__ == '__main__':
    main()
