"""Audit actual staggered closed-wall velocity readbacks without NumPy."""
from pathlib import Path
import array,json,math,struct,sys
root=Path(sys.argv[1]);raw=(root/'velocity-rgba16f.bin').read_bytes();n=round((len(raw)/8)**(1/3));assert n**3*8==len(raw)
v=list(struct.iter_unpack('<4e',raw));b=array.array('f');b.frombytes((root/'divergence-before-r32f.bin').read_bytes());assert len(b)==n**3
squared=0.;normal=0.;upper=0.
for z in range(n):
 for y in range(n):
  for x in range(n):
   i=(z*n+y)*n+x;div=0.
   for axis,(position,stride) in enumerate([(x,1),(y,n),(z,n*n)]):
    div+=(v[i+stride][axis] if position+1<n else 0)-v[i][axis]
    if position==0:normal=max(normal,abs(v[i][axis]))
    if position==n-1:upper=max(upper,abs(v[i][axis]))
   squared+=div*div
before=math.sqrt(sum(q*q for q in b)/len(b));after=math.sqrt(squared/len(v));mean=sum(b)/len(b)
result={'grid':n,'rhsMean':mean,'rhsRms':before,'postProjectionDivergenceRms':after,'lowerWallNormalMax':normal,'lastInteriorNormalMax':upper}
print(json.dumps(result,indent=2));(root/'boundary-audit.json').write_text(json.dumps(result,indent=2))
assert normal==0,'Normal velocity leaked at stored wall faces'
assert abs(mean)<before*1e-5+1e-10,'Closed-domain divergence is not compatible'
assert after<before*.02+1e-7,'Projection leaves excessive full-domain divergence'
assert upper>1e-7,'Last interior normal faces appear incorrectly clamped'
