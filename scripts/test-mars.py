"""Native Mars controls, rotation, resource lifecycle and framing regressions."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
from PIL import Image, ImageChops, ImageStat

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/mars/regressions')
    args = parser.parse_args()
    executable = args.executable.resolve()
    results = {}

    def capture(name, **options):
        folder = args.output/name
        folder.mkdir(parents=True, exist_ok=True)
        command = [sys.executable, str(ROOT/'scripts/test_desktop.py'), '--',
                   str(executable), '--capture-dir', str(folder), '--scene', 'mars',
                   '--width', '640', '--height', '360', '--debug-gpu']
        for key, value in {'frames': 1, **options}.items():
            command.append('--'+key.replace('_', '-'))
            if value is not True:
                command.append(str(value))
        run = subprocess.run(command, capture_output=True, text=True, timeout=180)
        if run.returncode:
            raise RuntimeError(f'{name}: {run.stdout}\n{run.stderr}')
        with Image.open(folder/'mars.png') as image:
            return image.convert('RGB')

    baseline = capture('baseline')
    metrics = json.loads((args.output/'baseline/metrics.json').read_text())
    assert metrics['sceneSamples'] == 1, 'Mars allocated redundant multisample targets'
    # Deep night must not receive a uniform ambient fill or a broad haze mask.
    # Classify surface pixels by solar incidence, away from the antialiased limb.
    dark_checks = {}
    for atmosphere in (0, 1, 2):
        globe = capture(f'night-atmosphere-{atmosphere}', density=atmosphere, flow=0)
        night = []
        for y in range(60, 300, 2):
            for x in range(180, 470, 2):
                qx = ((x+.5-320)/360-.075)/.375
                qy = (-(y+.5-180)/360-.008)/.375
                radius2 = qx*qx+qy*qy
                if radius2 >= .85**2:
                    continue
                incidence = (qx*.76+qy*.30+math.sqrt(1-radius2)*.58)/math.sqrt(.76**2+.30**2+.58**2)
                if incidence < -.25:
                    night.append(max(globe.getpixel((x, y))))
        assert night and max(night) <= 3, f'Atmosphere {atmosphere} illuminated deep night'
        dark_checks[str(atmosphere)] = max(night)
    results['deepNightMax255'] = dark_checks
    # Normal-mapped ridges must fade before the shared solar horizon; they
    # must not leave a bright contour that switches off with the solar disc.
    smooth_ground = capture('horizon-smooth', detail=0, density=0, flow=0)
    rough_ground = capture('horizon-relief', detail=2, density=0, flow=0)
    horizon_changes = []
    for y in range(60, 300):
        for x in range(180, 470):
            qx = ((x+.5-320)/360-.075)/.375
            qy = (-(y+.5-180)/360-.008)/.375
            radius2 = qx*qx+qy*qy
            if radius2 >= .85**2:
                continue
            incidence = (qx*.76+qy*.30+math.sqrt(1-radius2)*.58)/math.sqrt(.76**2+.30**2+.58**2)
            if 0 <= incidence < .006:
                a, b = smooth_ground.getpixel((x, y)), rough_ground.getpixel((x, y))
                horizon_changes.append(max(abs(a[c]-b[c]) for c in range(3)))
    assert horizon_changes and max(horizon_changes) <= 8, 'Terrain retained a bright solar-horizon contour'
    results['horizonReliefMaxDifference255'] = max(horizon_changes)
    for parameter in ('detail', 'flow', 'turbulence', 'density'):
        # Drift needs elapsed time to become visible; other controls can be
        # checked at the initial pose, including their independence from motion.
        extra = {'start_time': 120} if parameter == 'flow' else {}
        low = capture(parameter+'-zero', **{parameter: 0}, **extra)
        high = capture(parameter+'-two', **{parameter: 2}, **extra)
        difference = ImageChops.difference(low, high)
        assert difference.getbbox(), f'Mars {parameter} has no effect'
        results[parameter+'MeanDifference255'] = sum(ImageStat.Stat(difference).mean)/3
    still = capture('rotation-start', flow=0, turbulence=0)
    for phase in (225, 450, 675):
        turned = capture(f'rotation-{phase}', start_time=phase, flow=0, turbulence=0)
        change = sum(ImageStat.Stat(ImageChops.difference(still, turned)).mean)/3
        assert change > 1, f'Mars phase {phase} did not reveal different terrain'
        results[f'rotation{phase}MeanDifference255'] = change
    full_turn = capture('rotation-900', start_time=900, flow=0, turbulence=0)
    difference = sum(ImageStat.Stat(ImageChops.difference(still, full_turn)).mean)/3
    assert difference < .1, f'Mars did not complete one turn in 900 seconds: {difference}'
    results['fullTurnMeanDifference255'] = difference
    lifecycle = capture('switch-resize', validate_lifecycle=True, frames=18)
    results['lifecycleGpuValidation'] = True
    assert max(hi for lo, hi in baseline.getextrema()) > 50, 'Mars surface is blank'
    results['passed'] = True
    (args.output/'results.json').write_text(json.dumps(results, indent=2)+'\n')
    print('PASS Mars: independent settings, 900-second rotation, scene-switch/resize GPU validation')


if __name__ == '__main__':
    main()
