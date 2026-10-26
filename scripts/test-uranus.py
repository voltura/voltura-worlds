"""Native Uranus controls, fixed geometry/light/sky, tilted spin and lifecycle."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'.deps/python'))
from PIL import Image, ImageChops, ImageStat


def difference(a, b):
    return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean)/3


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/uranus/regressions')
    args = parser.parse_args()
    results = {}

    def capture(name, **options):
        folder = args.output/name
        command = [sys.executable, str(ROOT/'scripts/test_desktop.py'), '--',
                   str(args.executable.resolve()), '--capture-dir', str(folder),
                   '--scene', 'uranus', '--debug-gpu']
        for key, value in {'width':640, 'height':360, 'frames':1, **options}.items():
            command += ['--'+key.replace('_','-')]
            if value is not True:
                command += [str(value)]
        run = subprocess.run(command, capture_output=True, text=True, timeout=240)
        if run.returncode:
            raise RuntimeError(name+': '+run.stdout+'\n'+run.stderr)
        results[name] = json.loads((folder/'metrics.json').read_text())
        assert results[name]['sceneSamples'] == 1, 'Redundant multisample allocation'
        with Image.open(folder/'uranus.png') as image:
            return image.convert('RGB')

    base = capture('baseline')
    center = base.getpixel((320,180))
    assert center[2] >= center[1] > center[0]+15, 'Natural Uranus is not pale cyan'
    assert center[1] > 120, 'Underexposed Uranus'
    black = capture('black', brightness=0)
    assert max(hi for lo, hi in black.getextrema()) <= 1, 'Brightness zero leaves radiance'
    frozen = capture('frozen', movement=0)
    frozen_later = capture('frozen-later', movement=0, start_time=450,
                           reaction='expressive', strength=1, energy=1)
    assert difference(frozen,frozen_later) == 0, 'Movement zero does not freeze audio and animation'
    later = capture('later', start_time=225)
    assert difference(base,later) > .005, 'No visible rotation'
    sky = (0,0,100,360)
    assert difference(base.crop(sky),later.crop(sky)) == 0, 'Stars move'
    for parameter in ('detail','flow','turbulence','density'):
        low = capture(parameter+'-low', **{parameter:0}, start_time=225)
        high = capture(parameter+'-high', **{parameter:2}, start_time=225)
        change = difference(low,high)
        assert change > .001, parameter+' has no visible effect'
        results[parameter+'Difference255'] = change
    for palette in (1,2,3):
        other = capture('palette-'+str(palette), palette=palette)
        assert difference(base,other) > .01, 'Palette has no effect'
        assert difference(base.crop(sky),other.crop(sky)) == 0, 'Palette changes stars'

    off = capture('audio-off', reaction='off', strength=1, energy=1)
    zero = capture('audio-zero', reaction='expressive', strength=0, energy=1)
    gentle = capture('audio-gentle', reaction='gentle', strength=1, energy=1)
    expressive = capture('audio-expressive', reaction='expressive', strength=1, energy=1)
    assert difference(base,off) == difference(base,zero) == 0, 'Disabled audio affects image'
    assert 0 < difference(base,gentle) < difference(base,expressive), 'Audio modes do not scale'
    assert difference(base.crop(sky),expressive.crop(sky)) == 0, 'Audio affects stars'

    mask = capture('mask', uranus_probe=2)
    lighting = capture('lighting', uranus_probe=3)
    for probe, reference in ((2,mask),(3,lighting)):
        changed = capture('fixed-'+str(probe), uranus_probe=probe, start_time=225,
                          flow=2, turbulence=2, reaction='expressive', energy=1, strength=1)
        assert difference(reference,changed) == 0, 'Geometry or sunlight changes with time/controls/audio'

    # Independently recover the projected oblate axis from image moments.
    coords = [(x+.5-320,180-y-.5) for y in range(360) for x in range(640)
              if mask.getpixel((x,y))[0] > 128]
    xx = sum(x*x for x,y in coords)/len(coords)
    yy = sum(y*y for x,y in coords)/len(coords)
    xy = sum(x*y for x,y in coords)/len(coords)
    major = .5*math.atan2(2*xy,xx-yy)
    minor = math.degrees(major)+90
    expected = 90+97.8
    error = abs((minor-expected+90)%180-90)
    assert error < 2, f'Projected rotation axis has wrong tilt: {error}'
    trace, delta = xx+yy, math.hypot(xx-yy,2*xy)
    ratio = math.sqrt((trace-delta)/(trace+delta))
    assert abs(ratio-24973/25559) < .004, 'Wrong oblateness'
    # A quarter-turn feature changes while a full body turn closes with wind off.
    albedo = capture('albedo-start', uranus_probe=1, turbulence=0)
    quarter = capture('albedo-quarter', uranus_probe=1, turbulence=0, start_time=225)
    full = capture('albedo-full', uranus_probe=1, turbulence=0, start_time=900)
    assert difference(albedo,quarter) > .01, 'Cloud features do not rotate'
    assert difference(albedo,full) < .01, 'Body spin does not close after 900 seconds'
    before = capture('wrap-before', start_time=899.98)
    after = capture('wrap-after', start_time=900.02)
    assert difference(before,after) < .03, 'Rotation boundary jumps'

    for name,w,h in [('landscape',640,360),('portrait',360,640),('ultrawide',1280,360)]:
        image = mask if name=='landscape' else capture(name, width=w, height=h, uranus_probe=2)
        points = [(x,y) for y in range(h) for x in range(w) if image.getpixel((x,y))[0]>128]
        x0,x1 = min(x for x,y in points),max(x for x,y in points)
        y0,y1 = min(y for x,y in points),max(y for x,y in points)
        assert abs((x0+x1+1)/2-w/2)<1 and abs((y0+y1+1)/2-h/2)<1, 'Planet is not centered'
        assert .72 < max(x1-x0+1,y1-y0+1)/min(w,h) < .77, 'Wrong framing scale'
        assert x0>8 and x1<w-8 and y0>8 and y1<h-8, 'Clipped planet'
    capture('lifecycle', validate_lifecycle=True)
    results['projectedAxisErrorDegrees'] = error
    results['polarToEquatorialRatio'] = ratio
    results['passed'] = True
    (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('PASS Uranus: cyan clouds, tilted rotation, fixed light/camera/stars, controls, audio, framing and GPU lifecycle')


if __name__ == '__main__':
    main()
