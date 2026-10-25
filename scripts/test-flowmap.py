"""Native analytic flow-map transport: motion and fine-detail retention."""
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

n = 128
scripts = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-flowmap-') as directory:
    root = Path(directory)
    plane = bytearray()
    for y in range(n):
        for x in range(n):
            px, py = (x+.5)/n, (y+.5)/n
            d = (.08+.06*math.cos(2*math.pi*32*px))*math.exp(-((px-.5)**2+(py-.5)**2)/.02)
            plane += struct.pack('<4e', 0, d, 0, d)
    source = root/'density.bin'
    source.write_bytes(plane*n)
    for frames, deform, renew in ((180, False, False), (720, False, False),
                                  (360, True, False), (720, True, False), (720, True, True)):
        out = root/(str(frames)+('-deform' if deform else '')+('-renew' if renew else ''))
        subprocess.run([sys.executable, str(scripts/'test_desktop.py'), '--', sys.argv[1],
                        '--capture-dir', str(out), '--scene', 'smoke', '--grid', '64',
                        '--width', '640', '--height', '360', '--frames', str(frames),
                        '--debug-gpu', '--fluid-diagnostics', '--smoke-reference-volume', str(source),
                        '--transport-test', 'maccormack', '--flowmap-reference']
                       + (['--transport-deform'] if deform else [])
                       + (['--flowmap-renew', '2', '--flowmap-cubic'] if renew else []), check=True)
        metadata = json.loads((out/'metrics.json').read_text())
        assert metadata['flowmapReference'] and metadata['simulationSteps'] == frames
        assert metadata['flowmapRenewals'] == (5 if renew else 0)
        raw = (out/'density-rgba16f.bin').read_bytes()
        error = norm = unchanged_error = 0.
        for y in range(40, 88):
            for x in range(40, 88):
                px, py = (x+.5)/n, (y+.5)/n
                original_x = py if frames == 180 else px
                envelope = math.exp(-((px-.5)**2+(py-.5)**2)/.02)
                expected = (.08+.06*math.cos(2*math.pi*32*original_x))*envelope
                unchanged = (.08+.06*math.cos(2*math.pi*32*px))*envelope
                actual = struct.unpack_from('<e', raw, ((n//2*n+y)*n+x)*8+6)[0]
                assert math.isfinite(actual) and actual >= 0
                error += (actual-expected)**2
                norm += expected**2
                unchanged_error += (actual-unchanged)**2
        relative = math.sqrt(error/norm)
        if deform and frames == 360:
            assert relative > .4, 'Deformation field did not deform density'
        else:
            assert relative < (.25 if renew else .06), (frames, deform, renew, relative)
        if frames == 180:
            assert math.sqrt(unchanged_error/norm) > .4, 'Field did not visibly rotate'
        print(f'PASS: {frames} steps, deformation={deform}, renewal={renew}, relative L2 to analytic return/rotation {relative:.6f}')
