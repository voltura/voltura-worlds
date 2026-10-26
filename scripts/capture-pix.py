"""Capture one real presented frame on the task-owned private Windows desktop."""
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
pix = pathlib.Path('C:/Program Files/Microsoft PIX/2603.25/pixtool.exe')
scene = sys.argv[1] if len(sys.argv) > 1 else 'orbit'
if scene not in ('aurora', 'orbit', 'smoke'):
    raise SystemExit('Expected aurora, orbit, or smoke')
output = root / 'artifacts' / 'pix' / scene
output.mkdir(parents=True, exist_ok=True)
args = 'profile-' + scene
subprocess.run([
    sys.executable, str(root / 'scripts/run-private-desktop.py'), '--', str(pix),
    '--log-file=' + str(output / 'capture.log'),
    'launch', '--command-line=' + args, '--working-directory=' + str(root),
    str(root / 'build/Release/VolturaWorlds.exe'),
    'take-capture', '--open', 'save-capture', str(output / 'frame.wpix'),
], check=True, timeout=120)

print(output / "frame.wpix")
print("Detailed PIX playback analysis requires Windows Developer Mode.")
