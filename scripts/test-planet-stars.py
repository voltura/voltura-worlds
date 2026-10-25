"""Visible NASA stars, stationary sky and Moon camera motion across five Worlds."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
from PIL import Image, ImageChops, ImageFilter, ImageStat


def difference(a,b):
    return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)/3


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable',type=Path)
    parser.add_argument('--output',type=Path,default=ROOT/'artifacts/planet-stars/regressions')
    args = parser.parse_args();results = {}
    old = ROOT/'assets/orbit/runtime/milkyway-2020.dds'
    new = ROOT/'assets/space/runtime/planet-sky.dds'
    assert new.stat().st_size == old.stat().st_size,'Star packing increased texture allocation'
    with Image.open(new) as sky:
        alpha = sky.convert('RGBA').getchannel('A')
        assert alpha.getextrema()[1] > 240,'Catalog stars missing from packed alpha'
    for scene in ('earth','mars','moon','jupiter','saturn'):
        def capture(name,**options):
            folder = args.output/scene/name;folder.mkdir(parents=True,exist_ok=True)
            command = [sys.executable,str(ROOT/'scripts/test_desktop.py'),'--',str(args.executable.resolve()),
                       '--capture-dir',str(folder),'--scene',scene,'--debug-gpu']
            for key,value in {'width':1280,'height':720,'frames':1,'flow':0,**options}.items():
                command += ['--'+key.replace('_','-'),str(value)]
            run = subprocess.run(command,capture_output=True,text=True,timeout=180)
            if run.returncode:
                raise RuntimeError(run.stdout+'\n'+run.stderr)
            with Image.open(folder/(scene+'.png')) as image:
                return image.convert('RGB')
        base = capture('baseline')
        later = capture('later',start_time=225)
        black = capture('black',brightness=0)
        assert max(hi for lo,hi in black.getextrema()) <= 1,f'{scene} stars ignore brightness zero'
        frozen = capture('frozen',movement=0)
        frozen_later = capture('frozen-later',movement=0,start_time=450,reaction='expressive',strength=1,energy=1)
        assert difference(frozen,frozen_later) == 0,f'{scene} star/planet composition does not freeze'
        # Keep the strip outside the globes, Moon's Earth and Saturn's rings;
        # isolate actual small stars from the diffuse sky.
        bounds = (0,0,80 if scene == 'saturn' else 210,720)
        background = base.crop(bounds)
        blurred = background.filter(ImageFilter.MedianFilter(7))
        stars = sum(max(pixel) >= 35 and max(pixel)-max(neighbor) >= 12
                    for pixel,neighbor in zip(background.get_flattened_data(),blurred.get_flattened_data()))
        assert stars >= 15,f'{scene} has no visible small background stars: {stars}'
        sky_change = difference(background,later.crop(bounds))
        if scene == 'moon':
            assert sky_change > .1,'Moon stars did not follow its camera orbit'
            end = capture('full-tour',start_time=900)
            assert difference(background,end.crop(bounds)) < .1,'Moon sky does not close after one tour'
        else:
            assert sky_change == 0,f'{scene} star field rotates or twinkles'
        results[scene] = {'visibleStarPixels':stars,'backgroundPhaseDifference255':sky_change}
    results['packedTextureBytes'] = new.stat().st_size
    results['passed'] = True
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('PASS planet stars: visible NASA foreground, stationary Earth/Mars/Jupiter/Saturn, orbiting Moon sky and unchanged texture size')


if __name__ == '__main__':
    main()
