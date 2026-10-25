"""Validate the synthesized field itself, independently of coarse projection."""
import array
import json
import math
import sys
from pathlib import Path

folder = Path(sys.argv[1])
metrics = json.loads((folder/'metrics.json').read_text())
nx, ny, nz = metrics['densityDimensions']
values = array.array('f')
values.frombytes((folder/'detail-velocity-rgba32f.bin').read_bytes())
if sys.byteorder != 'little':
    values.byteswap()
if len(values) != nx*ny*nz*4 or not all(math.isfinite(v) for v in values):
    raise ValueError('Invalid or nonfinite detail velocity')
sum_div = sum_derivative = peak_div = peak_velocity = 0.0
count = 0
for z in range(1, nz-1):
    for y in range(1, ny-1):
        for x in range(1, nx-1):
            i = ((z*ny+y)*nx+x)*4
            dx = (values[i+4]-values[i-4])*(nx*.5)
            dy = (values[i+nx*4+1]-values[i-nx*4+1])*(ny*.5)
            dz = (values[i+nx*ny*4+2]-values[i-nx*ny*4+2])*(nz*.5)
            div = dx+dy+dz
            sum_div += div*div
            sum_derivative += dx*dx+dy*dy+dz*dz
            peak_div = max(peak_div, abs(div))
            peak_velocity = max(peak_velocity, *(abs(v) for v in values[i:i+3]))
            count += 1
report = {'divergenceRms': math.sqrt(sum_div/count), 'maxDivergence': peak_div,
          'relativeDivergence': math.sqrt(sum_div/max(sum_derivative, 1e-30)),
          'maxVelocityComponent': peak_velocity,
          'evidence': 'Native synthesized-field readback; central discrete divergence, not interpolated trajectory conservation'}
(folder/'detail-diagnostics.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
if peak_velocity < .001 or report['relativeDivergence'] > .0002 or report['divergenceRms'] > .0001:
    raise SystemExit('Detail field is inactive or has excessive discrete divergence')
