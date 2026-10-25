"""Reproduce Moon maps from NASA SVS LROC 2025 / LOLA and compact NASA Earth.

Requires numpy, Pillow, tifffile and imagecodecs in .deps/python or Python.
Original TIFFs remain in .deps/moon; no original downloads ship.
"""
import concurrent.futures
import hashlib
import json
from pathlib import Path
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.deps/python'))
import numpy as np
import tifffile
import imagecodecs
from PIL import Image

BASE = 'https://svs.gsfc.nasa.gov/vis/a000000/a004700/a004720/'
SOURCES = {name: BASE + name for name in ('lroc_color_16bit_srgb_8k.tif', 'ldem_64_uint.tif')}
EXPECTED = {
    'lroc_color_16bit_srgb_8k.tif': 'db7808e878b6a55eb409bb231eab8deb477f84b5c9d7396d76ff73e5d54992d9',
    'ldem_64_uint.tif': '0f40bce8b42864deddb6943a38474879e691d5b20647aa5e54c2612b23106499',
}
SIZE = (8192, 4096)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def srgb_to_linear(v):
    return np.where(v <= .04045, v / 12.92, ((v + .055) / 1.055) ** 2.4)


def linear_to_srgb(v):
    return np.where(v <= .0031308, v * 12.92, 1.055 * np.maximum(v, 0) ** (1 / 2.4) - .055)


