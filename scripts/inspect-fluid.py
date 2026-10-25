"""Inspect native GPU field dumps; no simulation or reference image is substituted."""
import json
import math
import struct
import sys
from pathlib import Path

folder = Path(sys.argv[1])
n = json.loads((folder / 'metrics.json').read_text())['grid']

def read(name, code, count):
    data = (folder / name).read_bytes()
    if len(data) != struct.calcsize(code) * count:
        raise ValueError(f'Unexpected field size: {name}')
    return struct.unpack('<' + code * count, data)

velocity = read('velocity-rgba16f.bin', 'e', n**3 * 4)
before = read('divergence-before-r32f.bin', 'f', n**3)
pressure = read('pressure-r32f.bin', 'f', n**3)
if not all(math.isfinite(v) for field in (velocity, before, pressure) for v in field):
    raise ValueError('Non-finite GPU fluid state')

pre, post, residual, consistency = [], [], [], []
for z in range(1, n-2):
    for y in range(1, n-2):
        for x in range(1, n-2):
            i = (z*n+y)*n+x
            div = sum(velocity[(i+s)*4+c]-velocity[i*4+c]
                      for c, s in enumerate((1, n, n*n)))
            lap = sum(pressure[i+s]+pressure[i-s] for s in (1, n, n*n))-6*pressure[i]
            pre.append(before[i])
            post.append(div)
            residual.append(before[i]-lap)
            consistency.append(div-(before[i]-lap))

def stats(values):
    return {'rms': math.sqrt(sum(v*v for v in values)/len(values)),
            'maxAbs': max(abs(v) for v in values)}

report = {'grid': n, 'interiorCells': len(pre), 'finite': True,
          'beforeProjection': stats(pre), 'afterProjection': stats(post),
          'pressureResidual': stats(residual),
          'projectionConsistencyError': stats(consistency),
          'maxVelocityComponent': max(abs(v) for v in velocity),
          'evidence': 'Native GPU field readback; interior discrete divergence in cell units'}
report['remainingDivergenceRatio'] = report['afterProjection']['rms']/max(report['beforeProjection']['rms'], 1e-12)
(folder/'fluid-diagnostics.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
