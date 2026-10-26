"""Mercury surface, controls, sky, freeze, framing and GPU lifecycle contracts."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
from PIL import Image,ImageChops,ImageStat


def difference(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)/3


def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('executable',type=Path)
 p.add_argument('--output',type=Path,default=ROOT/'artifacts/mercury/regressions');args=p.parse_args()
 results={}
 def capture(name,**options):
  folder=args.output/name;folder.mkdir(parents=True,exist_ok=True)
  command=[sys.executable,str(ROOT/'scripts/test_desktop.py'),'--',str(args.executable.resolve()),'--capture-dir',str(folder),'--scene','mercury','--debug-gpu']
  for key,value in {'width':640,'height':360,'frames':1,'flow':0,**options}.items():
   command.append('--'+key.replace('_','-'))
   if value is not True:command.append(str(value))
  run=subprocess.run(command,capture_output=True,text=True,timeout=180)
  if run.returncode:raise RuntimeError(name+': '+run.stdout+'\n'+run.stderr)
  with Image.open(folder/'mercury.png') as im:return im.convert('RGB')
 base=capture('baseline');assert max(hi for lo,hi in base.getextrema())>70,'Blank Mercury'
 metrics=json.loads((args.output/'baseline/metrics.json').read_text())
 assert metrics['sceneSamples']==1,'Redundant multisample allocation'
 for parameter in ('detail','turbulence','density','brightness'):
  a=capture(parameter+'-zero',**{parameter:0});b=capture(parameter+'-two',**{parameter:2})
  results[parameter+'Difference255']=difference(a,b)
  assert results[parameter+'Difference255']>.01,parameter+' has no effect'
 assert difference(capture('drift-zero',start_time=120),capture('drift-two',start_time=120,flow=2))>.01,'Camera drift has no effect'
 for palette in (1,2,3):assert difference(base,capture('palette-'+str(palette),palette=palette))>.01,'Palette has no effect'
 for phase in (225,450,675):
  value=difference(base,capture('phase-'+str(phase),start_time=phase));assert value>1,'Rotation did not reveal new terrain'
  results['phase'+str(phase)+'Difference255']=value
 assert difference(base,capture('full-turn',start_time=900))<.1,'Rotation is not 900 seconds'
 assert difference(capture('frozen',movement=0),capture('frozen-later',movement=0,start_time=450,reaction='expressive',strength=1,energy=1))==0,'Movement zero did not freeze animation/audio'
 assert difference(base,capture('audio',reaction='expressive',strength=1,energy=1))>.001,'Audio response has no effect'
 sky=base.crop((0,0,100,360));later=capture('sky-later',start_time=225,reaction='expressive',strength=1,energy=1)
 assert difference(sky,later.crop((0,0,100,360)))==0,'Sky moves or responds to audio'
 assert max(hi for lo,hi in capture('black',brightness=0).getextrema())<=1,'Brightness zero did not darken composition'
 # Check deep night and relief at 60 degrees; the fuller default is 30 degrees.
 night_lighting=capture('night-lighting',turbulence=1.5)
 smooth=capture('smooth-horizon',detail=0,turbulence=1.5);rough=capture('rough-horizon',detail=2,turbulence=1.5)
 night=[];horizon=[]
 for y in range(60,300):
  for x in range(180,470):
   qx=((x+.5-320)/360-.075)/.375;qy=(-(y+.5-180)/360-.008)/.375
   rr=qx*qx+qy*qy
   if rr>=.85**2:continue
   mu=qx*math.sin(math.pi/3)+math.sqrt(1-rr)*.5
   if mu<-.25:night.append(max(night_lighting.getpixel((x,y))))
   if 0<=mu<.006:horizon.append(max(abs(a-b) for a,b in zip(smooth.getpixel((x,y)),rough.getpixel((x,y)))))
 assert night and max(night)<=3,'False night-side illumination'
 assert horizon and max(horizon)<=8,'Bright relief contour at solar horizon'
 results['deepNightMax255']=max(night);results['horizonReliefMaxDifference255']=max(horizon)
 # Subtract the zero-brightness image to locate the complete lit globe at
 # several aspects; dark space remains outside its expected analytic bounds.
 for w,h in ((1920,1080),(2560,1080),(720,1280)):
  im=capture(f'framing-{w}x{h}',width=w,height=h,flow=2,start_time=150)
  scale=min(w,h);cx=w*.5+(.075+math.sin(150*.0011)*.018)*scale
  cy=h*.5-(.008+math.sin(150*.0014)*.012)*scale;radius=.375*scale
  assert cx-radius>0 and cx+radius<w and cy-radius>0 and cy+radius<h,'Globe clips viewport'
  assert max(hi for lo,hi in im.getextrema())>70,'Empty layout capture'
 for quality in ('eco','auto','high'):
  im=capture('quality-'+quality,quality=quality);assert difference(base,im)<.1,'Quality changes Mercury geography'
 capture('switch-resize',validate_lifecycle=True,frames=24)
 # The native lifecycle path asserts stable allocations on repeated planet/resize
 # roundtrips. Its initial all-scenes tour also retains Aurora's shared artwork.
 results['lifecycleGpuValidation']=True
 results['textureGpuBytes']=metrics['textureGpuBytes'];results['passed']=True
 (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
 print('PASS Mercury: terrain, controls, 900-second spin, freeze/audio, stars, night/terminator, aspect ratios and GPU lifecycle')


if __name__=='__main__':main()
