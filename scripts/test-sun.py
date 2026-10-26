"""Sun source alignment, settings, frozen audio, framing and GPU lifecycle."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
from PIL import Image,ImageChops,ImageStat

def difference(a,b):
    return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)/3

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable',type=Path)
    parser.add_argument('--output',type=Path,default=ROOT/'artifacts/sun/regressions')
    args=parser.parse_args()
    results={}
    def capture(name,**options):
        folder=args.output/name
        command=[sys.executable,str(ROOT/'scripts/test_desktop.py'),'--',
                 str(args.executable.resolve()),'--capture-dir',str(folder),
                 '--scene','sun','--debug-gpu']
        for key,value in {'width':640,'height':360,'frames':1,'flow':0,**options}.items():
            command.append('--'+key.replace('_','-'))
            if value is not True:command.append(str(value))
        run=subprocess.run(command,capture_output=True,text=True,timeout=240)
        if run.returncode:raise RuntimeError(name+': '+run.stdout+'\n'+run.stderr)
        metrics=json.loads((folder/'metrics.json').read_text())
        assert metrics['sceneSamples']==1,'Sun allocated redundant MSAA'
        with Image.open(folder/'sun.png') as image:return image.convert('RGB')

    base=capture('baseline')
    assert max(hi for lo,hi in base.getextrema())>150,'Blank or dim Sun'
    assert ImageStat.Stat(base.crop((270,140,370,220))).mean[0]>80,'Disk missing'
    for parameter in ('brightness','detail','turbulence','density'):
        change=difference(capture(parameter+'-zero',**{parameter:0}),
                          capture(parameter+'-two',**{parameter:2}))
        assert change>.01,parameter+' has no visible effect'
        results[parameter+'Difference255']=change
    assert max(hi for lo,hi in capture('black',brightness=0).getextrema())<=1,'Brightness zero leaves emission'
    quiet=capture('quiet',turbulence=0,density=0)
    assert ImageStat.Stat(quiet.crop((280,150,360,210))).mean[0]>80,'Zero corona/activity removes disk'
    frozen=capture('frozen',movement=0,flow=2)
    assert difference(frozen,capture('frozen-later',movement=0,flow=2,start_time=450,
                                    reaction='expressive',strength=1,energy=1))==0,'Movement zero does not freeze motion/audio'
    assert difference(base,capture('later',start_time=120))>.01,'Plasma does not animate'
    living=capture('surface-six-seconds',start_time=6)
    interior=(265,125,375,235)
    change=difference(base.crop(interior),living.crop(interior))
    assert change>3,'Surface motion is too subtle with camera drift disabled'
    results['fixedCameraSurfaceSixSecondDifference255']=change
    plume_change=difference(base.crop((160,85,240,150)),living.crop((160,85,240,150)))
    assert plume_change>.03,'Observed prominence layer is static with camera drift disabled'
    results['fixedCameraProminenceSixSecondDifference255']=plume_change
    # Isolate off-limb arches from the halo: Corona zero must still allow
    # activity to emit above the limb, concentrated in distinct regions.
    arches=capture('arches-no-corona',density=0)
    loop_pixels=empty_pixels=0
    for y in range(20,340):
        for x in range(160,480):
            radius=math.hypot(x+.5-320,y+.5-180)/(360*.32)
            if 1.08<radius<1.42:
                if max(arches.getpixel((x,y)))>45:loop_pixels+=1
                else:empty_pixels+=1
    assert loop_pixels>80 and empty_pixels>loop_pixels*2,'Distinct off-limb prominences are missing or form a uniform halo'
    results['offLimbLoopPixels']=loop_pixels
    assert difference(capture('camera-zero',start_time=120),
                      capture('camera-two',start_time=120,flow=2))>.1,'Camera drift has no effect'
    off=capture('audio-off',reaction='off',strength=1,energy=1)
    gentle=capture('audio-gentle',reaction='gentle',strength=1,energy=1)
    expressive=capture('audio-expressive',reaction='expressive',strength=1,energy=1)
    assert difference(base,off)==0,'Audio Off reacts'
    assert 0<difference(off,gentle)<difference(off,expressive),'Audio reaction levels incorrect'
    assert difference(base,capture('audio-strength-zero',reaction='expressive',strength=0,energy=1))==0,'Audio strength zero reacts'
    for palette in (1,2,3):
        assert difference(base,capture('palette-'+str(palette),palette=palette))>.1,'Palette has no effect'
    for quality in ('eco','auto','high'):
        image=capture('quality-'+quality)
        # Quality may simplify loops, but photographic structure remains.
        assert difference(base.crop((285,155,350,210)),image.crop((285,155,350,210)))<.1,'Quality changes source structure'
    # Independently predict the display of source samples away from added loops.
    # This rejects sphere-mapping, mirroring, tint-only placeholders and invented
    # geography while allowing BC7 filtering and the shared bloom/postprocess.
    source_view=capture('source-alignment',turbulence=0,density=0,brightness=.2,palette=3)
    with Image.open(ROOT/'assets/sun/runtime/sun-aia171.dds') as im:source=im.convert('RGB')
    errors=[]
    for y in range(110,250,7):
        for x in range(250,390,7):
            qx=(x+.5-320)/(360*.32);qy=(y+.5-180)/(360*.32)
            if qx*qx+qy*qy>.65:continue
            u=int(2048+qx*1605);v=int(2048+qy*1605)
            encoded=source.getpixel((u,v))[0]/255
            linear=encoded/12.92 if encoded<=.04045 else ((encoded+.055)/1.055)**2.4
            intensity=(.028+linear**1.05*3.5)*.2
            rgb=[intensity*c for c in (.82,.85,.9)];peak=max(rgb)
            expected=[255*(.62*(1-math.exp(-c))+.38*c*(1-math.exp(-peak))/peak)**(1/2.2) for c in rgb]
            errors.extend(abs(a-b) for a,b in zip(source_view.getpixel((x,y)),expected))
    error=sum(errors)/len(errors)
    assert error<10,f'Photographic source is misaligned: mean error {error}'
    results['sourceMeanError255']=error
    for w,h in ((1920,1080),(3840,1080),(720,1280)):
        image=capture(f'framing-{w}x{h}',width=w,height=h,flow=2,start_time=150,density=2,turbulence=2)
        box=image.convert('L').point(lambda c:255 if c>15 else 0).getbbox()
        assert box and min(box[0],box[1],w-box[2],h-box[3])>8,'Corona or loops clip viewport'
    capture('switch-resize',validate_lifecycle=True,frames=24)
    manifest=json.loads((ROOT/'assets/sun/sources.json').read_text())
    assert manifest['sourceDimensions']==[4096,4096] and manifest['wavelengthAngstrom']==171,'Incorrect source'
    entries=json.loads((ROOT/'assets/sun/runtime-hashes.json').read_text(encoding='utf-8-sig'))
    if isinstance(entries,dict):entries=[entries]
    assert len(entries)==2,'Missing prominence texture'
    for entry in entries:
        assert hashlib.sha256((ROOT/'assets/sun/runtime'/entry['File']).read_bytes()).hexdigest()==entry['Hash'].lower(),'Runtime hash mismatch'
    assert manifest['prominence']['wavelengthAngstrom']==304,'Missing observed prominence data'
    results['passed']=True
    (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('PASS Sun: photographic source, settings, freeze/audio, palettes, quality, framing and GPU lifecycle')

if __name__=='__main__':main()
