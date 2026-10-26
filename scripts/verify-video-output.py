"""Check native video crop readbacks and exactly-once display orientation."""
from pathlib import Path
import json,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
from PIL import Image, ImageChops, ImageStat
out=ROOT/'artifacts/broader-video/contracts'
report=json.loads((out/'capabilities.json').read_text())
results={}
for name,expected in [('left',(254,0,0)),('right',(255,255,0)),('top',(254,0,0)),('bottom',(255,255,0))]:
 image=Image.open(out/(name+'.png')).convert('RGB');w,h=image.size
 mean=ImageStat.Stat(image.crop((w//2-4,h//2-4,w//2+4,h//2+4))).mean
 assert max(abs(a-b) for a,b in zip(mean,expected))<15,(name,mean)
 results[name]=mean
fit=Image.open(out/'fit.png').convert('RGB');assert max(fit.getpixel((fit.width//2,0)))<=1,'Fit bar is not black'
base=Image.open(out/'h264.mp4.png').convert('RGB')
for degree in [90,180,270]:
 actual=Image.open(out/f'rotate-{degree}.mov.png').convert('RGB')
 expected=base.rotate(degree,expand=True).resize(actual.size)
 error=sum(ImageStat.Stat(ImageChops.difference(expected,actual)).mean)/3
 assert error<12,(degree,error)
 results[f'rotation{degree}MeanError255']=error
 expected_aspect=180/320 if degree!=180 else 320/180
 assert abs(report[f'rotate-{degree}.mov']['displayAspect']-expected_aspect)<1e-4
assert abs(report['sar.mp4']['displayAspect']-32/9)<1e-4,'Nonsquare pixels ignored'
(out/'pixel-checks.json').write_text(json.dumps(results,indent=2))
print('PASS: native left/right/top/bottom crop extremes, Fit bars, exactly-once rotation and nonsquare pixels')
