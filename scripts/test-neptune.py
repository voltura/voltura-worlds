"""Neptune cloud-cover rendering, controls, framing, freeze and resource lifecycle."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.deps/python'))
from PIL import Image, ImageChops, ImageStat


def difference(a, b):
    return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/neptune/regressions')
    args = parser.parse_args()
    results = {}

    def capture(name, **options):
        folder = args.output/name
        command = [sys.executable, str(ROOT/'scripts/test_desktop.py'), '--',
                   str(args.executable.resolve()), '--capture-dir', str(folder),
                   '--scene', 'neptune', '--debug-gpu']
        for key, value in {'width':640, 'height':360, 'frames':1, **options}.items():
            command += ['--'+key.replace('_','-')]
            if value is not True:
                command += [str(value)]
        run = subprocess.run(command, capture_output=True, text=True, timeout=240)
        if run.returncode:
            raise RuntimeError(name+': '+run.stdout+'\n'+run.stderr)
        results[name] = json.loads((folder/'metrics.json').read_text())
        assert results[name]['sceneSamples'] == 1, 'Redundant multisample targets'
        with Image.open(folder/'neptune.png') as image:
            return image.convert('RGB')

    base = capture('baseline')
    assert max(hi for lo,hi in base.getextrema()) > 100, 'Blank or underexposed cloud deck'
    # Cinematic Neptune is blue; the opaque planet hides stars.
    center = base.getpixel((340,180))
    assert center[2] > center[1] > center[0], 'Cinematic Neptune is not blue'
    # Broad image regions detect the visible vortex and bright methane streaks
    # independently of the procedural field's implementation.
    storm_blue = ImageStat.Stat(base.crop((324,223,336,235))).mean[2]
    surrounding_blue = ImageStat.Stat(base.crop((342,223,354,235))).mean[2]
    assert storm_blue < surrounding_blue*.85, 'Dark storm is missing'
    white_clouds = sum(r>100 and g>130 and b>150 and r/b>.6
                       for r,g,b in base.crop((270,90,400,200)).get_flattened_data())
    assert white_clouds > 50, 'Bright methane cloud streaks are missing'
    black = capture('black', brightness=0)
    assert max(hi for lo,hi in black.getextrema()) <= 1, 'Brightness zero leaves light'
    frozen = capture('frozen', movement=0)
    frozen_later = capture('frozen-later', movement=0, start_time=450,
                           reaction='expressive', strength=1, energy=1)
    assert difference(frozen, frozen_later) == 0, 'Movement zero does not freeze'
    later = capture('later', start_time=225, flow=0)
    early = capture('early', flow=0)
    assert difference(early,later) > .05, 'Cloud atmosphere does not move'
    assert difference(early.crop((0,0,100,360)),later.crop((0,0,100,360))) == 0, 'Star field moves'
    for parameter in ('detail','flow','turbulence','density','palette'):
        low = capture(parameter+'-low', **{parameter:0}, start_time=225)
        high = capture(parameter+'-high', **{parameter:2}, start_time=225)
        change = difference(low,high)
        assert change > .001, parameter+' does not change Neptune'
        results[parameter+'Difference255'] = change
    for name,width,height in [('portrait',360,640),('ultrawide',1280,360)]:
        image = capture(name,width=width,height=height)
        # Blue globe pixels must stay clear of all viewport edges.
        globe = [(x,y) for y in range(height) for x in range(width)
                if (lambda c:c[2]>75 and c[2]>c[0]*1.4)(image.getpixel((x,y)))]
        assert globe and min(x for x,y in globe)>8 and max(x for x,y in globe)<width-8, 'Horizontal clipping'
        assert min(y for x,y in globe)>8 and max(y for x,y in globe)<height-8, 'Vertical clipping'
    audio = capture('audio', reaction='expressive', strength=1, energy=1)
    assert difference(base,audio) > .001, 'Audio has no effect'
    for palette in range(4):
        capture('palette-'+str(palette), palette=palette)
    no_haze = capture('opaque', density=0)
    assert no_haze.getpixel((340,180))[2] > 40, 'Atmosphere zero removes the planet'
    capture('lifecycle',validate_lifecycle=True)
    # Native lifecycle validation checks stable allocations over repeated planet
    # switches and resizes. Aurora's shared cached artwork can remain resident.
    results['lifecycleGpuValidation'] = True
    results['passed'] = True
    args.output.mkdir(parents=True,exist_ok=True)
    (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('PASS Neptune: cloud deck, controls, stationary stars, freeze, framing and GPU lifecycle')


if __name__ == '__main__':
    main()
