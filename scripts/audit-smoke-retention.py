"""CPU transport audit from native field readbacks; frozen flow, not a live GPU benchmark."""
import argparse,hashlib,json,time
from pathlib import Path
import numpy as np
p=argparse.ArgumentParser();p.add_argument('capture',type=Path);p.add_argument('--output',type=Path,required=True)
p.add_argument('--size',type=int,default=128);p.add_argument('--steps',type=int,default=12)
p.add_argument('--unbounded',action='store_true',help='Diagnostic only: bypass enclosing-cell limiter')
p.add_argument('--flow-scale',type=float,default=1)
p.add_argument('--frequencies',type=int,nargs='+',default=[8,24]);args=p.parse_args()
if args.size<8 or args.size>256 or args.steps<1 or args.steps>120 or not 0<=args.flow_scale<=4 or any(f<1 or f>=args.size/2 for f in args.frequencies):p.error('Invalid audit size, steps, flow scale or frequency')
meta=json.loads((args.capture/'metrics.json').read_text());g=meta['grid'];n=args.size
velocity=np.fromfile(args.capture/'velocity-rgba16f.bin',dtype='<f2').astype(np.float32).reshape(g,g,g,4)
dims=meta['densityDimensions'];detail=np.fromfile(args.capture/'detail-velocity-rgba32f.bin',dtype='<f4').reshape(dims[2],dims[1],dims[0],4)
assert np.isfinite(velocity).all() and np.isfinite(detail).all()
z,y,x=np.mgrid[:n,:n,:n].astype(np.float32);points=np.stack([(x+.5)/n,(y+.5)/n,(z+.5)/n],axis=-1)
def linear(field,pos):
 size=np.array(field.shape[:3][::-1],dtype=np.float32);q=pos*size-.5;b=np.floor(q).astype(np.int32);f=q-b
 result=np.zeros(pos.shape[:-1],dtype=np.float32)
 for iz in range(2):
  for iy in range(2):
   for ix in range(2):
    w=(f[...,0] if ix else 1-f[...,0])*(f[...,1] if iy else 1-f[...,1])*(f[...,2] if iz else 1-f[...,2])
    result+=field[np.clip(b[...,2]+iz,0,field.shape[0]-1),np.clip(b[...,1]+iy,0,field.shape[1]-1),np.clip(b[...,0]+ix,0,field.shape[2]-1)]*w
 return result
def flow(pos):
 value=np.empty_like(pos)
 for c in range(3):
  offset=np.zeros(3,dtype=np.float32);offset[c]=.5/g
  value[...,c]=linear(velocity[...,c],pos+offset)+linear(detail[...,c],pos)
 return value*args.flow_scale
v=flow(points);maps=[]
for sign in [1,-1]:
 midpoint=points-v*(sign/120)
 q=(points-flow(midpoint)*(sign/60))*n-.5
 b=np.floor(q).astype(np.int32);f=(q-b).astype(np.float32);f2=f*f;f3=f2*f
 weights=[-.5*f+f2-.5*f3,1-2.5*f2+1.5*f3,.5*f+2*f2-1.5*f3,-.5*f2+.5*f3]
 indices=[[np.clip(b[...,axis]+k-1,0,n-1) for k in range(4)] for axis in range(3)]
 maps.append((weights,indices))
del v,midpoint,q,b,f,f2,f3
mask=(x>n*.15)&(x<n*.85)&(y>n*.15)&(y<n*.85)&(z>n*.15)&(z<n*.85)
def advect(a,mapping):
 w,idx=mapping;out=np.zeros_like(a);lo=np.full_like(a,np.inf);hi=np.full_like(a,-np.inf)
 for iz in range(4):
  plane=np.zeros_like(a)
  for iy in range(4):
   row=np.zeros_like(a)
   for ix in range(4):
    sample=a[idx[2][iz],idx[1][iy],idx[0][ix]]
    row+=sample*w[ix][...,0]
    if ix in [1,2] and iy in [1,2] and iz in [1,2]:np.minimum(lo,sample,out=lo);np.maximum(hi,sample,out=hi)
   plane+=row*w[iy][...,1]
  out+=plane*w[iz][...,2]
 return (out if args.unbounded else np.clip(out,lo,hi)).astype(np.float16).astype(np.float32)
results=[]
for freq in args.frequencies:
 start=time.monotonic();initial=(.5+.4*np.cos(points[...,0]*(2*np.pi*freq))).astype(np.float16).astype(np.float32);a=initial.copy()
 for direction in range(2):
  for step in range(args.steps):a=advect(a,maps[direction])
 source=initial[mask]-.5;final=a[mask]-.5
 item={'minimum':float(a.min()),'maximum':float(a.max()),'cyclesPerDomain':freq,'cellsPerCycle':n/freq,'roundtripCorrelationAmplitude':float(np.dot(source,final)/np.dot(source,source)),
 'roundtripRelativeL2':float(np.linalg.norm(final-source)/np.linalg.norm(source)),'elapsedCpuSeconds':time.monotonic()-start}
 results.append(item);print(json.dumps(item),flush=True)
report={'evidence':'CPU Catmull-Rom and FP16 replay using frozen native staggered velocity plus detail; not live simulation, performance, or physical-display proof',
 'sourceHashes':{name:hashlib.sha256((args.capture/name).read_bytes()).hexdigest() for name in ['velocity-rgba16f.bin','detail-velocity-rgba32f.bin']},
 'evaluationRegion':'Central cube: cell indices strictly between 0.15N and 0.85N; includes low-density cells', 'bounded':not args.unbounded,'grid':n,'flowScale':args.flow_scale,'stepsEachDirection':args.steps,'secondsEachDirection':args.steps/60,'results':results}
args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2))
