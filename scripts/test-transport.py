"""Native GPU regression for cumulative smoke-density loss during transport."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

scripts = Path(__file__).resolve().parent
wide = '--wide' in sys.argv[2:]
cubic = '--cubic' in sys.argv[2:]
with tempfile.TemporaryDirectory(prefix='Voltura-transport-') as directory:
    subprocess.run([
        sys.executable, str(scripts/'test_desktop.py'), '--', sys.argv[1],
        '--capture-dir', directory, '--scene', 'smoke', '--width', '640',
        '--height', '360', '--frames', '720', '--grid', '48' if wide else '64', '--debug-gpu',
        '--fluid-diagnostics', '--transport-test', 'cubic' if cubic else 'maccormack',
    ] + (['--wide-density'] if wide else []), check=True)
    subprocess.run([sys.executable, str(scripts/'inspect-transport.py'), directory], check=True)
    report = json.loads((Path(directory)/'transport-diagnostics.json').read_text())
    if cubic and not (report['peakRatio'] > .66 and report['relativeL2Error'] < .24):
        raise SystemExit('Cubic transport lost its detail-preservation gain')
    if not (.90 < report['massRatio'] < 1.15 and report['peakRatio'] > (.65 if wide else .59)
            and report['relativeL2Error'] < (.25 if wide else .30)):
        raise SystemExit('Smoke transport lost excessive mass or detail')
    print('PASS: one native GPU revolution retains density mass and peak within regression bounds.')
