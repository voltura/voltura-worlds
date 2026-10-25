"""Native Saturn geometry, shadows, transparency, controls and framing contracts."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
import numpy as np
from PIL import Image,ImageChops,ImageStat

def difference(a,b):
    return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)/3

def geometry(w,h,elevation=25,phase=0,flow=0):
    # Independent orthographic ray/ellipsoid and ray/plane intersections.
    e=np.radians(elevation);roll=np.radians(15);R=2.322
    ext=np.maximum(R*np.sqrt([np.cos(roll)**2+(np.sin(roll)*np.sin(e))**2,np.sin(roll)**2+(np.cos(roll)*np.sin(e))**2]),1)+.035
    scale=min(.425*w/min(w,h)/ext[0],.425*h/min(w,h)/ext[1])
    y,x=np.mgrid[:h,:w];q=np.stack(((x+.5-w/2)/min(w,h)/scale,-(y+.5-h/2)/min(w,h)/scale,np.full((h,w),4.)),axis=-1)
    angle=phase*2*np.pi/900
    q[...,:2]-=np.array([np.sin(angle),np.sin(2*angle)])*.015*flow
    rx=np.array([[1,0,0],[0,np.cos(e),np.sin(e)],[0,-np.sin(e),np.cos(e)]])
    rz=np.array([[np.cos(roll),np.sin(roll),0],[-np.sin(roll),np.cos(roll),0],[0,0,1]])
    rotation=rx@rz;o=q@rotation.T;d=rotation@np.array([0,0,-1.])
    m=np.array([1,(60268/54364)**2,1]);a=sum(d*d*m);b=(o*d*m).sum(-1);c=(o*o*m).sum(-1)-1
    disc=b*b-a*c;t=(-b-np.sqrt(np.maximum(disc,0)))/a
    rt=-o[...,1]/d[1];rp=o+rt[...,None]*d;r=np.linalg.norm(rp[...,[0,2]],axis=-1)
    return disc,t,rt,r,o,d

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('executable',type=Path);parser.add_argument('--output',type=Path,default=ROOT/'artifacts/saturn/regressions');args=parser.parse_args()
    results={}
    def capture(name,**options):
        folder=args.output/name;folder.mkdir(parents=True,exist_ok=True)
        cmd=[sys.executable,str(ROOT/'scripts/test_desktop.py'),'--',str(args.executable.resolve()),'--capture-dir',str(folder),'--scene','saturn','--debug-gpu']
        for key,value in {'width':960,'height':540,'frames':1,'flow':0,**options}.items():
            cmd+=['--'+key.replace('_','-')]
            if value is not True:cmd+=[str(value)]
        run=subprocess.run(cmd,capture_output=True,text=True,timeout=300)
        if run.returncode:
            error=args.executable.resolve().parent/'capture-error.txt'
            raise RuntimeError(name+': '+run.stdout+run.stderr+(error.read_text() if error.exists() else ''))
        with Image.open(folder/'saturn.png') as image:return image.convert('RGB')
    base=capture('baseline');assert np.asarray(base).max()>80,'Blank Saturn'
    assert np.asarray(capture('black',brightness=0)).max()<=1,'Brightness zero leaks light'
    assert difference(capture('frozen',movement=0),capture('frozen-later',movement=0,start_time=450,reaction='expressive',strength=1,energy=1))==0,'Movement zero does not freeze'
    assert difference(base,capture('audio',reaction='expressive',strength=1,energy=1))>.01,'Audio has no effect'
    for control in ('detail','flow','turbulence','density'):
        delta=difference(capture(control+'-zero',start_time=225,**{control:0}),capture(control+'-two',start_time=225,**{control:2}))
        assert delta>.001,control+' has no independent effect';results[control+'Difference255']=delta
    for palette in (1,2,3):assert difference(base,capture('palette-'+str(palette),palette=palette))>.01
    assert difference(base,capture('full-turn',start_time=900))<.1,'900-second wrap does not close'
    for quality in ('eco','high'):assert difference(base,capture('quality-'+quality,quality=quality))==0
    disc,t,rt,r,o,d=geometry(960,540)
    globe=np.asarray(capture('globe-mask',saturn_probe=1))[...,0]
    assert np.mean(globe[disc>.03]>50)>.999,'Globe shape differs from physical ellipsoid'
    assert globe[disc<-.03].max()==0,'Globe extends beyond ellipsoid'
    ring=np.asarray(capture('ring-mask',saturn_probe=2))[...,0]
    occluded=(disc>.03)&(rt>t)&(r>1.3)&(r<2.2)
    assert occluded.sum()>100 and ring[occluded].max()==0,'Back rings show through globe'
    foreground=(disc>.03)&(rt<t)&(r>1.6)&(r<1.9)
    assert foreground.sum()>100 and np.mean(ring[foreground]>50)>.99,'Front rings missing over globe'
    outside=disc<-.03
    gap=outside&(r>1.97)&(r<2.01);belt=outside&(r>1.6)&(r<1.9)
    assert ring[gap].max()==0 and ring[belt].mean()>70,'Cassini Division or B ring incorrect'
    # Independently trace sunlight in Saturn's equatorial frame.
    light=np.array([-.45,.35,1.]);light/=np.linalg.norm(light)
    surface=o+t[...,None]*d
    crossing=surface-light*(surface[...,1]/light[1])[...,None]
    shadowRadius=np.linalg.norm(crossing[...,[0,2]],axis=-1)
    globeShadowExpected=(disc>.03)&(surface[...,1]<-.03)&(shadowRadius>1.6)&(shadowRadius<1.9)
    ringPosition=o+rt[...,None]*d
    metric=np.array([1,(60268/54364)**2,1])
    la=sum(light*light*metric);lb=(ringPosition*light*metric).sum(-1);lc=(ringPosition*ringPosition*metric).sum(-1)-1
    ld=lb*lb-la*lc;lt=(-lb-np.sqrt(np.maximum(ld,0)))/la
    ringShadowExpected=outside&(r>1.6)&(r<1.9)&(ld>.02)&(lt>0)
    for name,probe in [('globe-shadow',3),('ring-shadow',4)]:
        image=np.asarray(capture(name,saturn_probe=probe))[...,0]
        assert (image>25).sum()>100,name+' missing'
        expected=globeShadowExpected if probe==3 else ringShadowExpected
        assert expected.sum()>20 and np.mean(image[expected]>25)>.99,name+' differs from independent light intersection'
    sky=capture('sky',saturn_probe=6)
    assert difference(sky,capture('sky-later',saturn_probe=6,start_time=225,reaction='expressive',strength=1,energy=1))==0,'Stars move or react to audio'
    # Transparent rings must attenuate the exact same sky, without emission.
    attenuation=np.asarray(capture('sky-attenuation',saturn_probe=5),dtype=float)
    assert attenuation.max()>0,'No sky transmission through rings'
    assert np.all(attenuation<=np.asarray(sky,dtype=float)+1),'Rings add emission in transmission probe'
    translucent=outside&(r>1.3)&(r<1.45)
    skyValues=np.asarray(sky,dtype=float)[translucent]
    transmitted=attenuation[translucent]
    assert np.any((transmitted>2)&(transmitted<skyValues-2)),'C ring does not partially transmit background stars'
    for elevation in (0,.01,89.99,90):capture('elevation-'+str(elevation),saturn_elevation=elevation)
    for w,h in ((1920,1080),(3440,1440),(1080,1080),(720,1280)):
        for phase in (0,225):
            options=dict(width=w,height=h,flow=2,start_time=phase)
            capture(f'layout-{w}x{h}-{phase}',**options)
            mask=np.maximum(np.asarray(capture(f'layout-globe-{w}x{h}-{phase}',saturn_probe=1,**options))[...,0],np.asarray(capture(f'layout-ring-{w}x{h}-{phase}',saturn_probe=2,**options))[...,0])
            yy,xx=np.where(mask>5)
            assert xx.min()>=w*.075-2 and xx.max()<=w*.925+2 and yy.min()>=h*.075-2 and yy.max()<=h*.925+2,'Composition exceeds 85% safe frame'
    capture('lifecycle',validate_lifecycle=True)
    metrics=json.loads((args.output/'baseline/metrics.json').read_text());assert metrics['sceneSamples']==1
    results.update(passed=True,estimatedGpuBytes=metrics['estimatedGpuBytes'])
    (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('PASS Saturn: ellipsoid, ring occlusion/gaps, shadows, controls, sky transmission, grazing angles, framing and lifecycle')
if __name__=='__main__':main()
