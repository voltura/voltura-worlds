"""Summarize captured frame changes; these numbers do not grade artistic quality."""
import argparse
import json
from pathlib import Path
from PIL import Image, ImageChops, ImageStat

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('folder', type=Path)
args = parser.parse_args()
frames = sorted(args.folder.glob('frame-*.png'))
if len(frames) < 2:
    parser.error('At least two complete native frames are required')
if any(path.name != f'frame-{index:05d}.png' for index, path in enumerate(frames)):
    parser.error('Expected a contiguous capture beginning with frame-00000.png')

pairs = []
previous = None
for index in sorted(set(range(0, len(frames), 30)) | {len(frames) - 1}):
    with Image.open(frames[index]) as source:
        frame = source.convert('RGB')
    if previous is not None:
        if previous.size != frame.size:
            raise SystemExit('Capture dimensions changed within the sequence')
        delta = ImageChops.difference(previous, frame)
        pairs.append({'frame': index, 'meanAbsoluteChannelChange255':
                      sum(ImageStat.Stat(delta).mean) / 3})
    previous = frame
report = {
    'frames': len(frames), 'width': previous.width, 'height': previous.height,
    'recordingSeconds': len(frames) / 30,
    'intervals': pairs,
    'evidence': 'PNG pixel change only; not motion smoothness, artistic quality or physical-display validation',
}
(args.folder / 'motion-analysis.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
