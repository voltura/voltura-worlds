"""Exercise independent chart resets and transport on the native GPU."""
import subprocess
import sys
import tempfile
from pathlib import Path

scripts = Path(__file__).resolve().parent
for frames in (1, 121, 241, 541):
    with tempfile.TemporaryDirectory(prefix='Voltura-coordinates-') as folder:
        subprocess.run([sys.executable, str(scripts/'test_desktop.py'), '--', sys.argv[1],
                        '--capture-dir', folder, '--scene', 'smoke', '--width', '640', '--height', '360',
                        '--frames', str(frames), '--grid', '32', '--debug-gpu', '--fluid-diagnostics',
                        '--transport-test', 'maccormack', '--detail-coordinates'], check=True)
        subprocess.run([sys.executable, str(scripts/'inspect-coordinates.py'), folder], check=True)
        subprocess.run([sys.executable, str(scripts/'inspect-detail.py'), folder], check=True)
print('PASS: native detail-coordinate initialization, transport and staggered resets')
