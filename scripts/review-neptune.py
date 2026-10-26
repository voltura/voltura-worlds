"""Capture Neptune motion and compare offscreen GPU cost with Venus and Jupiter."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'.deps/python'))
from PIL import Image, ImageChops, ImageStat


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/neptune/review')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    def capture(scene, name, **options):
        folder = args.output/name
        command = [sys.executable, str(ROOT/'scripts/run-private-desktop.py'), '--',
                   str(args.executable.resolve()), '--scene', scene,
                   '--capture-dir', str(folder), '--debug-gpu']
        for key, value in options.items():
            command += ['--'+key.replace('_','-'), str(value)]
        subprocess.run(command, check=True, timeout=240, capture_output=True, text=True)
        return folder

    frames = []
    for index in range(12):
        folder = capture('neptune', 'motion-'+str(index), width=1280, height=720,
                         frames=1, start_time=index*2, flow=0)
        with Image.open(folder/'neptune.png') as source:
            frames.append(source.convert('RGB').copy())
    differences = [sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)/3
                   for a,b in zip(frames,frames[1:])]
    assert all(0 < delta < 4 for delta in differences), 'Motion jumps or does not advance'
    frames[0].save(args.output/'motion.gif', save_all=True,
                   append_images=frames[1:], duration=150, loop=0)
    sheet = Image.new('RGB', (1280, 1080))
    for index, frame in enumerate(frames[::2]):
        sheet.paste(frame.resize((640,360)), ((index%2)*640,(index//2)*360))
    sheet.save(args.output/'motion-sheet.jpg', quality=94)
    results = {'motionDifference255': differences, 'scenes': {}}
    # Sequential runs avoid measuring these scenes concurrently with each other.
    for scene in ('venus','jupiter','neptune'):
        folder = capture(scene, 'timing-'+scene, width=1920, height=1080,
                         frames=180, timing_warmup=45, quality='auto')
        results['scenes'][scene] = json.loads((folder/'metrics.json').read_text())
    results['evidence'] = 'Native offscreen GPU timing; not physical desktop presentation performance'
    (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print(json.dumps({scene: {key: data[key] for key in ('gpuMedianMs','gpuP95Ms')}
                      for scene,data in results['scenes'].items()}, indent=2))


if __name__ == '__main__':
    main()
