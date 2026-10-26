"""Reproduce 8K Mercury textures from NASA/MESSENGER/USGS source GeoTIFFs.
Requires numpy, Pillow and tifffile; caches and original data never ship.
"""
import hashlib
import json
from pathlib import Path
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'.deps/python'))
import numpy as np
import tifffile
from PIL import Image

SOURCES = {
 'surface.tif': ('https://planetarymaps.usgs.gov/mosaic/Mercury_MESSENGER_mosaic_global_250m_2013.tif',
                 '720938d9a403803b016e88d22d58cddbbf4fe8cb99f652c90f19b5a1bc946c76'),
 'elevation.tif': ('https://planetarymaps.usgs.gov/mosaic/Mercury_Messenger_USGS_DEM_Global_665m_v2.tif',
                   'defce776241dcaf44cb0f081ee508c17dbea28aa5a22880bf7a5e8c25f96cbea'),
}
SIZE = (8192,4096)


def main():
 cache=ROOT/'.deps/mercury'; cache.mkdir(parents=True,exist_ok=True)
 output=ROOT/'assets/mercury'; output.mkdir(parents=True,exist_ok=True)
 manifest={}
 for name,(url,expected) in SOURCES.items():
  path=cache/name
  if not path.exists():
   temporary=path.with_suffix('.part')
   urllib.request.urlretrieve(url,temporary);temporary.replace(path)
  with path.open('rb') as stream: checksum=hashlib.file_digest(stream,'sha256').hexdigest()
  if checksum!=expected: raise ValueError('Unexpected source hash: '+name)
  with tifffile.TiffFile(path) as source:
   page=source.pages[0];geo=page.geotiff_tags
   center=geo['ProjCenterLongGeoKey']
   expected_shape=(30662,61324) if name=='surface.tif' else (11520,23040)
   if page.shape!=expected_shape or center!=(0 if name=='surface.tif' else 180):
    raise ValueError('Unexpected Mercury source grid: '+name)
   manifest[name]={'url':url,'sha256':checksum,'dimensions':[page.shape[1],page.shape[0]],
                   'projection':geo,'nodata':str(page.tags['GDAL_NODATA'].value)}
 # Memory-map the large uncompressed originals. Preserve source geography;
 # monochrome photographic brightness receives only a restrained warm tint.
 surface=tifffile.memmap(cache/'surface.tif')
 gray=np.asarray(Image.fromarray(surface).resize(SIZE,Image.Resampling.LANCZOS,reducing_gap=3),dtype=np.float32)
 rgb=np.uint8(np.clip(gray[:,:,None]*np.array([1.025,1.0,.97],dtype=np.float32),0,255)+.5)
 Image.fromarray(rgb).save(cache/'mercury-color.png')
 del surface,gray,rgb
 elevation=tifffile.memmap(cache/'elevation.tif')
 if np.any(elevation==-32768): raise ValueError('Unexpected missing DEM samples')
 # DEM is 0..360 east; the surface mosaic is -180..180 east.
 # Filter elevations in float32 before deriving east/north physical slopes.
 height=np.asarray(Image.fromarray(elevation.astype(np.float32)).resize(SIZE,Image.Resampling.BILINEAR))
 height=np.roll(height,4096,axis=1)
 radius=2439400.0
 latitude=(.5-(np.arange(4096,dtype=np.float32)+.5)/4096)*np.pi
 spacing=2*np.pi*radius/8192*np.maximum(np.cos(latitude),.015)
 east=(np.roll(height,-1,axis=1)-np.roll(height,1,axis=1))/(2*spacing[:,None])
 north=-np.gradient(height,axis=0)/(np.pi*radius/4096)
 normal=np.stack((-east,-north,np.ones_like(height)),axis=2)
 normal/=np.linalg.norm(normal,axis=2,keepdims=True)
 normal[:2,:,:]=[0,0,1];normal[-2:,:,:]=[0,0,1]
 Image.fromarray(np.uint8(np.clip(normal*.5+.5,0,1)*255+.5)).save(cache/'mercury-normal.png')
 manifest['derivation']={'version':1,'dimensions':list(SIZE),'radiusMeters':radius,
  'longitude':'-180..180 positive east, north up; DEM shifted 180 degrees to match surface',
  'normalBasis':'east, north, outward; physical slopes; polar first/last two rows flattened',
  'surface':'monochrome photographic mosaic, sRGB display interpretation, RGB tint [1.025,1,.97]; not calibrated albedo or measured true color',
  'pillow':Image.__version__,'numpy':np.__version__,'tifffile':tifffile.__version__}
 (output/'sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
 print('Prepared 8192x4096 Mercury surface and physical terrain-normal maps',flush=True)


if __name__=='__main__': main()
