"""Verify GPU timing scopes against their enclosing frame and unchanged pixels."""
import json
import math
import subprocess
import sys
import tempfile
from pathlib import Path
scripts = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-stage-timing-') as temp:
    root = Path(temp)
    for name, extra in [('off', []), ('on', ['--gpu-stages'])]:
        subprocess.run([sys.executable, str(scripts/'test_desktop.py'), '--', sys.argv[1],
                        '--capture-dir', str(root/name), '--scene', 'smoke', '--width', '640',
                        '--height', '360', '--frames', '90', '--grid', '32', '--debug-gpu'] + extra, check=True)
    baseline = json.loads((root/'off/metrics.json').read_text())
    report = json.loads((root/'on/metrics.json').read_text())
    assert 'gpuStages' not in baseline
    assert len(report['gpuStages']) == 14
    total = 0.0
    for name, stage in report['gpuStages'].items():
        assert stage['samples'] == 79, (name, stage)
        assert all(math.isfinite(stage[key]) and stage[key] >= 0 for key in ('meanMs', 'p95Ms'))
        total += stage['meanMs']
    assert report['gpuStages']['sceneDraw']['meanMs'] > 0
    assert report['gpuStages']['pressureIterations']['meanMs'] > 0
    assert 0 < total <= report['gpuMeanMs'] + .001, (total, report['gpuMeanMs'])
    assert (root/'off/smoke.png').read_bytes() == (root/'on/smoke.png').read_bytes()
    print(json.dumps({'passed': True, 'scopeMeanSumMs': total, 'frameMeanMs': report['gpuMeanMs'],
                      'identicalPixels': True, 'evidence': 'Native GPU queries on private desktop'}, indent=2))
