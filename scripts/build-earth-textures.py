"""Build aligned Earth maps (Python + Pillow; source downloads never ship).

The former NASA BMNG mask archive redirects to a general landing page. Use
versioned public-domain Natural Earth ocean/lake geometry instead of guessing
water from surface color. NASA imagery remains the color/cloud/night source.
"""
import hashlib
import json
from pathlib import Path
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.deps/python'))
from PIL import Image, ImageDraw

SOURCES = {
    'january.jpg': 'https://assets.science.nasa.gov/content/dam/science/esd/eo/images/bmng/bmng-base/january/world.200401.3x21600x10800.jpg',
    'night.jpg': 'https://assets.science.nasa.gov/content/dam/science/esd/eo/images/imagerecords/144000/144897/BlackMarble_2016_3km_gray.jpg',
    'ocean.geojson': 'https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/ne_10m_ocean.geojson',
    'lakes.geojson': 'https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/ne_10m_lakes.geojson',
}


def main():
    cache = ROOT / '.deps/earth'
    output = ROOT / 'assets/earth'
    cache.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    manifest = {'sources': {}, 'derivation': {
        'dimensions': [8192, 4096], 'longitude': '-180..180 positive east; north at top',
        'materialChannels': 'R: water coverage, G: 2016 night emission, B: unused',
        'waterMask': 'Natural Earth v5.1.2 1:10m oceans and lakes; NASA archive unavailable',
        'conversionVersion': 1, 'pillow': Image.__version__,
    }}
    previous = output / 'sources.json'
    expected = json.loads(previous.read_text())['sources'] if previous.exists() else {}
    for name, url in SOURCES.items():
        path = cache / name
        if not path.exists():
            temporary = path.with_suffix(path.suffix + '.part')
            urllib.request.urlretrieve(url, temporary)
            temporary.replace(path)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if name in expected and digest != expected[name]['sha256']:
            raise RuntimeError(f'Source hash changed: {name}')
        manifest['sources'][name] = {'url': url, 'sha256': digest}
    Image.MAX_IMAGE_PIXELS = 300_000_000
    size = (8192, 4096)
    with Image.open(cache / 'january.jpg') as source:
        source.convert('RGB').resize(size, Image.Resampling.LANCZOS).save(cache / 'earth-color.png')
    water = Image.new('L', (16384, 8192))
    draw = ImageDraw.Draw(water)
    for name in ('ocean.geojson', 'lakes.geojson'):
        features = json.loads((cache / name).read_text(encoding='utf-8'))['features']
        for feature in features:
            geometry = feature['geometry']
            if geometry is None:
                continue
            polygons = geometry['coordinates'] if geometry['type'] == 'MultiPolygon' else [geometry['coordinates']]
            for polygon in polygons:
                for index, ring in enumerate(polygon):
                    points = [((lon + 180) / 360 * water.width - .5,
                               (90 - lat) / 180 * water.height - .5) for lon, lat, *rest in ring]
                    draw.polygon(points, fill=255 if index == 0 else 0)
    water = water.resize(size, Image.Resampling.LANCZOS)
    # Geographic landmarks protect polarity, alignment and polygon-hole handling.
    def at(lon, lat):
        return water.getpixel((int((lon+180)/360*size[0]), int((90-lat)/180*size[1])))
    assert at(-140, 0) > 250 and at(20, 20) < 5 and at(90, 30) < 5
    assert at(-87, 44) > 250 and at(0, -85) < 5
    with Image.open(cache / 'night.jpg') as source:
        night = source.convert('L').resize(size, Image.Resampling.LANCZOS)
    Image.merge('RGB', (water, night, Image.new('L', size))).save(cache / 'earth-material.png')
    cloud = ROOT / 'assets/orbit/earth-clouds-8192.tif'
    manifest['sources']['clouds'] = {
        'url': 'https://eoimages.gsfc.nasa.gov/images/imagerecords/57000/57747/cloud_combined_8192.tif',
        'sha256': hashlib.sha256(cloud.read_bytes()).hexdigest(),
        'runtime': '../orbit/runtime/earth-clouds-8192.dds',
    }
    if 'clouds' in expected and manifest['sources']['clouds']['sha256'] != expected['clouds']['sha256']:
        raise RuntimeError('Source hash changed: shared NASA clouds')
    (output / 'sources.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Prepared 8K Earth surface and aligned water/night material maps', flush=True)


if __name__ == '__main__':
    main()
