"""Native lunar geography, camera/lighting geometry, controls and GPU lifecycle."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageChops, ImageStat

ROOT = Path(__file__).resolve().parents[1]


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def unit(a):
    length = math.sqrt(dot(a, a))
    return tuple(v/length for v in a)


def ray_at(x, y, phase, width=640, height=360):
    # Independent pinhole projection, camera at three lunar radii; no drift.
    angle = (phase + 1/60)*2*math.pi/900
    outward = (math.sin(angle), 0, math.cos(angle))
    right = (math.cos(angle), 0, -math.sin(angle))
    qx = ((x+.5-width/2)/min(width, height)-.075)/(.375*math.sqrt(8))
    qy = (-(y+.5-height/2)/min(width, height)-.008)/(.375*math.sqrt(8))
    return tuple(3*v for v in outward), unit(tuple(-outward[i]+right[i]*qx+(qy if i == 1 else 0) for i in range(3)))


def hit(origin, ray, center=(0, 0, 0), radius=1):
    offset = tuple(center[i]-origin[i] for i in range(3))
    along = dot(offset, ray)
    perpendicular = tuple(offset[i]-along*ray[i] for i in range(3))
    discriminant = radius*radius-dot(perpendicular, perpendicular)
    if discriminant <= 0 or along <= 0:
        return None
    distance = along-math.sqrt(discriminant)
    return distance, tuple((origin[i]+distance*ray[i]-center[i])/radius for i in range(3))


def display(rgb, exposure=.64):
    linear = [(c/255/12.92 if c/255 <= .04045 else ((c/255+.055)/1.055)**2.4)*exposure for c in rgb]
    peak = max(max(linear), .00001)
    return [255*(.62*(1-math.exp(-c))+.38*c*(1-math.exp(-peak))/peak)**(1/2.2) for c in linear]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/moon/regressions')
    args = parser.parse_args()
    results = {}

    def capture(name, **options):
        folder = args.output/name
        folder.mkdir(parents=True, exist_ok=True)
        command = [sys.executable, str(ROOT/'scripts/test_desktop.py'), '--', str(args.executable.resolve()),
                   '--capture-dir', str(folder), '--scene', 'moon', '--debug-gpu']
        for key, value in {'width': 640, 'height': 360, 'frames': 1, **options}.items():
            command.append('--'+key.replace('_', '-'))
            if value is not True:
                command.append(str(value))
        run = subprocess.run(command, capture_output=True, text=True, timeout=240)
        if run.returncode:
            raise RuntimeError(f'{name}: {run.stdout}\n{run.stderr}')
        with Image.open(folder/'moon.png') as image:
            return image.convert('RGB')

    def difference(a, b):
        return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean)/3

    baseline = capture('baseline')
    manifest = json.loads((ROOT/'assets/moon/sources.json').read_text())
    with Image.open(ROOT/'assets/moon/runtime/moon-color.dds') as source:
        for name, point in manifest['landmarks'].items():
            actual = source.getpixel(tuple(point['sourcePixel']))[:3]
            expected = [v/257 for v in point['sourceRgb16']]
            assert max(abs(a-b) for a,b in zip(actual, expected)) < 9, f'Converted landmark mismatch: {name}'
    with Image.open(ROOT/'assets/moon/runtime/moon-normal.dds') as normals:
        for y in (0, 4095):
            for x in range(0, 8192, 128):
                r,g,b = normals.getpixel((x,y))[:3]
                assert abs(r-128) <= 5 and abs(g-128) <= 5 and b >= 247, 'Unstable polar slope after BC7 compression'
    metrics = json.loads((args.output/'baseline/metrics.json').read_text())
    assert metrics['sceneSamples'] == 1, 'Redundant Moon MSAA'
    black = capture('black', brightness=0)
    assert max(hi for lo, hi in black.getextrema()) <= 1, 'Zero brightness leaves light'
    frozen = capture('frozen', movement=0)
    later = capture('frozen-audio', movement=0, start_time=450, reaction='expressive', strength=1, energy=1)
    assert difference(frozen, later) == 0, 'Movement zero fails to freeze camera/audio'
    audio = capture('audio', reaction='expressive', strength=1, energy=1)
    assert difference(baseline, audio) > .01, 'Audio has no effect'
    for parameter, phase in (('detail', 0), ('flow', 225), ('turbulence', 0), ('density', 390)):
        a = capture(parameter+'-zero', start_time=phase, **{parameter: 0})
        b = capture(parameter+'-two', start_time=phase, **{parameter: 2})
        change = difference(a, b)
        assert change > .001, f'{parameter} control has no effect'
        results[parameter+'MeanDifference255'] = change
    for palette in (1, 2, 3):
        assert difference(baseline, capture(f'palette-{palette}', palette=palette)) > .01
    for quality in ('eco', 'high'):
        assert difference(baseline, capture('quality-'+quality, quality=quality)) == 0, 'Quality changed fixed lunar map detail'
    start = capture('tour-start', flow=0)
    for phase in (225, 450, 675):
        frame = capture(f'tour-{phase}', flow=0, start_time=phase)
        assert difference(start, frame) > 1, 'Camera did not reveal different terrain/phase'
        assert difference(start.crop((0, 0, 100, 360)), frame.crop((0, 0, 100, 360))) > .01, 'Sky does not follow orbiting camera'
    end = capture('tour-end', flow=0, start_time=900)
    results['fullTourMeanDifference255'] = difference(start, end)
    assert results['fullTourMeanDifference255'] < .1, 'Tour does not close after 900 seconds'

    # Source-based independent geographic projection: central near side,
    # longitude seam on far side, both intermediate longitudes and high latitudes.
    with Image.open(ROOT/'assets/moon/runtime/moon-color.dds') as source:
        reference = source.convert('RGB').resize((2048, 1024), Image.Resampling.LANCZOS)
    light = (math.sin(math.pi/3), 0, math.cos(math.pi/3))
    for phase in (0, 225, 450, 675):
        globe = capture(f'geography-{phase}', start_time=phase, moon_probe=1, flow=0, brightness=.2)
        # Stay below the shared bloom knee so camera glare is not mistaken
        # for direct illumination on the unlit surface.
        normal = capture(f'lighting-{phase}', start_time=phase, flow=0, brightness=.1)
        errors, night = [], []
        for y in range(55, 305, 6):
            for x in range(215, 480, 6):
                origin, ray = ray_at(x, y, phase)
                surface = hit(origin, ray)
                if surface is None or dot(surface[1], tuple(-v for v in ray)) < .25:
                    continue
                n = surface[1]
                u = math.atan2(n[0], n[2])/(2*math.pi)+.5
                v = .5-math.asin(n[1])/math.pi
                rgb = reference.getpixel((int(u*2048)%2048, min(int(v*1024), 1023)))
                errors.extend(abs(a-b) for a, b in zip(globe.getpixel((x, y)), display(rgb)))
                if dot(n, light) < -.25:
                    night.append(max(normal.getpixel((x, y))))
        error = sum(errors)/len(errors)
        assert error < 9, f'Map is mirrored/misaligned at {phase}: {error}'
        assert not night or max(night) <= 1, f'Unexpected night illumination at {phase}: {max(night)}'
        results[f'geography{phase}MeanError255'] = error
        results[f'night{phase}Max255'] = max(night, default=0)

    # Native object masks verify the actual physical Earth size and occlusion.
    with Image.open(ROOT/'assets/moon/runtime/moon-earth.dds') as source:
        # Alpha is an independent emission channel, not transparency.
        # Pillow's RGBA resize premultiplies RGB; filter the channels separately.
        earth_reference = Image.merge('RGBA', tuple(channel.resize((256, 128), Image.Resampling.LANCZOS) for channel in source.convert('RGBA').split()))
    for phase in (0, 390, 450, 510):
        mask = capture(f'geometry-{phase}', start_time=phase, moon_probe=3, flow=0)
        earth_frame = capture(f'earth-lighting-{phase}', start_time=phase, flow=0) if phase in (390, 510) else None
        earth_errors = []
        predicted, actual = 0, 0
        for y in range(360):
            for x in range(640):
                origin, ray = ray_at(x, y, phase)
                moon = hit(origin, ray)
                earth = hit(origin, ray, (0, 0, 384400/1737.4), 6371/1737.4)
                if earth and (not moon or earth[0] < moon[0]):
                    predicted += 1
                    en = earth[1]
                    nv = -dot(en, ray)
                    if earth_frame and nv > .6:
                        body = (en[0], .91748*en[1]+.39778*en[2], -.39778*en[1]+.91748*en[2])
                        u = math.atan2(body[0], body[2])/(2*math.pi)+.5
                        v = .5-math.asin(max(-1, min(1, body[1])))/math.pi
                        rgba = earth_reference.getpixel((int(u*256)%256, min(int(v*128), 127)))
                        mu = dot(en, light)
                        linear = [c/255/12.92 if c/255 <= .04045 else ((c/255+.055)/1.055)**2.4 for c in rgba[:3]]
                        day_gate = max(0, min(1, (mu+.12)/.14))
                        city = (rgba[3]/255)**1.35*(.42/math.pi)*(1-day_gate*day_gate*(3-2*day_gate))
                        sun = max(mu, 0)
                        radiance = [linear[i]*sun/math.pi+city*(1,.63,.28)[i]+(1-nv)**4*sun*(.03,.10,.24)[i] for i in range(3)]
                        peak = max(max(radiance)*3.2, .00001)
                        expected = [255*(.62*(1-math.exp(-c*3.2))+.38*c*3.2*(1-math.exp(-peak))/peak)**(1/2.2) for c in radiance]
                        earth_errors.extend(abs(a-b) for a,b in zip(earth_frame.getpixel((x,y)), expected))
                r, g, b = mask.getpixel((x, y))
                if g > 50 and r < 10 and b < 10:
                    actual += 1
        assert abs(actual-predicted) <= max(12, predicted*.2), f'Earth size/occlusion mismatch at {phase}: {actual}/{predicted}'
        assert (predicted > 0) == (phase in (390, 510)), 'Earth geometry did not cover visible/hidden cases'
        results[f'earth{phase}Pixels'] = {'predicted': predicted, 'actual': actual}
        if earth_errors:
            error = sum(earth_errors)/len(earth_errors)
            assert error < 15, f'Earth does not share lunar Sun direction at {phase}: {error}'
            results[f'earth{phase}LightingMeanError255'] = error

    capture('lifecycle', validate_lifecycle=True, frames=18)
    capture('fallback-lifecycle', validate_lifecycle=True, single_sample=True, frames=18)
    small = capture('allocation-small', width=640, height=360)
    capture('allocation-large', width=1280, height=720)
    low = json.loads((args.output/'allocation-small/metrics.json').read_text())
    high = json.loads((args.output/'allocation-large/metrics.json').read_text())
    expected_growth = (1280*720-640*360)*17  # display/HDR 16 + quarter-size bloom pair 1
    assert abs(high['estimatedGpuBytes']-low['estimatedGpuBytes']-expected_growth) < 200000, 'Unexpected depth/MSAA allocation'
    assert small.getbbox()
    results['passed'] = True
    (args.output/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    print('PASS Moon: geography, phases, natural Earth size/occlusion, controls, frozen audio and GPU lifecycle')


if __name__ == '__main__':
    main()
