"""Native live-flow check for useful coordinate lifetime, distinct from analytic rotation."""
import subprocess
import sys
import tempfile
from pathlib import Path
scripts=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-chart-renewal-') as folder:
    subprocess.run([sys.executable,str(scripts/'test_desktop.py'),'--',sys.argv[1],
                    '--capture-dir',folder,'--scene','smoke','--width','640','--height','360',
                    '--frames','720','--grid','64','--debug-gpu','--detail-coordinates','--fluid-diagnostics'],check=True)
    subprocess.run([sys.executable,str(scripts/'inspect-chart-strain.py'),folder,'--check'],check=True)
    print('PASS: live detail chart stays within distortion regression bounds at12seconds.')
