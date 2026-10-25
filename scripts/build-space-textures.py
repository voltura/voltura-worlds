"""Pack NASA catalog stars into the existing 4K sky texture's unused alpha."""
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'.deps/python'))
import imagecodecs
import numpy as np
from PIL import Image


def main():
    cache = ROOT/'.deps/space';output = ROOT/'assets/space'
    cache.mkdir(parents=True,exist_ok=True);output.mkdir(parents=True,exist_ok=True)
    foreground = cache/'hiptyc_2020_4k_gal.exr'
    background = ROOT/'assets/orbit/milkyway-2020.png'
    expected = {
        foreground: '7ddcebdd7f29815a32dffc62b57223adc351a0b3ae6badba38748e13a648892e',
        background: '6dbb55798cf64c0e306ebe21b1e57c9dee4a6207a83cf52fe9a732f1fbaaed38',
    }
    for path,checksum in expected.items():
        with path.open('rb') as stream:
            if hashlib.file_digest(stream,'sha256').hexdigest() != checksum:
                raise ValueError('Sky source hash changed: '+path.name)
    stars = imagecodecs.exr_decode(foreground.read_bytes()).astype(np.float32)
    if stars.shape != (2048,4096,3) or not np.isfinite(stars).all():
        raise ValueError('Unexpected NASA star foreground layout')
    # A neutral foreground preserves catalog positions and relative intensities.
    # RGB remains the original Milky Way derivative; alpha is independent linear
    # star emission, never transparency. BC7 retains the same allocation size.
    intensity = np.mean(np.maximum(stars,0),axis=2)
    with Image.open(background) as image:
        rgb = image.convert('RGB')
        alpha = Image.fromarray(np.uint8(np.clip(intensity,0,1)*255+.5))
        rgba = Image.merge('RGBA',(*rgb.split(),alpha))
        rgba.save(cache/'planet-sky.png')
    manifest = {
        'sources': {
            'foreground': {'url':'https://svs.gsfc.nasa.gov/vis/a000000/a004800/a004851/hiptyc_2020_4k_gal.exr','sha256':expected[foreground]},
            'backgroundDerivative': {'path':'assets/orbit/milkyway-2020.png','sha256':expected[background],
                                     'originalUrl':'https://svs.gsfc.nasa.gov/vis/a000000/a004800/a004851/milkyway_2020_4k_gal.exr'},
        },
        'derivation': {'conversionVersion':1,'dimensions':[4096,2048],
                       'channels':'sRGB RGB: original Milky Way; linear alpha: mean positive RGB of NASA Hipparcos/Tycho foreground',
                       'coordinates':'NASA January 2021 corrected galactic mapping; same coordinates for RGB and foreground',
                       'runtimeStarGain':.18,'runtimeStarSmoothFade':[.02,.08],
                       'pillow':Image.__version__,'numpy':np.__version__,'imagecodecs':imagecodecs.__version__},
    }
    (output/'sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Prepared shared 4K Milky Way + catalog-star texture without extra runtime texture memory',flush=True)


if __name__ == '__main__':
    main()
