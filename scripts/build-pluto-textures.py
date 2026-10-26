"""Reproduce Pluto's photographic BC7 input from the New Horizons PIA11707 map."""
import hashlib
import json
from pathlib import Path
import sys
import urllib.request
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
import numpy as np
from PIL import Image
URL='https://d2pn8kiwq2w21t.cloudfront.net/original_images/jpegPIA11707.jpg'
SHA256='e36f3daa1b3b0e911418ff830ddccf8b63e10a082c6ccc96eb7d8170991962d6'
SIZE=(5920,2960) # Nearly native resolution, divisible by BC7's four-pixel blocks.
def main():
 cache=ROOT/'.deps/pluto';cache.mkdir(parents=True,exist_ok=True)
 source=cache/'source.jpg'
 if not source.exists():
  temporary=source.with_suffix('.part');urllib.request.urlretrieve(URL,temporary);temporary.replace(source)
 if hashlib.sha256(source.read_bytes()).hexdigest()!=SHA256:raise ValueError('Unexpected Pluto source hash')
 with Image.open(source) as im:
  if im.size!=(5926,2963):raise ValueError('Unexpected Pluto source grid')
  rgb=np.asarray(im.convert('RGB')).copy()
 # Preserve every observed source pixel. Only the unobserved southern cap
 # receives a synthetic photographic continuation, at the original pixel scale.
 # Reflect the observed rows across each column's coverage boundary: this
 # retains the source's local frequency/detail instead of blurring the image
 # or extending a featureless flat color. This is not measured polar geography.
 observed=rgb.max(axis=2)>12
 height,width=observed.shape
 boundary=np.where(observed,np.arange(height)[:,None],0).max(axis=0)
 y=np.arange(height)[:,None]
 missing=y>boundary[None,:]
 donor=np.clip(2*boundary[None,:]-y+1,0,height-1)
 extension=rgb[donor,np.arange(width)[None,:]].astype(np.float32)
 # Converge the final 16 rows to one average ice color at the exact sphere pole
 # to prevent a longitude pinwheel. All other generated pixels keep native detail.
 polar=np.clip((y-(height-17))/16,0,1)
 pole=extension[-17,:,:].mean(axis=0)
 extension=extension*(1-polar[:,:,None])+pole*polar[:,:,None]
 rgb=np.where(missing[:,:,None],np.uint8(np.clip(extension,0,255)+.5),rgb)
 Image.fromarray(rgb).resize(SIZE,Image.Resampling.LANCZOS).save(cache/'pluto-color.png')
 output=ROOT/'assets/pluto';output.mkdir(parents=True,exist_ok=True)
 manifest={'sources':{'PIA11707':{'url':URL,'page':'https://www.jpl.nasa.gov/images/pia11707-pluto-color-map/','sha256':SHA256,'dimensions':[5926,2963]}},
 'derivation':{'version':1,'dimensions':list(SIZE),'mapping':'north up, equirectangular; map center faces viewer at startup; 25 degree north observer',
 'surface':'Published MVIC photographic color mosaic interpreted as sRGB; not calibrated albedo or measured elevation. Unequal spatial resolution retained.',
 'unobserved':'Only pixels south of each source coverage boundary are reconstructed by reflecting observed rows at native pixel scale. No blur or smoothing of observed pixels. Final 16 polar rows converge to mean ice color. Synthetic continuation, not observed terrain.',
 'pillow':Image.__version__,'numpy':np.__version__}}
 (output/'sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('Prepared 5920x2960 Pluto photographic map; sharp photographic continuation in unobserved south cap')
if __name__=='__main__':main()
