"""Frozen-volume shape diagnostic, not a live fluid or production asset.

Preserves simulated pigment proportions while testing multiscale density erosion.
NumPy only. X-fastest RGBA16F input/output; deterministic seed and bounded slabs.
"""
import argparse
import json
from pathlib import Path
import numpy as np

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('output', type=Path)
p.add_argument('--size', type=int, default=192)
p.add_argument('--erosion', type=float, default=.12)
a = p.parse_args()
source = np.fromfile(a.source, dtype='<f2').reshape(128, 128, 128, 4).astype(np.float32)
assert np.isfinite(source).all() and 24 <= a.size <= 384
rng = np.random.default_rng(149)
tiles = [rng.random((32, 32, 32), dtype=np.float32) for _ in range(4)]

def trilinear(field, coords, periodic=False, smooth=False):
    c = np.floor(coords).astype(np.int32)
    f = coords - c
    if smooth:
        f = f*f*f*(f*(f*6-15)+10)
    result = np.zeros(coords.shape[:-1] + field.shape[3:], np.float32)
    for z in range(2):
        for y in range(2):
            for x in range(2):
                ix = c + (x,y,z)
                ix = ix % field.shape[0] if periodic else np.clip(ix,0,field.shape[0]-1)
                w = np.prod(np.where(np.array((x,y,z)),f,1-f),axis=-1)
                if field.ndim == 4:
                    w = w[...,None]
                result += field[ix[...,2],ix[...,1],ix[...,0]]*w
    return result

a.output.parent.mkdir(parents=True, exist_ok=True)
mass_before = mass_after = 0.
with a.output.open('wb') as output:
    for start in range(0,a.size,8):
        z,y,x = np.meshgrid((np.arange(start,min(start+8,a.size))+.5)/a.size,
                            (np.arange(a.size)+.5)/a.size,
                            (np.arange(a.size)+.5)/a.size,indexing='ij')
        q = np.stack((x,y,z),axis=-1).astype(np.float32)
        d = trilinear(source,q*128-.5)
        # Irregular random lattice rather than sinusoidal stripes. This is
        # a shape probe, not a physically transported turbulence spectrum.
        warp = np.stack([trilinear(tiles[i],q*9 + i*7.37,True,True)-.5
                         for i in range(3)],axis=-1)
        pos = q + warp*.055
        detail = sum(weight*(trilinear(tiles[i],pos*freq + i*3.71,True,True)-.5)
                     for i,(freq,weight) in enumerate(((19,1.),(37,.5),(71,.25),(127,.125))))
        density = d[...,3].copy()
        # Shift is absolute concentration, leaving dense cores intact while
        # breaking broad low-density sheets into pockets and rounded boundaries.
        shaped = np.maximum(0,density + a.erosion*(detail-.16))
        shaped = np.minimum(shaped,density*3)
        d *= (shaped/np.maximum(density,1e-12))[...,None]
        assert np.isfinite(d).all() and (d>=0).all()
        mass_before += float(density.sum(dtype=np.float64))
        mass_after += float(shaped.sum(dtype=np.float64))
        d.astype('<f2').tofile(output)
report = {'size':a.size,'erosion':a.erosion,'seed':149,'source':str(a.source),
          'massRatio':mass_after/max(mass_before,1e-12),
          'evidence':'Frozen density-shape diagnostic; no temporal or live-fluid claim.'}
a.output.with_suffix('.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report))
