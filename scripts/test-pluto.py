"""Pluto photographic geography, controls, framing, freeze and GPU lifecycle."""
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
import numpy as np

def difference(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)/3

def display(rgb):
 linear=[c/255/12.92 if c/255<=.04045 else ((c/255+.055)/1.055)**2.4 for c in rgb]
 linear=[c*.2 for c in linear];peak=max(max(linear),.00001)
 return [255*(.62*(1-math.exp(-c))+.38*c*(1-math.exp(-peak))/peak)**(1/2.2) for c in linear]

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('executable',type=Path)
 p.add_argument('--output',type=Path,default=ROOT/'artifacts/pluto/regressions');args=p.parse_args()
 results={}
 def capture(name,**options):
  folder=args.output/name
  command=[sys.executable,str(ROOT/'scripts/test_desktop.py'),'--',str(args.executable.resolve()),'--capture-dir',str(folder),'--scene','pluto','--debug-gpu']
  for key,value in {'width':640,'height':360,'frames':1,'flow':0,**options}.items():
   command.append('--'+key.replace('_','-'))
   if value is not True:command.append(str(value))
  run=subprocess.run(command,capture_output=True,text=True,timeout=240)
  if run.returncode:raise RuntimeError(name+': '+run.stdout+'\n'+run.stderr)
  metrics=json.loads((folder/'metrics.json').read_text());assert metrics['sceneSamples']==1,'Redundant MSAA'
  with Image.open(folder/'pluto.png') as im:return im.convert('RGB')
 base=capture('baseline');assert max(hi for lo,hi in base.getextrema())>140,'Blank/dim Pluto'
 for parameter in ('detail','turbulence','density','brightness'):
  change=difference(capture(parameter+'-zero',**{parameter:0}),capture(parameter+'-two',**{parameter:2}))
  assert change>.001,parameter+' has no effect';results[parameter+'Difference255']=change
 assert difference(capture('drift-zero',start_time=120),capture('drift-two',start_time=120,flow=2))>.01,'No camera drift'
 for palette in (1,2,3):assert difference(base,capture('palette-'+str(palette),palette=palette))>.01,'No palette effect'
 for phase in (225,450,675):assert difference(base,capture('phase-'+str(phase),start_time=phase))>1,'No rotation'
 assert difference(base,capture('full-turn',start_time=900))<.1,'Rotation is not 900 seconds'
 assert difference(capture('frozen',movement=0),capture('frozen-later',movement=0,start_time=450,reaction='expressive',strength=1,energy=1))==0,'Movement zero fails to freeze motion/audio'
 assert difference(base,capture('audio',reaction='expressive',strength=1,energy=1))>.001,'No audio response'
 assert difference(base.crop((0,0,100,360)),capture('sky-later',start_time=225,reaction='expressive',strength=1,energy=1).crop((0,0,100,360)))==0,'Stars move/react'
 assert max(hi for lo,hi in capture('black',brightness=0).getextrema())<=1,'Brightness zero leaves light'
 # Compare independently projected photographic source samples across the heart,
 # longitude seam and far side. A low exposure avoids the shared bloom knee.
 with Image.open(ROOT/'assets/pluto/runtime/pluto-color.dds') as im:reference=im.convert('RGB')
 for phase in (0,225,450,675):
  image=capture('geography-'+str(phase),start_time=phase,pluto_probe=1,brightness=.2)
  errors=[];a=-phase*2*math.pi/900;tilt=-math.radians(25)
  for y in range(70,290,7):
   for x in range(230,465,7):
    nx=((x+.5-320)/360-.075)/.375;ny=(-(y+.5-180)/360-.008)/.375
    r2=nx*nx+ny*ny
    if r2>.8:continue
    nz=math.sqrt(1-r2)
    by=math.cos(tilt)*ny-math.sin(tilt)*nz;bz=math.sin(tilt)*ny+math.cos(tilt)*nz
    bx=math.cos(a)*nx+math.sin(a)*bz;bz=-math.sin(a)*nx+math.cos(a)*bz
    u=.5+math.atan2(bx,bz)/(2*math.pi);v=.5-math.asin(by)/math.pi
    rgb=reference.getpixel((int(u*reference.width)%reference.width,min(int(v*reference.height),reference.height-1)))
    errors.extend(abs(c-d) for c,d in zip(image.getpixel((x,y)),display(rgb)))
  error=sum(errors)/len(errors);assert error<9,f'Incorrect geography at {phase}: {error}'
  results['geography'+str(phase)+'MeanError255']=error
 # A white glacier must occupy the initial central region above the equator.
 heart_view=capture('heart',pluto_probe=1)
 heart=heart_view.crop((335,165,365,200));dark=heart_view.crop((285,190,315,215))
 assert sum(ImageStat.Stat(heart).mean)>sum(ImageStat.Stat(dark).mean)+30,'Heart not initially visible'
 for w,h in ((1920,1080),(2560,1080),(720,1280)):
  image=capture(f'framing-{w}x{h}',width=w,height=h,flow=2,start_time=150,pluto_probe=2)
  box=image.convert('L').point(lambda c:255 if c>50 else 0).getbbox()
  assert box and box[0]>8 and box[1]>8 and box[2]<w-8 and box[3]<h-8,'Globe clips viewport'
  assert .70*min(w,h)<box[2]-box[0]<.80*min(w,h),'Unexpected globe size'
  assert abs((box[2]-box[0])-(box[3]-box[1]))<3,'Globe is stretched'
 for quality in ('eco','auto','high'):assert difference(base,capture('quality-'+quality,quality=quality))<.1,'Quality changes geography'
 assert max(hi for lo,hi in capture('no-haze',pluto_probe=3,density=0).crop((250,80,450,280)).getextrema())<=1,'Atmosphere zero leaves haze'
 capture('switch-resize',validate_lifecycle=True,frames=24)
 # Reject probes that target a different planet or have invalid values.
 for options in (['--pluto-probe','4'],['--uranus-probe','1']):
  run=subprocess.run([sys.executable,str(ROOT/'scripts/test_desktop.py'),'--',str(args.executable.resolve()),'--capture-dir',str(args.output/'invalid'),'--scene','pluto',*options],capture_output=True,text=True,timeout=90)
  assert run.returncode!=0,'Invalid capture probe accepted'
 manifest=json.loads((ROOT/'assets/pluto/sources.json').read_text())
 assert manifest['derivation']['dimensions']==[5920,2960],'Unexpected texture resolution'
 # Regressions for the reported coverage-edge line and far-side softness.
 gray=np.asarray(reference,dtype=np.float32).mean(axis=2)/255
 samples=np.asarray(manifest['derivation']['originalCoverageBoundarySamples'])
 xs,ys=samples[:,0],samples[:,1]
 seam_deficits=[]
 for offset in range(-3,4):
  row=np.clip(ys+offset,8,gray.shape[0]-9)
  deficit=np.mean((gray[row-8,xs]+gray[row+8,xs])*.5-gray[row,xs])
  seam_deficits.append(float(deficit))
 assert max(seam_deficits)<.04,'Dark line follows old southern coverage boundary'
 results['coverageEdgeMaxMeanDeficit']=max(seam_deficits)
 with Image.open(ROOT/'assets/pluto/runtime/pluto-color.dds') as texture:
  reduced=np.asarray(texture.convert('RGB').resize((1480,740),Image.Resampling.LANCZOS),dtype=np.float32).mean(axis=2)/255
 def clarity(box):
  x0,y0,x1,y1=box;c=reduced[y0:y1,x0:x1]
  return float(np.abs(np.diff(c,axis=0)).mean()+np.abs(np.diff(c,axis=1)).mean())
 encounter=clarity((440,210,640,430))
 ratios=[clarity(box)/encounter for box in ((20,145,310,440),(1150,145,1460,440))]
 assert all(.65<ratio<3.5 for ratio in ratios),'Far-side clarity does not match encounter-side detail'
 results['farSideClarityRatios']=ratios
 assert np.max(np.abs(gray[:,0]-gray[:,-1]))<.025,'Longitude texture seam'
 raw=(ROOT/'assets/pluto/runtime/pluto-color.dds').read_bytes()
 hashes=json.loads((ROOT/'assets/pluto/runtime-hashes.json').read_text(encoding='utf-8-sig'))
 entry=hashes[0] if isinstance(hashes,list) else hashes
 assert hashlib.sha256(raw).hexdigest()==entry['Hash'].lower(),'Runtime texture hash mismatch'
 results['lifecycleGpuValidation']=True;results['passed']=True
 (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 print('PASS Pluto: source geography/heart, controls, rotation, freeze/audio, stars, framing and GPU lifecycle')
if __name__=='__main__':main()