def main():
    cache = ROOT / '.deps/moon'
    output = ROOT / 'assets/moon'
    cache.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    manifest_file = output / 'sources.json'
    previous = json.loads(manifest_file.read_text())['sources'] if manifest_file.exists() else {}

    def download(item):
        name, url = item
        path = cache / name
        if not path.exists():
            print('Downloading ' + name, flush=True)
            temporary = path.with_suffix('.tif.part')
            urllib.request.urlretrieve(url, temporary)
            temporary.replace(path)
        checksum = digest(path)
        if checksum != EXPECTED[name]:
            raise ValueError('Source hash changed: ' + name)
        print(name + ': ' + checksum, flush=True)
        return name, {'url': url, 'sha256': checksum}

    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        sources = dict(pool.map(download, SOURCES.items()))

    # tifffile retains RGB16 precision; Pillow's RGB TIFF decoder reduces to 8 bit.
    color = tifffile.imread(cache / 'lroc_color_16bit_srgb_8k.tif')
    if color.shape != (4096, 8192, 3) or color.dtype != np.uint16:
        raise ValueError(f'Unexpected LROC encoding: {color.shape} {color.dtype}')
    landmarks = {}
    for name, lon, lat in (('Tycho', -11.36, -43.31), ('Copernicus', -20.08, 9.62),
                           ('Mare Tranquillitatis', 31.4, 8.5), ('Tsiolkovskiy', 129.1, -20.4),
                           ('Mare Moscoviense', 147.9, 27.3), ('longitude seam west', -179.95, 0),
                           ('longitude seam east', 179.95, 0), ('north pole', 0, 89.95), ('south pole', 0, -89.95)):
        x, y = int((lon+180)/360*8192), int((90-lat)/180*4096)
        landmarks[name] = {'longitude': lon, 'latitude': lat, 'sourcePixel': [x, y],
                           'sourceRgb16': [int(v) for v in color[y, x]]}
    # Keep 16-bit samples through source conversion. Quantize only at the final
    # 8-bit BC7 input; ordered sub-LSB dither avoids coherent banding.
    quantized = np.empty(color.shape, dtype=np.uint8)
    pattern = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], dtype=np.float32)
    dither = np.tile((pattern + .5) / 16 - .5, (1024, 2048))
    for channel in range(3):
        values = color[:, :, channel].astype(np.float32) / 257
        quantized[:, :, channel] = np.uint8(np.clip(np.floor(values + .5 + dither), 0, 255))
    Image.fromarray(quantized).save(cache / 'moon-color.png')
    del color, quantized, dither

    # NASA's unsigned TIFF is in half meters above radius 1,727,400 m:
    # elevation relative to the 1,737,400 m mean sphere = sample * .5 - 10,000.
    dem = tifffile.memmap(cache / 'ldem_64_uint.tif')
    if dem.shape != (11520, 23040) or dem.dtype != np.uint16:
        raise ValueError(f'Unexpected LOLA encoding: {dem.shape} {dem.dtype}')
    # Filter float elevations, never an 8-bit image. Both sources are already
    # centered on 0 longitude, north at top, positive east to the right.
    resized = Image.fromarray(np.asarray(dem, dtype=np.float32) * .5 - 10000).resize(SIZE, Image.Resampling.BILINEAR)
    height = np.asarray(resized)
    radius = 1737400.0
    lat = (.5 - (np.arange(4096, dtype=np.float32) + .5) / 4096) * np.pi
    spacing = 2 * np.pi * radius / 8192 * np.maximum(np.cos(lat), .015)
    east = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) / (2 * spacing[:, None])
    north = -np.gradient(height, axis=0) / (np.pi * radius / 4096)
    normal = np.stack((-east, -north, np.ones_like(height)), axis=2)
    normal /= np.linalg.norm(normal, axis=2, keepdims=True)
    normal[:2, :, :] = (0, 0, 1)
    normal[-2:, :, :] = (0, 0, 1)
    Image.fromarray(np.uint8(np.clip(normal * .5 + .5, 0, 1) * 255 + .5)).save(cache / 'moon-normal.png')
    del dem, resized, height, east, north, normal

    # A small, self-contained Earth texture: linear-light cloud composite in
    # RGB and cloud-attenuated Black Marble emission in linear alpha.
    earth_manifest = json.loads((ROOT / 'assets/earth/sources.json').read_text())
    earth_inputs = {
        'surface': ROOT / '.deps/earth/january.jpg',
        'clouds': ROOT / 'assets/orbit/earth-clouds-8192.tif',
        'night': ROOT / '.deps/earth/night.jpg',
    }
    Image.MAX_IMAGE_PIXELS = 300_000_000
    layers = {}
    for key, path in earth_inputs.items():
        record = earth_manifest['sources']['january.jpg' if key == 'surface' else 'night.jpg'] if key != 'clouds' else {
            'url': 'https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57747/cloud_combined_8192.tif',
            'sha256': 'd137775d8966ab8d443fd5126dc6e7ad72072bc1ed50555c5818d221735daf0f',
        }
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            temporary = path.with_suffix(path.suffix + '.part')
            urllib.request.urlretrieve(record['url'], temporary)
            temporary.replace(path)
        if digest(path) != record['sha256']:
            raise ValueError('Earth source hash changed: ' + key)
        sources['earth-' + key] = dict(record)
        with Image.open(path) as image:
            layers[key] = np.asarray(image.convert('RGB' if key == 'surface' else 'L').resize((2048, 1024), Image.Resampling.LANCZOS), dtype=np.float32) / 255
    clouds = np.clip(layers['clouds'] * 1.12 - .045, 0, 1)
    rgb = srgb_to_linear(layers['surface']) * (1 - clouds[:, :, None]) + .8 * clouds[:, :, None]
    rgba = np.empty((1024, 2048, 4), dtype=np.uint8)
    rgba[:, :, :3] = np.uint8(np.clip(linear_to_srgb(rgb), 0, 1) * 255 + .5)
    rgba[:, :, 3] = np.uint8(layers['night'] * (1 - clouds) * 255 + .5)
    Image.fromarray(rgba).save(cache / 'moon-earth.png')
    for key in earth_inputs:
        if 'earth-' + key in previous and sources['earth-' + key]['sha256'] != previous['earth-' + key]['sha256']:
            raise ValueError('Earth source hash changed: ' + key)
    manifest = {'sources': sources, 'landmarks': landmarks, 'derivation': {
        'conversionVersion': 1, 'dimensions': list(SIZE), 'radiusMeters': radius,
        'longitude': '-180..180 positive east; north at top; no source longitude shift',
        'normalBasis': 'east, north, outward; physical slopes, flat first/last two polar rows',
        'elevationMeters': 'unsigned sample * 0.5 - 10000 relative to 1737400 m sphere',
        'color': 'RGB16 sRGB retained until ordered-dither quantization to BC7 input',
        'earth': '2048x1024 RGB: linear-light cloud/surface composite; linear alpha: cloud-attenuated night emission',
        'numpy': np.__version__, 'pillow': Image.__version__, 'tifffile': tifffile.__version__, 'imagecodecs': imagecodecs.__version__,
    }}
    manifest_file.write_text(json.dumps(manifest, indent=2) + '\n')
    print('Prepared Moon color, terrain normals and compact Earth', flush=True)


if __name__ == '__main__':
    main()
