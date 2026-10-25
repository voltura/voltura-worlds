"""Cell-centred Poisson multigrid prototype; CPU validation, not app runtime."""
from pathlib import Path
import json
import sys
import numpy as np

def neighbor(a,axis,d,periodic):
 if periodic:return np.roll(a,-d,axis=axis)
 return np.take(a,np.clip(np.arange(a.shape[axis])+d,0,a.shape[axis]-1),axis=axis)
def laplace(p,periodic):
 return sum(neighbor(p,a,d,periodic) for a in range(3) for d in [-1,1])-6*p
def smooth(p,b,count,periodic):
 for _ in range(count):p=p+(laplace(p,periodic)-b)*(.8/6)
 return p
# Laplace has negative diagonal, so p <- p + omega*(L p - b)/6.
def restrict(a):
 n=a.shape[0]//2
 return a.reshape(n,2,n,2,n,2).mean(axis=(1,3,5))
def prolong(a,periodic):
 for axis in range(3):
  shape=list(a.shape);shape[axis]*=2;fine=np.empty(shape,dtype=a.dtype)
  even=[slice(None)]*3;odd=even.copy();even[axis]=slice(0,None,2);odd[axis]=slice(1,None,2)
  fine[tuple(even)]=.75*a+.25*neighbor(a,axis,-1,periodic)
  fine[tuple(odd)]=.75*a+.25*neighbor(a,axis,1,periodic)
  a=fine
 return a
def cycle(p,b,periodic):
 if len(p)<=4:return smooth(p,b,128,periodic)
 p=smooth(p,b,4,periodic)
 rhs=4*restrict(b-laplace(p,periodic));rhs-=rhs.mean()
 error=cycle(np.zeros_like(rhs),rhs,periodic)
 p+=prolong(error,periodic)
 return smooth(p,b,4,periodic)
def exact(b,periodic):
 n=len(b)
 if periodic:
  f=2*np.pi*np.fft.fftfreq(n);lam=2*(np.cos(f)[:,None,None]+np.cos(f)[None,:,None]+np.cos(f)[None,None,:])-6;lam[0,0,0]=1
  h=np.fft.fftn(b)/lam;h[0,0,0]=0
  return np.fft.ifftn(h).real
 q=np.cos(np.pi*(np.arange(n)[:,None]+.5)*np.arange(n)[None,:]/n)*np.sqrt(2/n);q[:,0]/=np.sqrt(2)
 def transform(a,m):
  for axis in range(3):a=np.moveaxis(np.tensordot(m,np.moveaxis(a,axis,0),axes=(1,0)),0,axis)
  return a
 f=np.pi*np.arange(n)/n;lam=2*(np.cos(f)[:,None,None]+np.cos(f)[None,:,None]+np.cos(f)[None,None,:])-6;lam[0,0,0]=1
 h=transform(b,q.T)/lam;h[0,0,0]=0
 return transform(h,q)
def main():
 dtype=np.float32 if '--float32' in sys.argv else np.float64
 out={};rng=np.random.default_rng(182);n=64;z,y,x=np.indices((n,n,n))
 for periodic in [True,False]:
  for name in ['modes','random']:
   pressure=.08*np.cos((2 if periodic else 1)*np.pi*(x+.5)/n)+.02*np.cos((16 if periodic else 8)*np.pi*(y+.5)/n)+.005*(-1.)**(x+y+z)
   b=laplace(pressure,periodic) if name=='modes' else rng.standard_normal((n,n,n));b=b.astype(dtype);b-=b.mean(dtype=np.float64)
   oracle=exact(b.astype(np.float64),periodic);assert np.linalg.norm(laplace(oracle,periodic)-b)/np.linalg.norm(b)<1e-6
   p=np.zeros_like(b);history=[]
   for _ in range(6):
    p=cycle(p,b,periodic);p-=p.mean()
    history.append(dict(residual=float(np.linalg.norm(laplace(p,periodic)-b)/np.linalg.norm(b)),pressureError=float(np.linalg.norm(p-oracle)/np.linalg.norm(oracle))))
   out[('periodic-' if periodic else 'closed-')+name]=history
   assert history[-1]['residual']<.001 and history[-1]['pressureError']<.001
 root=Path('artifacts/revision-182');root.mkdir(exist_ok=True);(root/('cpu-float32-results.json' if dtype==np.float32 else 'cpu-results.json')).write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))
if __name__=='__main__':main()
