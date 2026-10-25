"""Conservative empty-space skipping must preserve native captured pixels."""
import subprocess
import sys
import tempfile
from pathlib import Path
scripts = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-empty-space-') as directory:
    root = Path(directory)
    for name, grid, frames, extra in [('sparse', 32, 90, []), ('mature', 32, 360, []), ('rectangular', 48, 360, ['--wide-density']),
                                     ('fine', 32, 360, ['--fine-atlas', '--fine-atlas-front'])]:
        for mode, option in [('skip', []), ('control', ['--no-empty-skip'])]:
            subprocess.run([sys.executable, str(scripts/'test_desktop.py'), '--', sys.argv[1],
                            '--capture-dir', str(root/name/mode), '--scene', 'smoke', '--width', '960', '--height', '540',
                            '--frames', str(frames), '--grid', str(grid), '--debug-gpu'] + extra + option, check=True)
        assert (root/name/'skip/smoke.png').read_bytes() == (root/name/'control/smoke.png').read_bytes(), name
        print(f'PASS: {name} native PNG identical with empty-space skipping enabled/disabled.')
