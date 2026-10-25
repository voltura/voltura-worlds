"""Estimate dense versus brick-atlas density storage from native RGBA16F dumps.

This is a planning measurement, not a GPU allocation or visual-quality claim.
Halo expansion is at brick granularity and includes all 26 neighboring bricks.
"""
import argparse
import json
from pathlib import Path
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('input', type=Path)
parser.add_argument('--grid', type=int, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
n = args.grid
data = np.fromfile(args.input, dtype='<f2')
if data.size != n**3*4:
    raise ValueError('Input must contain exactly grid cubed RGBA16F values')
data = data.reshape(n,n,n,4)
if not np.isfinite(data).all() or (data < 0).any():
    raise ValueError('Density must be finite and nonnegative')
rows=[]
for threshold in (0, .0001, .001, .012):
    occupied = data[...,3] > threshold
    for brick in (4,8,16):
        if n % brick:
            continue
        m=n//brick
        active=occupied.reshape(m,brick,m,brick,m,brick).any(axis=(1,3,5))
        padded=np.pad(active,1)
        halo=np.zeros_like(active)
        for z in range(3):
            for y in range(3):
                for x in range(3):
                    halo |= padded[z:z+m,y:y+m,x:x+m]
        count=int(halo.sum())
        rows.append(dict(threshold=threshold,brick=brick,
            activeBricks=int(active.sum()),haloBricks=count,totalBricks=m**3,
            haloFraction=count/m**3,
            # Estimate density ping-pong plus light, all RGBA16F. Separate
            # coarse velocity/pressure, atlas lookup, alignment, and staging
            # are deliberately excluded and must be budgeted independently.
            densityAndLightBytes={str(scale):count*(brick*scale)**3*8*3
                                  for scale in (1,2,4)}))
result=dict(input=str(args.input),grid=n,denseDensityAndLightBytes=n**3*8*3,
            rows=rows, caveat='Frozen occupancy estimate; not an allocation guarantee. '
            'Positive thresholds discard low density. Future motion may expand occupancy.')
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(result,indent=2))
