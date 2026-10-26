"""Record SDR and synthetic FP16 output, memory and timing evidence (no physical HDR claim)."""
from pathlib import Path
import json
import math
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
report = {}
for scene in ('aurora', 'orbit', 'sun'):
    report[scene] = {}
    for hdr in (False, True):
        mode = 'hdr' if hdr else 'sdr'
        output = ROOT/'artifacts/hdr/diagnostics'/scene/mode
        command = [sys.executable, str(ROOT/'scripts/test_desktop.py'), '--',
                   str(ROOT/'build/Release/VolturaWorlds.exe'), '--capture-dir', str(output),
                   '--scene', scene, '--width', '1920', '--height', '1080',
                   '--frames', '120', '--debug-gpu']
        if scene == "sun":
            command += ["--show-body-statistics"]
        if hdr:
            command += ['--hdr-output', '--hdr-white', '203', '--hdr-peak', '400']
        subprocess.run(command, check=True, timeout=240)
        metrics = json.loads((output/'metrics.json').read_text())
        report[scene][mode] = metrics
        if hdr:
            raw = output/(scene+'.rgba16f')
            metadata = json.loads((output/(scene+'.rgba16f.json')).read_text())
            data = raw.read_bytes()
            assert len(data) == metadata['width']*metadata['height']*8
            maximum = 0
            bright = 0
            for *rgb, alpha in struct.iter_unpack('<4e', data):
                assert alpha == 1
                assert all(math.isfinite(v) and 0 <= v <= 5 for v in rgb)
                k = max(rgb)
                maximum = max(maximum, k)
                bright += k > 203/80
            metrics['readback'] = {'maximumScRgb': maximum, 'pixelsAboveReferenceWhite': bright,
                                   'pixelCount': metadata['width']*metadata['height'],
                                   'physicalHdrVerified': False}
            if scene == 'orbit':
                assert bright > 0, 'Orbit highlights must exceed reference white'
        print(scene, mode, metrics['estimatedGpuBytes'], metrics['gpuP95Ms'], flush=True)
(ROOT/'artifacts/hdr/diagnostics/results.json').write_text(json.dumps(report, indent=2))

# Bad synthetic values must fail before writing pixels.
for number, flags in enumerate((['--hdr-white', '0'], ['--hdr-white', '203junk'],
                                ['--hdr-white', '500', '--hdr-peak', '400'], ['--record', '1'])):
    output = ROOT/'artifacts/hdr/diagnostics'/f'invalid-{number}'
    result = subprocess.run([sys.executable, str(ROOT/'scripts/test_desktop.py'), '--',
                             str(ROOT/'build/Release/VolturaWorlds.exe'), '--capture-dir', str(output),
                             '--width', '64', '--height', '64', '--frames', '1', '--hdr-output', *flags],
                            timeout=60)
    assert result.returncode != 0, f'Invalid diagnostic flags accepted: {flags}'
    assert not list(output.glob('*.rgba16f'))
print('PASS: native HDR readbacks, SDR references, finite bounds and invalid diagnostic flags')
