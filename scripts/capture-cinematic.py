"""Capture comparable native Worlds evidence without changing published site media.

Run before and after a renderer change with the same --output directory.
GPU timings come from separate captures, never from video encoding.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.deps/python'))
import imageio_ffmpeg
from PIL import Image, ImageChops, ImageStat

SCENES = ('aurora', 'orbit', 'mars', 'smoke', 'bloom', 'silk', 'eclipse',
          'lattice', 'drift', 'contour', 'plastic_metal', 'sun')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=('before', 'after'))
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/cinematic')
    parser.add_argument('--scenes', nargs='+', choices=SCENES, default=SCENES)
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--frames', type=int, default=360)
    parser.add_argument('--skip-video', action='store_true')
    parser.add_argument('--video-only', action='store_true')
    args = parser.parse_args()
    if args.repeats < 1 or args.frames < 12:
        parser.error('Use at least one repeat and twelve frames for timing samples')
    if args.skip_video and args.video_only:
        parser.error('--skip-video and --video-only cannot be combined')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    executable = ROOT/'build/Release/VolturaWorlds.exe'
    shaders = ROOT/'build/Release/shaders'
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in shaders.glob('*.cso')}
    (output/f'{args.phase}-shader-hashes.json').write_text(json.dumps(hashes, indent=2))
    if args.phase == 'before':
        snapshot = output/'baseline-shaders'
        snapshot.mkdir(exist_ok=True)
        for shader in shaders.glob('*.cso'):
            shutil.copy2(shader, snapshot/shader.name)
    metrics_path = output/f'{args.phase}-metrics.json'
    report = json.loads(metrics_path.read_text()) if metrics_path.exists() else {}
    if args.video_only and any(scene not in report for scene in args.scenes):
        parser.error('--video-only requires existing timing captures for each selected World')
    for scene in args.scenes:
        if not args.video_only:
            report[scene] = {}
        for quality, grid in (() if args.video_only else (('eco', 64), ('auto', 80), ('high', 96))):
            samples = []
            for repeat in range(args.repeats):
                folder = output/args.phase/scene/quality/str(repeat)
                folder.mkdir(parents=True, exist_ok=True)
                command = [sys.executable, str(ROOT/'scripts/run-private-desktop.py'), '--',
                           str(executable), '--capture-dir', str(folder), '--scene', scene,
                           '--width', '1920', '--height', '1080', '--frames', str(args.frames),
                           '--grid', str(grid), '--quality', quality, '--debug-gpu']
                subprocess.run(command, check=True, timeout=600, capture_output=True)
                samples.append(json.loads((folder/'metrics.json').read_text()))
            report[scene][quality] = {
                'gpuP95Ms': statistics.median(s['gpuP95Ms'] for s in samples),
                'estimatedGpuBytes': max(s['estimatedGpuBytes'] for s in samples),
                'runs': samples,
            }
            print(f'{args.phase} {scene} {quality}: '
                  f'{report[scene][quality]["gpuP95Ms"]:.3f} ms p95', flush=True)
        if not args.skip_video:
            folder = output/args.phase/scene/'motion'
            folder.mkdir(parents=True, exist_ok=True)
            subprocess.run([sys.executable, str(ROOT/'scripts/run-private-desktop.py'), '--',
                            str(executable), '--capture-dir', str(folder), '--scene', scene,
                            '--width', '1920', '--height', '1080', '--frames', str(args.frames),
                            '--grid', '80', '--quality', 'auto', '--record', '360', '--debug-gpu'],
                           check=True, timeout=600, capture_output=True)
            subprocess.run([sys.executable, str(ROOT/'scripts/analyze-motion.py'), str(folder)],
                           check=True, capture_output=True)
            subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(), '-hide_banner', '-loglevel', 'error',
                            '-y', '-framerate', '30', '-i', str(folder/'frame-%05d.png'),
                            '-c:v', 'libx264', '-crf', '19', '-preset', 'fast', '-pix_fmt', 'yuv420p',
                            '-an', '-movflags', '+faststart', str(folder/f'{scene}.mp4')],
                           check=True, timeout=180)
            # Keep native samples for temporal inspection, reclaim only this run's frames.
            for frame in folder.glob('frame-*.png'):
                if int(frame.stem.split('-')[1]) not in (0, 30, 90, 180, 270, 359):
                    frame.unlink()
            print(f'{args.phase} {scene}: 12-second native clip complete', flush=True)
        metrics_path.write_text(json.dumps(report, indent=2))

    if args.phase == 'after' and (output/'before-metrics.json').exists():
        before = json.loads((output/'before-metrics.json').read_text())
        comparison_path = output/'comparison.json'
        comparisons = json.loads(comparison_path.read_text()) if comparison_path.exists() else {}
        for scene in args.scenes:
            comparisons[scene] = {}
            for quality in ('eco', 'auto', 'high'):
                old, new = before[scene][quality], report[scene][quality]
                change = new['gpuP95Ms']/old['gpuP95Ms']-1
                comparisons[scene][quality] = {
                    'gpuP95ChangePercent': change*100,
                    'gpuBudgetPassed': change <= .05,
                    'allocationUnchanged': new['estimatedGpuBytes'] == old['estimatedGpuBytes'],
                }
            old_path = output/'before'/scene/'auto/0'/f'{scene}.png'
            new_path = output/'after'/scene/'auto/0'/f'{scene}.png'
            with Image.open(old_path) as old_image, Image.open(new_path) as new_image:
                sheet = Image.new('RGB', (1920, 1080))
                sheet.paste(old_image.resize((960, 540)), (0, 270))
                sheet.paste(new_image.resize((960, 540)), (960, 270))
                sheet.save(output/f'{scene}-comparison.jpg', quality=94)
                difference = ImageChops.difference(old_image.convert('RGB'), new_image.convert('RGB'))
                comparisons[scene]['meanPixelChange255'] = sum(ImageStat.Stat(difference).mean)/3
        comparison_path.write_text(json.dumps(comparisons, indent=2))
        print(json.dumps(comparisons, indent=2), flush=True)


if __name__ == '__main__':
    main()
