"""Build a local before/after review and inspect every encoded native frame."""
import argparse
import html
import json
from pathlib import Path
import statistics
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'.deps/python'))
import imageio_ffmpeg
from PIL import Image, ImageChops, ImageDraw, ImageStat

SCENES = ('aurora', 'orbit', 'smoke', 'bloom', 'silk', 'eclipse',
          'lattice', 'drift', 'contour', 'mercury')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'artifacts/cinematic')
    args = parser.parse_args()
    output = args.output.resolve()
    comparison = json.loads((output/'comparison.json').read_text())
    paired_path = output/'paired-performance.json'
    paired = json.loads(paired_path.read_text()) if paired_path.exists() else {}
    temporal = {}
    sections = []
    overview = Image.new('RGB', (1280, 10*210), '#080d15')
    draw = ImageDraw.Draw(overview)
    for row, scene in enumerate(SCENES):
        temporal[scene] = {}
        strip = Image.new('RGB', (6*320, 2*200), '#080d15')
        labels = ImageDraw.Draw(strip)
        for phase_index, phase in enumerate(('before', 'after')):
            video = output/phase/scene/'motion'/f'{scene}.mp4'
            reader = imageio_ffmpeg.read_frames(str(video), pix_fmt='rgb24',
                                               output_params=['-vf', 'scale=320:180'])
            metadata = next(reader)
            previous = None
            changes = []
            frames = 0
            try:
                for index, pixels in enumerate(reader):
                    frame = Image.frombytes('RGB', (320, 180), pixels)
                    if previous is not None:
                        changes.append(sum(ImageStat.Stat(ImageChops.difference(previous, frame)).mean)/3)
                    previous = frame
                    frames += 1
                    if index in (0, 30, 90, 180, 270, 359):
                        column = (0, 30, 90, 180, 270, 359).index(index)
                        strip.paste(frame, (column*320, phase_index*200+20))
                        labels.text((column*320+8, phase_index*200+4), f'{phase} {index/30:.2f}s', fill='white')
            finally:
                reader.close()
            assert frames == 360, f'{scene} {phase}: expected 360 original frames, got {frames}'
            temporal[scene][phase] = {
                'frames': frames, 'fps': metadata['fps'],
                'meanAdjacentChange255': statistics.mean(changes),
                'medianAdjacentChange255': statistics.median(changes),
                'maximumAdjacentChange255': max(changes),
                'unchangedAdjacentFrames': sum(change == 0 for change in changes),
                'evidence': 'Decoded 320x180 video frame differences; not artistic or physical-display acceptance',
            }
            with Image.open(output/phase/scene/'auto/0'/f'{scene}.png') as image:
                overview.paste(image.resize((320, 180)), (phase_index*640+160, row*210+25))
            draw.text((phase_index*640+160, row*210+6), f'{scene.capitalize()} / {phase}', fill='white')
        strip.save(output/f'{scene}-motion-contact.jpg', quality=92)
        entries = comparison[scene]
        timings = ' · '.join(f'{q.capitalize()}: {paired.get(scene, {}).get(q, entries[q])["gpuP95ChangePercent"]:+.1f}% GPU'
                             for q in ('eco', 'auto', 'high'))
        sections.append(f'''<section><h2>{html.escape(scene.capitalize())}</h2>
<p>{html.escape(timings)}</p><p><button data-play>Play both from start</button>
<button data-pause>Pause both</button></p><div class="pair">
<figure><figcaption>Before</figcaption><img src="before/{scene}/auto/0/{scene}.png">
<video controls preload="none" src="before/{scene}/motion/{scene}.mp4"></video></figure>
<figure><figcaption>After</figcaption><img src="after/{scene}/auto/0/{scene}.png">
<video controls preload="none" src="after/{scene}/motion/{scene}.mp4"></video></figure></div>
<details><summary>Motion samples</summary><img src="{scene}-motion-contact.jpg"></details></section>''')
    overview.save(output/'overview.jpg', quality=94)
    (output/'temporal-review.json').write_text(json.dumps(temporal, indent=2))
    (output/'review.html').write_text('''<!doctype html><html lang="en"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Worlds cinematic review</title>
<style>body{margin:32px auto;padding:0 24px;max-width:1600px;background:#080d15;color:#dce5f0;font:16px system-ui}
h1,h2{font-weight:500}p{color:#a8bbce}section{margin:48px 0}.pair{display:grid;grid-template-columns:1fr 1fr;gap:16px}
figure{margin:0}figcaption{margin-bottom:8px}img,video{width:100%;display:block;margin-bottom:12px;border-radius:6px}
button{font:inherit;color:#dce5f0;background:#152536;border:1px solid #42596e;border-radius:5px;padding:7px 12px;cursor:pointer}
summary{cursor:pointer;padding:12px 0}@media(max-width:800px){.pair{grid-template-columns:1fr}}</style>
<h1>Voltura Worlds — cinematic review</h1><p>Left: original renderer. Right: cinematic revision.
Each clip contains 360 native frames at 30 fps (12 seconds). Native D3D12 offscreen readback on a private
Windows desktop; no physical-display or visual-acceptance claim. GPU changes compare medians of repeated
p95 capture timings at matching settings, using alternating warmed runs where available.
Allocations and detailed runs are in comparison.json and paired-performance.json.</p>'''
        + '\n'.join(sections) + '''<script>
document.querySelectorAll('[data-play],[data-pause]').forEach(button => {
  button.addEventListener('click', () => {
    button.closest('section').querySelectorAll('video').forEach(video => {
      if (button.hasAttribute('data-play')) { video.currentTime = 0; video.play().catch(() => {}); }
      else video.pause();
    });
  });
});
</script></html>''', encoding='utf-8')
    print('Created review.html, overview.jpg, motion contacts, and temporal-review.json')


if __name__ == '__main__':
    main()
