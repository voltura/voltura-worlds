"""Capture offsets exercise later Orbit rotations without simulating Smoke history."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

scripts = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-capture-time-') as directory:
    root = Path(directory)
    base = [sys.executable, str(scripts/'test_desktop.py'), '--', sys.argv[1],
            '--width', '640', '--height', '360', '--frames', '1', '--debug-gpu']
    for name, args in [('default', []), ('zero', ['--start-time','0']), ('later', ['--start-time','500'])]:
        subprocess.run(base+['--scene','orbit','--capture-dir',str(root/name)]+args,check=True)
    assert (root/'default/orbit.png').read_bytes() == (root/'zero/orbit.png').read_bytes()
    assert (root/'later/orbit.png').read_bytes() != (root/'zero/orbit.png').read_bytes()
    assert json.loads((root/'later/metrics.json').read_text())['startTimeOffsetSeconds'] == 500
    timing_base = base.copy()
    timing_base[timing_base.index('--frames')+1] = '16'
    for name, options in [('timing-default', []), ('timing-warm', ['--timing-warmup', '12'])]:
        subprocess.run(timing_base+['--scene','orbit','--capture-dir',str(root/name)]+options,check=True)
    assert (root/'timing-default/orbit.png').read_bytes() == (root/'timing-warm/orbit.png').read_bytes()
    timing = json.loads((root/'timing-warm/metrics.json').read_text())
    assert timing['timingWarmupFrames'] == 12 and timing['timingSampleCount'] == 4
    for index, value in enumerate(['-1', '36001', '1junk']):
        result = subprocess.run(base+['--scene','orbit','--capture-dir',str(root/f'invalid-warmup-{index}'),
                                     '--timing-warmup',value])
        assert result.returncode != 0, value
    for i, (scene, value) in enumerate([('orbit','nan'),('orbit','inf'),('orbit','-1'),
                                        ('orbit','86401'),('orbit','1junk'),('smoke','1')]):
        result = subprocess.run(base+['--scene',scene,'--capture-dir',str(root/f'invalid{i}'),
                                     '--start-time',value])
        assert result.returncode != 0, (scene,value)
    print('PASS: zero offset preserves pixels; later phase changes artwork; invalid or simulated offsets rejected.')
