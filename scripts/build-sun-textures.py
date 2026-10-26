"""Prepare a pinned NASA/SDO front-facing AIA 171 observation, not a globe map."""
import hashlib
import json
from pathlib import Path
import sys
import urllib.request
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'.deps/python'))
import numpy as np
from PIL import Image

URL = 'https://sdo.gsfc.nasa.gov/assets/img/browse/2026/09/24/20260924_000610_4096_0171.jpg'
SHA256 = 'aa16f6ec5d4f82dba42259a57f80668a85214f32abdf79f23f2602b232fba47a'
PROMINENCE_URL = 'https://svs.gsfc.nasa.gov/vis/a010000/a010600/a010610/AIA_red_2_full_disk_4Kx3K_00069.jpg'
PROMINENCE_SHA256 = 'e626b5576b255a3dc08b6bec7f32c40c9be320227c69398d8fe7246c27e58cee'

def main():
    cache = ROOT/'.deps/sun'
    cache.mkdir(parents=True, exist_ok=True)
    source = cache/'source.jpg'
    if not source.exists():
        temporary = source.with_suffix('.part')
        urllib.request.urlretrieve(URL, temporary)
        temporary.replace(source)
    if hashlib.sha256(source.read_bytes()).hexdigest() != SHA256:
        raise ValueError('Unexpected SDO observation checksum')
    with Image.open(source) as image:
        if image.size != (4096,4096):
            raise ValueError('Unexpected SDO image dimensions')
        rgb = np.asarray(image.convert('RGB'), dtype=np.float32)/255
    # Preserve the observed disk and its corona; remove only the bottom caption
    # and taper the outer frame to black. No spherical reprojection or upscaling.
    y,x = np.mgrid[:4096,:4096]
    radius = np.sqrt((x-2047.5)**2+(y-2047.5)**2)/1605
    fade = np.clip((1.25-radius)/.06,0,1)*np.clip((3900-y)/80,0,1)
    linear = np.where(rgb<=.04045,rgb/12.92,((rgb+.055)/1.055)**2.4)
    linear *= fade[:,:,None]
    encoded = np.where(linear<=.0031308,linear*12.92,1.055*linear**(1/2.4)-.055)
    Image.fromarray(np.uint8(np.clip(encoded*255+.5,0,255))).save(cache/'sun-aia171.png')
    prominence_source=cache/'prominence-source.jpg'
    if not prominence_source.exists():
        urllib.request.urlretrieve(PROMINENCE_URL,prominence_source)
    if hashlib.sha256(prominence_source.read_bytes()).hexdigest()!=PROMINENCE_SHA256:
        raise ValueError('Unexpected NASA prominence observation checksum')
    with Image.open(prominence_source) as image:
        if image.size!=(4096,3112):raise ValueError('Unexpected prominence dimensions')
        # Align the observed limb to the 171-image disk. The original contains
        # genuine off-limb fine structure, so no parametric arcs are generated.
        image=image.convert('RGB').transform((4096,4096),Image.Transform.AFFINE,
            (1450/1605,0,2048-2047.5*1450/1605,
             0,1450/1605,1556-2047.5*1450/1605),Image.Resampling.BICUBIC)
        plasma=np.asarray(image,dtype=np.float32)/255
    linear=np.where(plasma<=.04045,plasma/12.92,((plasma+.055)/1.055)**2.4)
    weight=np.clip((radius-.89)/.13,0,1)
    weight=weight*weight*(3-2*weight)
    outer=np.clip((1.48-radius)/.07,0,1)
    linear*=weight[:,:,None]*outer[:,:,None]
    encoded=np.where(linear<=.0031308,linear*12.92,1.055*linear**(1/2.4)-.055)
    Image.fromarray(np.uint8(np.clip(encoded*255+.5,0,255))).save(cache/'sun-prominence.png')
    output = ROOT/'assets/sun'
    output.mkdir(parents=True,exist_ok=True)
    manifest = {'observationUTC':'2026-09-24T00:06:10Z','instrument':'NASA/SDO AIA',
                'wavelengthAngstrom':171,'url':URL,'sha256':SHA256,
                'prominence':{'url':PROMINENCE_URL,'sha256':PROMINENCE_SHA256,
                    'page':'https://svs.gsfc.nasa.gov/10610/','observationDateUTC':'2010-03-30',
                    'wavelengthAngstrom':304,'sourceDimensions':[4096,3112],
                    'centerPixels':[2048,1556],'diskRadiusPixels':1450,
                    'processing':'Bicubic affine image alignment to 4096 square; linear-light smooth radial mask .89–1.02 and outer fade 1.41–1.48 disk radii; BC7 sRGB mip chain. Artistic composite with 2026 AIA171 disk, not simultaneous observations.'},
                'sourceDimensions':[4096,4096],
                'derivation':{'dimensions':[4096,4096],'centerPixels':[2047.5,2047.5],
                              'diskRadiusPixels':1605,'mapping':'front-facing image, north up; not equirectangular',
                              'processing':'sRGB decode; radial fade 1.19–1.25 disk radii; bottom caption fade rows 3820–3900; sRGB encode; full mip chain BC7 sRGB',
                              'pillow':Image.__version__,'numpy':np.__version__}}
    (output/'sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Prepared 4096x4096 NASA/SDO AIA 171 front-facing observation')

if __name__ == '__main__':
    main()
