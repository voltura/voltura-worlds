"""Compare a native GPU rotation diagnostic against its analytic initial field.

Run a 720-frame --transport-test capture (12 seconds, exactly one revolution).
This is a transport diagnostic, not evidence of finished smoke artwork.
"""
import json
import math
import struct
import sys
from pathlib import Path

folder = Path(sys.argv[1])
metrics = json.loads((folder / 'metrics.json').read_text())
if metrics['warmupFrames'] != 720:
    raise ValueError('Expected exactly one 720-frame revolution')
n = metrics['densityGrid']
nx, ny, nz = metrics.get('densityDimensions', [n, n, n])
full_precision = (folder / 'density-rgba32f.bin').exists()
raw = (folder / ('density-rgba32f.bin' if full_precision else 'density-rgba16f.bin')).read_bytes()
if len(raw) != nx*ny*nz * (16 if full_precision else 8):
    raise ValueError('Unexpected density field size')
total = expected_total = error = square_error = square_expected = 0.0
maximum = expected_maximum = 0.0
negative = nonfinite = invalid_pigment = 0
pigment_square_error = [0.0]*3
pigment_square_expected = [0.0]*3
for index, rgba in enumerate(struct.iter_unpack('<4f' if full_precision else '<4e', raw)):
    z, remainder = divmod(index, nx*ny)
    y, x = divmod(remainder, nx)
    qx, qy, qz = (x+.5)/nx-.65, (y+.5)/ny-.5, (z+.5)/nz-.5
    expected = .1*math.exp(-(qx*qx+qy*qy+qz*qz)/.0025)*(1+.3*math.cos(qx*80)*math.cos(qy*80)*math.cos(qz*80))
    actual = rgba[3]
    if not all(math.isfinite(v) for v in rgba):
        nonfinite += 1
        continue
    negative += actual < 0
    expected_pigments = [expected*(.5+.45*math.sin(qx*100)),expected*(.5+.45*math.sin(qy*110)),expected*(.5+.45*math.cos(qz*90))]
    invalid_pigment += any(v < 0 or v > actual + .00001 for v in rgba[:3])
    for c in range(3):
        pigment_square_error[c] += (rgba[c]-expected_pigments[c])**2
        pigment_square_expected[c] += expected_pigments[c]**2
    total += actual
    expected_total += expected
    error += abs(actual-expected)
    square_error += (actual-expected)**2
    square_expected += expected**2
    maximum = max(maximum, actual)
    expected_maximum = max(expected_maximum, expected)
report = {
    'densityDimensions': [nx,ny,nz], 'storageBits': 32 if full_precision else 16, 'revolutions': 1, 'nonfiniteCells': nonfinite,
    'negativeDensityCells': negative, 'invalidPigmentCells': invalid_pigment,
    'pigmentRelativeL2Error': [math.sqrt(a/b) for a,b in zip(pigment_square_error,pigment_square_expected)], 'massRatio': total/expected_total,
    'peakRatio': maximum/expected_maximum,
    'relativeL1Error': error/expected_total,
    'relativeL2Error': math.sqrt(square_error/square_expected),
    'evidence': 'Native D3D12 density readback versus analytic rigid rotation; not artwork acceptance',
}
(folder/'transport-diagnostics.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
if nonfinite or negative or invalid_pigment:
    raise SystemExit('Non-finite, negative, or out-of-density-range pigment values')
