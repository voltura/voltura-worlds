"""Persistent tracer rendering survives scene switches and zero-time redraws."""
import subprocess
import sys
import tempfile
from pathlib import Path
scripts=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-tracers-') as directory:
    root=Path(directory)
    subprocess.run([sys.executable,str(scripts/'test_desktop.py'),'--',sys.argv[1],
                    '--capture-dir',str(root),'--scene','smoke','--width','960','--height','540',
                    '--frames','360','--grid','32','--debug-gpu','--smoke-filaments',
                    '--validate-lifecycle','--validate-paused-frame'],check=True)
    assert (root/'smoke.png').read_bytes()==(root/'smoke-paused.png').read_bytes(), 'Tracer state moved during zero-time redraw'
    print('PASS: scene/resize lifecycle and pixel-identical zero-time redraws with persistent tracers.')
