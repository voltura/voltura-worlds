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
 # Imagegen reconstruction supplies resolved far-side texture and a continuous
 # southern cap. Preserve the original high-resolution encounter/heart region.
 reconstruction=ROOT/'assets/pluto/source/pluto-reconstruction.png'
 expected='b1d7c1b5be195299d73931b9bacff934a28b7f0c26f3035642c2dca05bb55087'
 if hashlib.sha256(reconstruction.read_bytes()).hexdigest()!=expected:raise ValueError('Unexpected Pluto reconstruction hash')
 with Image.open(reconstruction) as image:
  reconstructed=np.asarray(image.convert('RGB').resize(SIZE,Image.Resampling.LANCZOS),dtype=np.float32).copy()
 original=np.asarray(Image.fromarray(rgb).resize(SIZE,Image.Resampling.LANCZOS),dtype=np.float32)
 height,width=original.shape[:2]
 # Correct border colors continuously rather than duplicating/mirroring strips.
 # The longitude wrap has equal endpoints, with correction decaying into the map.
 delta=(reconstructed[:,-1,:]-reconstructed[:,0,:])*.5
 taper=(.5+.5*np.cos(np.linspace(0,np.pi,192))).astype(np.float32)
 reconstructed[:,:192,:]+=delta[:,None,:]*taper[None,:,None]
 reconstructed[:,-192:,:]-=delta[:,None,:]*taper[None,::-1,None]
 observed=original.max(axis=2)>40
 boundary=np.where(observed,np.arange(height)[:,None],0).max(axis=0)
 x=(np.arange(width)+.5)/width;y=np.arange(height)[:,None]
 def smooth(v):
  v=np.clip(v,0,1);return v*v*(3-2*v)
 # Preserve the heart and sharp encounter hemisphere; blend only at its borders.
 central=smooth((x-.28)/.055)*smooth((.72-x)/.055)
 coverage=smooth((boundary[None,:]-80-y)/96)
 keep=central[None,:]*coverage
 rgb=original*keep[:,:,None]+reconstructed*(1-keep[:,:,None])
 # Collapse only the final eight rows at each pole to avoid longitude pinwheels.
 for pole,start in ((0,0),(-1,height-8)):
  mean=rgb[pole,:,:].mean(axis=0)
  for row in range(start,start+8):
   weight=(7-row)/7 if start==0 else (row-start)/7
   rgb[row]=rgb[row]*(1-weight)+mean*weight
 rgb=np.uint8(np.clip(rgb,0,255)+.5)
 Image.fromarray(rgb).save(cache/'pluto-color.png')
 output=ROOT/'assets/pluto';output.mkdir(parents=True,exist_ok=True)
 manifest={'sources':{'PIA11707':{'url':URL,'page':'https://www.jpl.nasa.gov/images/pia11707-pluto-color-map/','sha256':SHA256,'dimensions':[5926,2963]}},
 'derivation':{'version':2,'dimensions':list(SIZE),'mapping':'north up, equirectangular; map center faces viewer at startup; 25 degree north observer',
 'surface':'Original MVIC high-resolution encounter/heart region retained; lower-resolution far side and missing south replaced with an artistic imagegen reconstruction. Not calibrated albedo or measured elevation.',
 'originalCoverageBoundarySamples':[[int(x),int(v)] for x,v in enumerate(np.where(original.max(axis=2)>12,np.arange(height)[:,None],0).max(axis=0)) if x%8==0],
 'reconstruction':{'file':'source/pluto-reconstruction.png','sha256':expected,'dimensions':[1774,887],'method':'Built-in imagegen edit of PIA11707; prompt saved beside source. Central observed region retained with smooth boundary weights, excluding the original dark coverage edge. Longitude endpoint color correction over 192px; final 8 polar rows converge.'},
 'pillow':Image.__version__,'numpy':np.__version__}}
 (output/'sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('Prepared 5920x2960 Pluto photographic map; detailed reconstructed far side and continuous south cap')
if __name__=='__main__':main()
