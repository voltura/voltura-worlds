"""Bounded native regression coverage for the procedural Worlds."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
from PIL import Image, ImageChops, ImageFilter, ImageStat

SCENES = ('silk', 'eclipse', 'lattice', 'drift', 'contour', 'plastic_metal', 'ripples')
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
                flags = {'start-time': 8, 'quality': 'auto', 'width':640, 'height':360, **options}
                command = [sys.executable, str(SCRIPTS/'test_desktop.py'), '--', executable,
                           '--capture-dir', str(out), '--scene', scene,
                           '--frames', '1', '--debug-gpu']
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
            if scene == 'ripples':
                assert difference(baseline,capture()) == 0, 'ripples: drops are not deterministic'
                probes = {mode:capture(**{'ripples-probe':mode, 'start-time':0, 'turbulence':0})
                          for mode in range(1,7)}
                calm = probes[4]
                # A calm view isolates the bottom from wind and drop detail:
                # shallow foreground stones should be clearer than the deep bed.
                bottom_detail = ImageChops.difference(calm,calm.filter(ImageFilter.GaussianBlur(2)))
                near_detail = sum(ImageStat.Stat(bottom_detail.crop((0,288,640,360))).mean)/3
                far_detail = sum(ImageStat.Stat(bottom_detail.crop((0,0,640,72))).mean)/3
                assert near_detail > .4 and near_detail > far_detail*1.5, 'ripples: shallow bottom is missing or deep detail does not fade'
                measured['shallowBottomDetail'] = near_detail
                measured['deepBottomDetail'] = far_detail
                assert difference(probes[5],calm) < .001, 'ripples: opposite phases do not cancel to calm water'
                single = difference(probes[1],calm)
                reinforced = difference(probes[6],calm)
                assert single > .02 and reinforced > single*1.1, 'ripples: coincident crests do not reinforce'
                # A controlled pair overlaps around the centre of the water.
                overlap = (270,105,370,255)
                both = probes[3].crop(overlap)
                for mode in (1,2,4):
                    assert difference(both,probes[mode].crop(overlap)) > .02, 'ripples: crossing region ignores a wave'
                assert difference(both,Image.blend(probes[1],probes[2],.5).crop(overlap)) > .02, 'ripples: crossing is a blend of independent ring images'
                # Exclude every drop so ambient motion cannot pass by moving a
                # ring through the sampled region. Check the whole water view.
                ambient = capture(**{'ripples-probe':4, 'start-time':8})
                ambient_later = capture(**{'ripples-probe':4, 'start-time':10})
                for box in ((0,0,320,180),(320,0,640,180),(0,180,320,360),(320,180,640,360)):
                    assert difference(ambient.crop(box),ambient_later.crop(box)) > .01, 'ripples: a drop-free region is static'
                clouds = capture(**{'ripples-probe':4, 'turbulence':0, 'start-time':8})
                clouds_later = capture(**{'ripples-probe':4, 'turbulence':0, 'start-time':14})
                assert difference(clouds,clouds_later) > .01, 'ripples: cloud illumination is static'
                assert difference(ambient,clouds) > .02, 'ripples: ambient wind ripples are invisible'
                ambient_frozen = capture(**{'ripples-probe':4, 'movement':0, 'start-time':8})
                assert difference(ambient_frozen,capture(**{'ripples-probe':4, 'movement':0, 'start-time':14})) == 0, 'ripples: ambient water or clouds ignore Movement zero'
                # Straddle a stream's event rollover (capture advances by 1/60 s).
                boundary = 2 * 12 / 1.1 - 18 - 1/60
                before = capture(**{'start-time':boundary-.001})
                after = capture(**{'start-time':boundary+.001})
                assert difference(before,after) < .5, 'ripples: event rollover pops'
                for width,height in ((960,360),(360,640)):
                    image = capture(width=width,height=height)
                    assert max(ImageStat.Stat(image).stddev) > 2, 'ripples: blank aspect variant'
            report[scene] = measured
            print(f'PASS {scene}: native parameters, audio isolation/strength/modes, quality and frozen movement',flush=True)
    print(json.dumps(report,indent=2))

if __name__ == '__main__':
    main()
