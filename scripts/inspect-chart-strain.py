"""Audit native detail-chart deformation in occupied cells without NumPy."""
import array
import json
import math
import struct
import sys
from pathlib import Path
folder=Path(sys.argv[1])
metrics=json.loads((folder/'metrics.json').read_text())
g=metrics['grid'];n=metrics['densityGrid']
if metrics['warmupFrames']!=720 or n!=g*2 or metrics['densityDimensions']!=[n,n,n]:
    raise ValueError('Expected720steps and cubic two-times density layout')
c=array.array('f');c.frombytes((folder/'coordinates-rgba32f.bin').read_bytes())
if sys.byteorder!='little':c.byteswap()
d=(folder/'density-rgba16f.bin').read_bytes()
if len(c)!=g*g*g*2*4 or len(d)!=n*n*n*8 or not all(math.isfinite(v) for v in c):
    raise ValueError('Invalid chart/density readback')
def point(x,y,z,chart):
    i=((z*g+y)*g*2+x+chart*g)*4
    return c[i:i+3]
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def dot(a,b):return sum(x*y for x,y in zip(a,b))
strains=[[],[]];inverted=[0,0]
for z in range(2,g-2,2):
 for y in range(2,g-2,2):
  for x in range(2,g-2,2):
   concentration=sum(struct.unpack_from('<e',d,(((z*2+dz)*n+y*2+dy)*n+x*2+dx)*8+6)[0]
                     for dz in range(2) for dy in range(2) for dx in range(2))/8
   if concentration<=.012:continue
   for chart in range(2):
    derivatives=[]
    for axis in range(3):
     a=[x,y,z];b=[x,y,z];a[axis]+=1;b[axis]-=1
     derivatives.append(tuple((v-w)*g*.5 for v,w in zip(point(*a,chart),point(*b,chart))))
    dx,dy,dz=derivatives
    cx,cy,cz=cross(dy,dz),cross(dz,dx),cross(dx,dy)
    determinant=dot(dx,cx)
    strain=max(0,sum(dot(v,v) for v in derivatives)+sum(dot(v,v) for v in [cx,cy,cz])/max(determinant*determinant,1e-8)-6)
    strains[chart].append(strain);inverted[chart]+=determinant<=0
report=[]
for chart in range(2):
 values=sorted(strains[chart]);count=len(values)
 if not count:raise ValueError('No occupied chart samples')
 report.append({'sampleCount':count,'medianStrain':values[count//2],
                'fractionAboveThreshold':sum(v>=4.5 for v in values)/count,
                'fractionNonpositiveDeterminant':inverted[chart]/count})
(folder/'chart-strain.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if '--check' in sys.argv and not (report[1]['fractionAboveThreshold']<.35 and report[1]['fractionNonpositiveDeterminant']<.003):
    raise SystemExit('Active detail chart became excessively distorted before renewal')
