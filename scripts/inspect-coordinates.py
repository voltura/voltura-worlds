"""Check native material coordinates against inverse rigid rotation.

Only the interior cylinder is compared: it never intersects domain boundaries.
The two packed charts reset independently every four seconds, offset by two.
"""
import json
import math
import struct
import sys
from pathlib import Path

folder = Path(sys.argv[1])
metrics = json.loads((folder/'metrics.json').read_text())
n = metrics['grid']
raw = (folder/'coordinates-rgba32f.bin').read_bytes()
if len(raw) != n*n*n*2*16:
    raise ValueError('Unexpected material-coordinate volume size')
f32 = lambda value: struct.unpack('<f', struct.pack('<f', value))[0]
dt, time = f32(1/60), 0.0
ages = [0.0, 0.0]
for frame in range(metrics['warmupFrames']):
    time = f32(time+dt)
    for chart in range(2):
        phase = chart*2
        reset = frame == 0 or math.floor(f32(time+phase)/4) != math.floor(f32(max(0, f32(time-dt))+phase)/4)
        ages[chart] = 0.0 if reset else ages[chart]+dt
squares, maxima, counts = [0.0]*2, [0.0]*2, [0]*2
for index, actual in enumerate(struct.iter_unpack('<4f', raw)):
    if not all(math.isfinite(v) for v in actual):
        raise ValueError('Nonfinite material coordinate')
    z, rest = divmod(index, n*n*2)
    y, packed_x = divmod(rest, n*2)
    chart, x = divmod(packed_x, n)
    qx, qy, qz = (x+.5)/n-.5, (y+.5)/n-.5, (z+.5)/n-.5
    if qx*qx+qy*qy > .22**2 or abs(qz) > .22:
        continue
    angle = -ages[chart]*2*math.pi/12
    c, s = math.cos(angle), math.sin(angle)
    expected = (.5+c*qx-s*qy, .5+s*qx+c*qy, .5+qz)
    for a, e in zip(actual[:3], expected):
        error = abs(a-e)
        squares[chart] += error*error
        maxima[chart] = max(maxima[chart], error)
        counts[chart] += 1
report = {'chartAgesSeconds': ages, 'maxError': maxima,
          'rmsError': [math.sqrt(squares[i]/counts[i]) for i in range(2)],
          'evidence': 'Native GPU coordinate readback versus analytic inverse rotation; not artwork acceptance'}
(folder/'coordinate-diagnostics.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
if max(maxima) > .0002:
    raise SystemExit('Material coordinates drifted or reset incorrectly')
