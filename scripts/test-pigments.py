import subprocess
import sys
import tempfile
from pathlib import Path
scripts=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-pigments-') as directory:
    subprocess.run([sys.executable,str(scripts/'test_desktop.py'),'--',sys.argv[1],
                    '--capture-dir',directory,'--scene','smoke','--width','640','--height','360',
                    '--frames','1440','--grid','32','--fluid-diagnostics','--debug-gpu'],check=True)
    subprocess.run([sys.executable,str(scripts/'inspect-pigments.py'),directory],check=True)
