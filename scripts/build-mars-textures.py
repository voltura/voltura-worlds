"""Build Mars runtime maps from NASA/USGS Viking and NASA MOLA data.

Run with Python, Pillow and numpy (the project's .deps/python is supported),
then run build-mars-textures.ps1 for pinned DirectXTex compression.
Source downloads stay in .deps/mars; only runtime maps ship with the app.
"""
import hashlib
import json
from pathlib import Path
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.deps/python'))
import numpy as np
from PIL import Image

SOURCES = {
    'viking-color.jpg': 'https://astrogeology.usgs.gov/ckan/dataset/7131d503-cdc9-45a5-8f83-5126c0fd397e/resource/5ea881c6-01b3-41fa-a7af-42d2131b54f1/download/mars_viking_mdim21_clrmosaic_1km.jpg',
    'mola-elevation.img': 'https://pds-geosciences.wustl.edu/mgs/urn-nasa-pds-mgs_mola_topography_derived/meg032/megt90n000fb.img',
    'mola-elevation.xml': 'https://pds-geosciences.wustl.edu/mgs/urn-nasa-pds-mgs_mola_topography_derived/meg032/megt90n000fb.xml',
}


EXPECTED_SHA256 = {
    'viking-color.jpg': 'fdfcd335559c3dc67052b7e8a9565d850e336ac0d1f3ea7f5eb7826ffb44ecb2',
    'mola-elevation.img': '09ae1c0956afcdc869bd82c55993849f67c16ef0efe73f38cb1ea204c186ad8b',
    'mola-elevation.xml': '5a6e71a01e1674769274416a958021df4ca4cf191a7631d88235d8e6b8146715',
}


def main():
    cache = ROOT / '.deps/mars'
    cache.mkdir(parents=True, exist_ok=True)
    output = ROOT / 'assets/mars'
    output.mkdir(parents=True, exist_ok=True)
    manifest = {}
    for name, url in SOURCES.items():
        path = cache / name
        if not path.exists():
            urllib.request.urlretrieve(url, path)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != EXPECTED_SHA256[name]:
            raise ValueError(f'Unexpected source hash: {name}')
        manifest[name] = {'url': url, 'sha256': digest}
    # This documented USGS browse product has 227 million pixels.
    Image.MAX_IMAGE_PIXELS = 250_000_000
    with Image.open(cache / 'viking-color.jpg') as source:
        color = source.convert('RGB').resize((8192, 4096), Image.Resampling.LANCZOS)
    color.save(cache / 'mars-color.png')
    # PDS4 XML: SignedMSB2, 5760 lines x 11520 samples, meters above areoid,
    # north to south, positive-east longitude 0..360. Color uses -180..180.
    elevation = np.fromfile(cache / 'mola-elevation.img', dtype='>i2')
    if elevation.size != 5760 * 11520:
        raise ValueError('Unexpected MOLA product dimensions')
    elevation = np.roll(elevation.reshape(5760, 11520).astype(np.float32), 5760, axis=1)
    height = np.asarray(Image.fromarray(elevation).resize((8192, 4096), Image.Resampling.BILINEAR))
    del elevation
    radius = 3_396_000.0
    latitude = (0.5 - (np.arange(4096, dtype=np.float32) + .5) / 4096) * np.pi
    east_spacing = (2 * np.pi * radius / 8192) * np.maximum(np.cos(latitude), .015)
    east = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) / (2 * east_spacing[:, None])
    north = -np.gradient(height, axis=0) / (np.pi * radius / 4096)
    normal = np.stack((-east, -north, np.ones_like(height)), axis=2)
    normal /= np.linalg.norm(normal, axis=2, keepdims=True)
    # Longitude is undefined at the poles; taper noisy east/west slopes there.
    normal[:2, :, :2] = 0
    normal[-2:, :, :2] = 0
    encoded = np.uint8(np.clip(normal * .5 + .5, 0, 1) * 255 + .5)
    Image.fromarray(encoded).save(cache / 'mars-normal.png')
    manifest['derivation'] = {
        'dimensions': [8192, 4096], 'radiusMeters': radius,
        'normalBasis': 'east, north, outward; physical elevation slopes; no baked exaggeration',
        'longitude': '-180..180 positive east; north at top',
        'pillow': Image.__version__, 'numpy': np.__version__,
    }
    (output / 'sources.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Prepared 8192x4096 Mars color and linear terrain-normal maps', flush=True)


if __name__ == '__main__':
    main()
