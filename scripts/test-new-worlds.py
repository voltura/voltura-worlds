"""Bounded native regression coverage for the six procedural Worlds."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
from PIL import Image, ImageChops, ImageStat

SCENES = ('silk', 'eclipse', 'lattice', 'drift', 'contour', 'mercury')
SCRIPTS = Path(__file__).resolve().parent

def difference(a, b):
    return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3

def main():
    executable = str(Path(sys.argv[1]).resolve())
    report = {}
    with tempfile.TemporaryDirectory(prefix='Voltura-new-worlds-') as folder:
        root = Path(folder)
        for scene in SCENES:
            sequence = 0
            def capture(**options):
                nonlocal sequence
                sequence += 1
                out = root / f'{scene}-{sequence}'
                flags = {'start-time': 8, 'quality': 'auto', **options}
                command = [sys.executable, str(SCRIPTS/'test_desktop.py'), '--', executable,
                           '--capture-dir', str(out), '--scene', scene, '--width', '640',
                           '--height', '360', '--frames', '1', '--debug-gpu']
                for key, value in flags.items():
                    command += ['--'+key, str(value)]
                subprocess.run(command, check=True, timeout=40, capture_output=True)
                with Image.open(out/(scene+'.png')) as image:
                    return image.convert('RGB')
            baseline = capture()
            assert max(ImageStat.Stat(baseline).stddev) > 2, f'{scene}: blank output'
            measured = {}
            variants = {'animation': {'start-time':14}, 'brightness': {'brightness':.4},
                        'detail': {'detail':0}, 'flow': {'flow':.25},
                        'turbulence': {'turbulence':2}, 'density': {'density':0},
                        'palette': {'palette':2}, 'quality': {'quality':'eco'}}
            for name, options in variants.items():
                measured[name] = difference(baseline, capture(**options))
                assert measured[name] > .005, f'{scene}: {name} has no visible response'
            frozen = capture(movement=0)
            assert difference(frozen,capture(movement=0,**{'start-time':14})) == 0, f'{scene}: Movement 0 animates'
            signal = {'bass':.8,'mid':.6,'treble':.7,'energy':.9,'impulse':.5}
            assert difference(baseline,capture(**signal)) == 0, f'{scene}: audio Off responds'
            assert difference(baseline,capture(reaction='expressive',strength=0,**signal)) == 0, f'{scene}: strength 0 responds'
            gentle = capture(reaction='gentle',strength=1,**signal)
            expressive = capture(reaction='expressive',strength=1,**signal)
            measured['audio'] = difference(baseline,expressive)
            measured['reactionModes'] = difference(gentle,expressive)
            assert measured['audio'] > .005 and measured['reactionModes'] > .005, f'{scene}: audio mapping inactive'
            assert difference(frozen,capture(movement=0,reaction='expressive',strength=1,**signal)) == 0, f'{scene}: audio moves frozen scene'
            report[scene] = measured
            print(f'PASS {scene}: native parameters, audio isolation/strength/modes, quality and frozen movement',flush=True)
    print(json.dumps(report,indent=2))

if __name__ == '__main__':
    main()
