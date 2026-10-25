"""Alternate original/candidate shader captures to check small GPU regressions.

Run with Worlds closed and no other rendering tests in progress. The original
shader snapshot must exist; the candidate is restored even on capture failure.
"""
import argparse
import json
from pathlib import Path
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenes', nargs='+', required=True,
                        choices=('aurora', 'orbit', 'smoke', 'bloom', 'silk', 'eclipse', 'lattice', 'drift', 'contour', 'mercury'))
    parser.add_argument('--qualities', nargs='+', choices=('eco', 'auto', 'high'), default=('eco', 'auto', 'high'))
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/cinematic')
    parser.add_argument('--frames', type=int, default=1200)
    args = parser.parse_args()
    if args.frames < 48:
        parser.error('Use at least 48 frames for warmup and timing samples')
    output = args.output.resolve()
    live = ROOT/'build/Release/shaders'
    original = output/'baseline-shaders'
    assert original.is_dir() and list(original.glob('*.cso')), 'Missing original shader snapshot'
    candidate = {p.name: p.read_bytes() for p in live.glob('*.cso')}
    assert set(candidate) == {p.name for p in original.glob('*.cso')}, 'Shader sets differ'
    report_path = output/'paired-performance.json'
    report = json.loads(report_path.read_text()) if report_path.exists() else {}
    try:
        for scene in args.scenes:
            report.setdefault(scene, {})
            for quality in args.qualities:
                results = {'before': [], 'after': []}
                for repeat in range(3):
                    for phase in (('before', 'after') if repeat % 2 == 0 else ('after', 'before')):
                        for name, data in candidate.items():
                            (live/name).write_bytes((original/name).read_bytes() if phase == 'before' else data)
                        folder = output/'paired'/scene/quality/f'{repeat}-{phase}'
                        subprocess.run([sys.executable, str(ROOT/'scripts/test_desktop.py'), '--',
                                        str(ROOT/'build/Release/VolturaWorlds.exe'), '--scene', scene,
                                        '--capture-dir', str(folder), '--width', '1920', '--height', '1080',
                                        '--quality', quality, '--grid', str({'eco':64,'auto':80,'high':96}[quality]),
                                        '--frames', str(args.frames), '--timing-warmup', str(args.frames//4), '--debug-gpu'],
                                       check=True, timeout=600, capture_output=True)
                        results[phase].append(json.loads((folder/'metrics.json').read_text()))
                old = statistics.median(r['gpuP95Ms'] for r in results['before'])
                new = statistics.median(r['gpuP95Ms'] for r in results['after'])
                result = {'beforeP95Ms': old, 'afterP95Ms': new,
                          'gpuP95ChangePercent': (new/old-1)*100,
                          'gpuBudgetPassed': new <= old*1.05,
                          'allocationUnchanged': {r['estimatedGpuBytes'] for r in results['before']} ==
                                                 {r['estimatedGpuBytes'] for r in results['after']},
                          'runs': results}
                report[scene][quality] = result
                report_path.write_text(json.dumps(report, indent=2))
                print(f'{scene} {quality}: {old:.3f} -> {new:.3f} ms '
                      f'({result["gpuP95ChangePercent"]:+.1f}%)', flush=True)
    finally:
        for name, data in candidate.items():
            (live/name).write_bytes(data)


if __name__ == '__main__':
    main()
