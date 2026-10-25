"""Build a deterministic band-limited 3D vector-potential prototype.

Requires NumPy (available in the installed Blender Python). This is Fourier
band-limited noise, not a wavelet decomposition or a completed turbulence model.
Output: 64^3 RGBA16F, X fastest; RGB potential, A zero. No external artwork.
"""
import json
from pathlib import Path
import sys
import numpy as np

out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
n = 64
seed = 20261023
rng = np.random.default_rng(seed)
frequencies = np.fft.fftfreq(n)
z, y, x = np.meshgrid(frequencies, frequencies, np.fft.rfftfreq(n), indexing='ij')
radius = np.sqrt(x*x+y*y+z*z)
low, high = .125, .25
inside = (radius > low) & (radius < high)
window = np.zeros_like(radius)
window[inside] = np.sin(np.pi*(radius[inside]-low)/(high-low))**2
channels = []
for channel in range(3):
    spectrum = np.fft.rfftn(rng.normal(size=(n,n,n))) * window
    channels.append(np.fft.irfftn(spectrum, s=(n,n,n), axes=(0,1,2)))
potential = np.stack(channels, axis=-1)

def derivative(field, axis):
    return (np.roll(field,-1,axis=axis)-np.roll(field,1,axis=axis))*.5

def curl(a):
    return np.stack((derivative(a[...,2],1)-derivative(a[...,1],0),
                     derivative(a[...,0],0)-derivative(a[...,2],2),
                     derivative(a[...,1],2)-derivative(a[...,0],1)), axis=-1)

velocity = curl(potential)
potential /= np.sqrt(np.mean(np.sum(velocity*velocity,axis=-1)))
packed = np.zeros((n,n,n,4),dtype='<f2')
packed[...,:3] = potential
packed.tofile(out/'potential-64-rgba16f.bin')
# Audit the quantized asset, not just the ideal pre-quantization spectrum.
decoded = packed[...,:3].astype(np.float64)
velocity = curl(decoded)
divergence = derivative(velocity[...,0],2)+derivative(velocity[...,1],1)+derivative(velocity[...,2],0)
power = np.zeros_like(radius)
for channel in range(3):
    power += np.abs(np.fft.rfftn(decoded[...,channel]))**2
# rFFT omits the conjugate half except the DC/Nyquist planes.
weights = np.ones_like(radius)
weights[:,:,1:-1] = 2
power *= weights
leakage = float(power[~inside].sum()/power.sum())
report = {'size':n, 'seed':seed, 'potentialBandCyclesPerVoxel':[low,high],
          'outsideBandPowerFractionAfterFP16':leakage,
          'velocityVectorRMS':float(np.sqrt(np.mean(np.sum(velocity*velocity,axis=-1)))),
          'discreteDivergenceRMS':float(np.sqrt(np.mean(divergence*divergence))),
          'byteLength':int(packed.nbytes),
          'evidence':'Offline periodic discrete field only; does not prove GPU interpolation, deformed spectrum, temporal behavior, or artwork quality.'}
assert np.isfinite(decoded).all()
assert leakage < 1e-6
assert .999 < report['velocityVectorRMS'] < 1.001
assert report['discreteDivergenceRMS'] < 1e-12
(out/'potential-report.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
