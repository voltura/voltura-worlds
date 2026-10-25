"""Native Bloom output must respond to Density and respect frozen movement."""
from pathlib import Path
import subprocess
import sys
import tempfile
from PIL import Image, ImageChops, ImageStat

scripts = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-bloom-') as directory:
    root = Path(directory)
    base = [sys.executable, str(scripts/'test_desktop.py'), '--', sys.argv[1],
            '--scene', 'bloom', '--width', '640', '--height', '360',
            '--frames', '1', '--debug-gpu']
    def capture(name, density, movement=1, time=0):
        subprocess.run(base+['--capture-dir', str(root/name), '--density', str(density),
                            '--movement', str(movement), '--start-time', str(time)], check=True)
        with Image.open(root/name/'bloom.png') as source:
            return source.convert('RGB')
    def difference(first, second):
        return sum(ImageStat.Stat(ImageChops.difference(first, second)).mean)/3
    sparse = capture('sparse', 0)
    normal = capture('normal', 1)
    dense = capture('dense', 2)
    assert difference(sparse, normal) > 1, 'Density 0 and 1 look unchanged'
    assert difference(normal, dense) > 1, 'Density 1 and 2 look unchanged'
    assert difference(normal, capture('moving', 1, time=12)) > .5, 'Animation stopped'
    assert difference(capture('frozen', 1, 0), capture('frozen-later', 1, 0, 12)) == 0, 'Movement 0 still animates'
    print('PASS: Density visibly changes folds; movement animates; movement zero freezes pixels.')
