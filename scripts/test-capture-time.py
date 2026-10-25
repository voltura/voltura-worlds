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
    for i, (scene, value) in enumerate([('orbit','nan'),('orbit','inf'),('orbit','-1'),
                                        ('orbit','86401'),('orbit','1junk'),('smoke','1')]):
        result = subprocess.run(base+['--scene',scene,'--capture-dir',str(root/f'invalid{i}'),
                                     '--start-time',value])
        assert result.returncode != 0, (scene,value)
    print('PASS: zero offset preserves pixels; later phase changes artwork; invalid or simulated offsets rejected.')
