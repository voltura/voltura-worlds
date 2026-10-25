"""Native Earth rendering, geographic lighting, controls and GPU lifecycle checks."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.deps/python'))
from PIL import Image, ImageChops, ImageFilter, ImageStat


def difference(a, b):
    return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean) / 3


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT / 'artifacts/earth/regressions')
    args = parser.parse_args()
    results = {}

    def capture(name, **options):
        folder = args.output / name
        width, height = options.pop('width', 640), options.pop('height', 360)
        command = [sys.executable, str(ROOT / 'scripts/test_desktop.py'), '--',
                   str(args.executable.resolve()), '--capture-dir', str(folder),
                   '--scene', 'earth', '--width', str(width), '--height', str(height), '--debug-gpu']
        for key, value in {'frames': 1, **options}.items():
            command.append('--' + key.replace('_', '-'))
            if value is not True:
                command.append(str(value))
        run = subprocess.run(command, capture_output=True, text=True, timeout=180)
        if run.returncode:
            raise RuntimeError(f'{name}: {run.stdout}\n{run.stderr}')
        if name in ('baseline', 'memory-720p'):
            metrics = json.loads((folder / 'metrics.json').read_text())
            assert metrics['sceneSamples'] == 1, 'Earth allocated redundant multisample targets'
            results[name + 'GpuBytes'] = metrics['estimatedGpuBytes']
        with Image.open(folder / 'earth.png') as image:
            return image.convert('RGB')

    baseline = capture('baseline')
    capture('memory-720p', width=1280, height=720)
    # Only the two display buffers, one HDR buffer and quarter-size bloom
    # buffers should scale with the viewport. Textures remain full resolution.
    viewport_bytes = lambda w, h: w*h*16 + ((w+3)//4)*((h+3)//4)*16
    assert results['memory-720pGpuBytes'] - results['baselineGpuBytes'] == viewport_bytes(1280, 720) - viewport_bytes(640, 360), 'Earth retained unused depth/MSAA allocations'
    assert max(hi for lo, hi in baseline.getextrema()) > 100, 'Blank Earth'
    for parameter in ('detail', 'flow', 'turbulence', 'density'):
        extra = {'start_time': 120} if parameter in ('flow', 'turbulence') else {}
        low = capture(parameter + '-zero', **{parameter: 0}, **extra)
        high = capture(parameter + '-two', **{parameter: 2}, **extra)
        change = difference(low, high)
        assert change > .001, f'Earth {parameter} has no effect'
        results[parameter + 'MeanDifference255'] = change
    black = capture('brightness-zero', brightness=0)
    assert max(hi for lo, hi in black.getextrema()) <= 1, 'Brightness zero leaves light'
    frozen = capture('frozen', movement=0)
    later = capture('frozen-later-audio', movement=0, start_time=450,
                    reaction='expressive', strength=1, energy=1)
    assert difference(frozen, later) == 0, 'Movement zero does not freeze scene/audio'
    audio = capture('audio', reaction='expressive', strength=1, energy=1)
    assert difference(baseline, audio) > .001, 'Active audio has no effect'
    still = capture('rotation-start', flow=0, turbulence=0)
    for phase in (225, 450, 675):
        changed = capture(f'rotation-{phase}', start_time=phase, flow=0, turbulence=0)
        assert difference(still, changed) > 1, f'Rotation phase {phase} unchanged'
        assert difference(still.crop((0, 0, 100, 360)), changed.crop((0, 0, 100, 360))) == 0, 'Stars move with the globe'
    turned = capture('rotation-900', start_time=900, flow=0, turbulence=0)
    assert difference(still, turned) < .1, 'Earth did not complete a 900-second turn'
    results['fullTurnMeanDifference255'] = difference(still, turned)

    # Probe outputs isolate the actual shader terms, through the native HDR path.
    city = capture('city', earth_probe=1, flow=0)
    specular = capture('specular', earth_probe=2, flow=0)
    # Keep the mask below the shared bloom extraction knee, so coastline glow
    # cannot turn land into apparent fractional water in this classification.
    water = capture('water', earth_probe=3, flow=0, brightness=.025)
    # Reflected sunlight legitimately blooms across a shoreline. Require an
    # eight-pixel land interior before checking for an incorrectly lit material.
    coast_guard = water.filter(ImageFilter.MaxFilter(17))
    ground = capture('unlit-night', earth_probe=7, flow=0, density=0)
    # Coordinate tests deliberately stay away from the antialiased limb.
    day_lights, night_ground, land_glints, sea_glints = [], [], [], []
    light_len = math.sqrt(.82**2 + .24**2 + .52**2)
    for y in range(45, 315):
        for x in range(170, 500):
            qx = ((x + .5 - 320) / 360 - .075) / .375
            qy = (-(y + .5 - 180) / 360 - .008) / .375
            r2 = qx*qx + qy*qy
            if r2 >= .85**2:
                continue
            mu = (qx*.82 + qy*.24 + math.sqrt(1-r2)*.52) / light_len
            if mu > .05:
                day_lights.append(max(city.getpixel((x, y))))
            if mu < -.15:
                night_ground.append(max(ground.getpixel((x, y))))
            mask = water.getpixel((x, y))[0]
            if coast_guard.getpixel((x, y))[0] <= 1:
                land_glints.append(max(specular.getpixel((x, y))))
            if mask > 90:
                sea_glints.append(max(specular.getpixel((x, y))))
    assert day_lights and max(day_lights) <= 2, 'City lights visible in daylight'
    assert night_ground and max(night_ground) <= 2, 'Uniform night-side fill'
    assert land_glints and max(land_glints) <= 3, 'Ocean glint appears over land'
    assert sea_glints and max(sea_glints) > 10, 'Water has no reflection'
    assert max(hi for lo, hi in city.getextrema()) > 10, 'No night lights'
    shadow = capture('cloud-shadows', earth_probe=4, flow=0)
    no_shadow = capture('without-cloud-shadows', earth_probe=5, flow=0)
    assert difference(shadow, no_shadow) > .1, 'Clouds cast no surface shadows'
    assert difference(baseline, capture('no-city-lights', earth_probe=6)) > .001
    results.update(dayCityMax255=max(day_lights), nightGroundMax255=max(night_ground),
                   landGlintMax255=max(land_glints), waterGlintMax255=max(sea_glints))

    # Independently project the actual source geography through the specified
    # eastward rotation, tilt and camera attitude. A reversed spin or mirrored
    # longitude fails even though its pictures would still change over time.
    with Image.open(ROOT / 'assets/earth/runtime/earth-color.dds') as source:
        reference = source.convert('RGB').resize((1024, 512), Image.Resampling.LANCZOS)
    for phase in (0, 30, 225):
        globe = capture(f'geography-{phase}', earth_probe=8, flow=0, start_time=phase)
        errors = []
        tilt, pitch = math.radians(-23.44), math.radians(-12)
        spin = -math.radians(-15) - (phase + 1/60) * 2*math.pi/900
        for y in range(105, 255, 12):
            for x in range(245, 450, 12):
                qx = ((x+.5-320)/360-.075)/.375
                qy = (-(y+.5-180)/360-.008)/.375
                if qx*qx+qy*qy > .65**2:
                    continue
                px, py, pz = qx, qy, math.sqrt(1-qx*qx-qy*qy)
                px, py = math.cos(tilt)*px-math.sin(tilt)*py, math.sin(tilt)*px+math.cos(tilt)*py
                py, pz = math.cos(pitch)*py-math.sin(pitch)*pz, math.sin(pitch)*py+math.cos(pitch)*pz
                px, pz = math.cos(spin)*px+math.sin(spin)*pz, -math.sin(spin)*px+math.cos(spin)*pz
                u, v = math.atan2(px,pz)/(2*math.pi)+.5, .5-math.asin(py)/math.pi
                rgb = reference.getpixel((min(int(u*1024),1023), min(int(v*512),511)))
                linear = [c/255/12.92 if c/255 <= .04045 else ((c/255+.055)/1.055)**2.4 for c in rgb]
                exposed = [c*2.4 for c in linear]
                peak = max(max(exposed), .00001)
                expected = [(.62*(1-math.exp(-c))+.38*c*(1-math.exp(-peak))/peak)**(1/2.2)*255 for c in exposed]
                actual = globe.getpixel((x,y))
                errors.extend(abs(a-b) for a,b in zip(actual,expected))
        error = sum(errors)/len(errors)
        assert error < 12, f'Incorrect map alignment or eastward rotation at {phase}: {error}'
        results[f'geography{phase}MeanError255'] = error
    capture('switch-resize', validate_lifecycle=True, frames=18)
    capture('single-sample-switch-resize', validate_lifecycle=True, single_sample=True, frames=18)
    results['lifecycleGpuValidation'] = True
    results['passed'] = True
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    print('PASS Earth: controls, rotation, frozen audio, geographic lighting, cloud shadows and GPU lifecycle')


if __name__ == '__main__':
    main()
