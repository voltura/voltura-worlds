"""Capture Sun presets, native motion and sequential 1080p/4K GPU timings."""
import argparse
import json
from pathlib import Path
import statistics
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
import imageio_ffmpeg

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,default=ROOT/'build/Release/VolturaWorlds.exe')
    parser.add_argument('--output',type=Path,default=ROOT/'artifacts/sun/review')
    parser.add_argument('--frames',type=int,default=240)
    parser.add_argument('--repeats',type=int,default=3)
    args=parser.parse_args()
    if args.frames<48 or args.repeats<1:parser.error('Use at least 48 frames and one repeat')
    def capture(name,**options):
        folder=args.output/name
        command=[sys.executable,str(ROOT/'scripts/run-private-desktop.py'),'--',str(args.executable.resolve()),
                 '--capture-dir',str(folder),'--scene','sun']
        for key,value in options.items():command.extend(['--'+key.replace('_','-'),str(value)])
        subprocess.run(command,check=True,capture_output=True,timeout=300)
        return json.loads((folder/'metrics.json').read_text())
    report={'evidence':'Sequential native D3D12 offscreen timings; not physical desktop presentation performance',
            'timings':{}}
    for w,h in ((1920,1080),(3840,2160)):
        for quality in ('eco','auto','high'):
            runs=[capture(f'{w}x{h}-{quality}-{i}',width=w,height=h,frames=args.frames,quality=quality)
                  for i in range(args.repeats)]
            entry={'gpuMedianMs':statistics.median(r['gpuMedianMs'] for r in runs),
                   'gpuP95Ms':statistics.median(r['gpuP95Ms'] for r in runs),'runs':runs}
            report['timings'][f'{w}x{h}-{quality}']=entry
            print(f'{w}x{h} {quality}: {entry["gpuMedianMs"]:.3f} ms median, {entry["gpuP95Ms"]:.3f} ms p95',flush=True)
    for palette in range(4):
        capture(f'palette-{palette}',width=1920,height=1080,frames=1,palette=palette)
    capture('active',width=1920,height=1080,frames=1,turbulence=2,density=1.3,start_time=60)
    capture('quiet',width=1920,height=1080,frames=1,turbulence=0,density=0)
    capture('motion',width=1280,height=720,frames=1,record=180)
    folder=args.output/'motion'
    subprocess.run([imageio_ffmpeg.get_ffmpeg_exe(),'-hide_banner','-loglevel','error','-y',
                    '-framerate','30','-i',str(folder/'frame-%05d.png'),'-c:v','libx264','-crf','18',
                    '-pix_fmt','yuv420p','-an','-movflags','+faststart',str(folder/'sun.mp4')],check=True)
    # Retain representative native frames as well as the complete encoded clip.
    for frame in folder.glob('frame-*.png'):
        if int(frame.stem.split('-')[1]) not in (0,30,60,90,120,150,179):frame.unlink()
    (args.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')

if __name__=='__main__':main()
