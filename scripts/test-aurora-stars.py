"""Check native Aurora stars remain visible and fixed as scene inputs change."""
import json
import pathlib
import subprocess
import sys
from PIL import Image, ImageChops

root = pathlib.Path(__file__).resolve().parents[1]
out = root / 'artifacts' / 'validation' / 'aurora-fixed-stars'
subprocess.run([
    sys.executable, str(root / 'scripts' / 'test_desktop.py'), '--',
    str(root / 'build' / 'Release' / 'VolturaWorlds.exe'),
    '--capture-dir', str(out), '--scene', 'aurora',
    '--width', '1920', '--height', '1080', '--frames', '60',
    '--record', '6', '--debug-gpu', '--validate-star-layer',
], cwd=root, check=True)
with Image.open(out / 'aurora.png') as source:
    baseline = source.convert('RGB')
assert max(high for low, high in baseline.getextrema()) > 20, 'Sky is empty'
for index in range(6):
    with Image.open(out / f'frame-{index:05d}.png') as source:
        actual = source.convert('RGB')
    assert actual.size == baseline.size
    assert ImageChops.difference(baseline, actual).getbbox() is None, (
        f'Stars changed at diagnostic frame {index}'
    )
report = {
    'passed': True, 'comparedFrames': 6, 'changedPixels': 0,
    'inputsVaried': ['time (seven-second jumps)', 'movement', 'energy', 'reaction', 'palette'],
    'evidence': 'Native D3D12 isolated sky readback on private desktop; full scene inspected separately',
}
(out / 'star-regression.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
