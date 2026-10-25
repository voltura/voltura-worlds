"""Native multigrid comparison with independent FFT/cosine pressure oracles."""
from pathlib import Path
import importlib.util,json,subprocess,sys
import numpy as np
spec=importlib.util.spec_from_file_location('mg',Path(__file__).with_name('probe-pressure-multigrid.py'));mg=importlib.util.module_from_spec(spec);spec.loader.exec_module(mg)
r=Path('artifacts/revision-183');r.mkdir(exist_ok=True);out={};rng=np.random.default_rng(183)
exe=sys.argv[1] if len(sys.argv)>1 else 'build/Release/worlds_pressure_gpu_tests.exe'
shader=sys.argv[2] if len(sys.argv)>2 else 'build/shaders/smoke_pressure_probe.cso'
for n in [32,64,128]:
 z,y,x=np.indices((n,n,n))
 for periodic in [True,False]:
  for kind in ['modes','random']:
   name=f'{n}-'+('periodic' if periodic else 'closed')+'-'+kind
   pressure=.08*np.cos((2 if periodic else 1)*np.pi*(x+.5)/n)+.02*np.cos((16 if periodic else 8)*np.pi*(y+.5)/n)+.005*(-1.)**(x+y+z)
   b=mg.laplace(pressure,periodic) if kind=='modes' else rng.standard_normal((n,n,n));b-=b.mean();b=b.astype('<f4');b.tofile(r/(name+'-rhs.bin'))
   subprocess.run([exe,shader,str(r/(name+'-rhs.bin')),str(r/(name+'-pressure.bin')),str(n),str(int(periodic)),'6'],check=True)
   actual=np.fromfile(r/(name+'-pressure.bin'),dtype='<f4').reshape(n,n,n).astype(np.float64);actual-=actual.mean()
   oracle=mg.exact(b.astype(np.float64),periodic)
   residual=float(np.linalg.norm(mg.laplace(actual,periodic)-b)/np.linalg.norm(b));error=float(np.linalg.norm(actual-oracle)/np.linalg.norm(oracle))
   out[name]={'residual':residual,'pressureError':error};print(name,out[name],flush=True)
   assert residual<.001 and error<.001,(name,out[name])
(r/'gpu-results.json').write_text(json.dumps(out,indent=2))
