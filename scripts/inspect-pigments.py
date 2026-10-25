"""Check native Smoke pigment concentration invariants (not transport-test RGB)."""
import json
import math
import struct
import sys
from pathlib import Path
folder = Path(sys.argv[1])
metrics = json.loads((folder/'metrics.json').read_text())
size = math.prod(metrics['densityDimensions'])
raw = (folder/'density-rgba16f.bin').read_bytes()
assert len(raw) == size*8
invalid = active = 0
max_error = 0.0
for r,g,b,density in struct.iter_unpack('<4e', raw):
    if not all(math.isfinite(v) and v >= 0 for v in (r,g,b,density)):
        invalid += 1
        continue
    error = abs(r+g+b-density)
    if error > .0025*density + 2e-7:
        invalid += 1
    if density > .012:
        active += 1
        max_error = max(max_error,error/density)
report = {'invalidCells':invalid,'activeCells':active,'maxActiveRelativeSumError':max_error,
          'evidence':'Native GPU concentration readback; FP16 sum tolerance, not artwork acceptance'}
(folder/'pigment-diagnostics.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
assert active > 0 and invalid == 0
