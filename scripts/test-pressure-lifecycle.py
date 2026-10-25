"""Capture-only pressure hierarchy lifecycle and zero-time redraw checks."""
import subprocess,sys,tempfile
from pathlib import Path
scripts=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-multigrid-') as directory:
 root=Path(directory)
 command=[sys.executable,str(scripts/'test_desktop.py'),'--',sys.argv[1],'--capture-dir',str(root),'--scene','smoke','--width','640','--height','360','--frames','120','--grid','32','--debug-gpu','--smoke-multigrid','--validate-lifecycle','--validate-paused-frame','--fluid-diagnostics']
 subprocess.run(command,check=True)
 subprocess.run([sys.executable,str(scripts/'inspect-pressure-boundaries.py'),str(root)],check=True)
 assert (root/'smoke.png').read_bytes()==(root/'smoke-paused.png').read_bytes(),'Multigrid scene changed during zero-time redraw'
 for grid in ['24','48','96','192']:
  invalid=command.copy();invalid[invalid.index('--grid')+1]=grid
  assert subprocess.run(invalid).returncode!=0,'Accepted unsupported hierarchy grid '+grid
 print('PASS: multigrid lifecycle, zero-time redraw and unsupported grid rejection')
