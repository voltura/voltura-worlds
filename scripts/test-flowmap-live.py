"""Live-source flow-map smoke: bounded fields, renewal and GPU validation."""
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

scripts = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-live-map-') as directory:
    root = Path(directory)
    masses = []
    for frames, adaptive in ((1, False), (180, False), (180, True)):
        out = root/(str(frames)+('-adaptive' if adaptive else ''))
        subprocess.run([sys.executable, str(scripts/'test_desktop.py'), '--', sys.argv[1],
                        '--capture-dir', str(out), '--scene', 'smoke', '--grid', '32',
                        '--width', '480', '--height', '270', '--frames', str(frames),
                        '--debug-gpu', '--fluid-diagnostics', '--flowmap-live']
                       + (['--flowmap-adaptive'] if adaptive else ['--flowmap-renew', '.5']), check=True)
        metadata = json.loads((out/'metrics.json').read_text())
        assert metadata['flowmapLive'] and metadata['flowmapAdaptive'] == adaptive
        if adaptive:
            assert 0 < metadata['flowmapAdaptiveRenewals'] <= metadata['flowmapRenewals']
        else:
            assert metadata['flowmapRenewals'] == (0 if frames == 1 else 5)
        raw = (out/'density-rgba16f.bin').read_bytes()
        assert len(raw) == 64**3*8
        pigment = [0., 0., 0.]
        mass = 0.
        for rgba in struct.iter_unpack('<4e', raw):
            assert all(math.isfinite(v) and 0 <= v <= 8.001 for v in rgba)
            assert abs(sum(rgba[:3])-rgba[3]) <= max(.00001, rgba[3]*.003), rgba
            mass += rgba[3]
            for i in range(3):
                pigment[i] += rgba[i]
        assert min(pigment) > .01, 'Missing continuously injected pigment'
        masses.append(mass)
    assert masses[1] > masses[0]*10, 'Continuous sources did not accumulate'
    assert masses[2] > masses[0]*10, 'Adaptive renewal lost continuous sources'
    print('PASS: live sources, fixed/adaptive renewal, finite bounded density and pigment mass.')
