"""Convert NASA/ESA Hubble OPAL Cycle 32 imagery and atmospheric observations.

The PS1 entry point downloads with Windows certificate verification. Python
uses the pinned local sources, numpy and Pillow; no FITS dependency is needed
for the deliberately narrow OPAL primary-image reader below.
"""
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.deps/python'))
import numpy as np
from PIL import Image

BASE = 'https://archive.stsci.edu/missions/hlsp/opal/cycle32/jupiter/'
PREFIX = 'hlsp_opal_hst_wfc3-uvis_jupiter-2025'
SOURCES = {
    PREFIX + '_all_v1_readme.txt': '344f5c1bcae360edfc7b4f1183b9ed47f887cbbe7fa0518fdf577c915784fdad',
    PREFIX + 'a_f395n-f502n-f631n_v1_globalmap.tif': '52b6cc32639f699ff5614b169c4c2727fff98e7eb432aff7c718177191716e6c',
    PREFIX + 'a_f631n_v1_globalmap.fits': '376ebe3b3e25d1bf7a3968486c8952bd7e1a11e15504b74c4e9f6021e6f0a0bd',
    PREFIX + 'b_f631n_v1_globalmap.fits': 'd990c53c9000bb413b64597c9ceab3da0f4d75554652e4e46b0a2879e4a6cb55',
    PREFIX + 'a_fq889n_v1_globalmap.fits': '4bdbeb4a4f9fbfe0daa737c4ba29de31b0626fb6e58c54e0142ef6d3aa6ef5ea',
}
SIZE = (4096, 2048)


def fits_image(path):
    """Read only a complete 3600x1800 big-endian float32 primary OPAL HDU."""
    with path.open('rb') as stream:
        header = {}
        for _ in range(16):
            block = stream.read(2880)
            if len(block) != 2880:
                raise ValueError('Truncated OPAL header')
            cards = [block[i:i+80].decode('ascii') for i in range(0, 2880, 80)]
            for card in cards:
                if card[8:10] == '= ':
                    header[card[:8].strip()] = card[10:30].strip().strip("'").strip()
            if any(card.startswith('END     ') for card in cards):
                break
        else:
            raise ValueError('OPAL header missing END')
        if tuple(header.get(key) for key in ('BITPIX', 'NAXIS', 'NAXIS1', 'NAXIS2')) != ('-32', '2', '3600', '1800'):
            raise ValueError('Unexpected OPAL FITS layout')
        if header.get('BSCALE', '1') != '1' or header.get('BZERO', '0') != '0':
            raise ValueError('Unexpected OPAL scaling')
        values = np.frombuffer(stream.read(3600*1800*4), dtype='>f4').reshape(1800, 3600).astype(np.float32)
    if not np.isfinite(values).all():
        raise ValueError('Non-finite OPAL pixels')
    return values, header


def polar_fill(array):
    """Preserve observations through 75 degrees, then converge to zonal means.

    All data beyond 80 degrees is deliberately marked as a reconstruction,
    not fabricated polar storm detail. This also removes marginal map fringes.
    """
    result = array.copy()
    for y in range(1800):
        lat = 90-(y+.5)/10
        if abs(lat) <= 75:
            continue
        edge = 100 if y < 900 else 1699
        if abs(lat) >= 80:
            result[y] = array[edge].mean(axis=0)
        else:
            t = (abs(lat)-75)/5
            t = t*t*(3-2*t)
            result[y] = array[y]*(1-t)+array[edge].mean(axis=0)*t
    return result


def main():
    cache = ROOT / '.deps/jupiter'
    output = ROOT / 'assets/jupiter'
    output.mkdir(parents=True, exist_ok=True)
    manifest = {'sources': {}}
    for name, checksum in SOURCES.items():
        with (cache/name).open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        if digest != checksum:
            raise ValueError('Source hash changed: '+name)
        manifest['sources'][name] = {'url': BASE+name, 'sha256': digest}
    with Image.open(cache/(PREFIX+'a_f395n-f502n-f631n_v1_globalmap.tif')) as image:
        color = np.asarray(image.convert('RGB'), dtype=np.float32)
    if color.shape != (1800, 3600, 3):
        raise ValueError('Unexpected OPAL color dimensions')
    red_a, ha = fits_image(cache/(PREFIX+'a_f631n_v1_globalmap.fits'))
    red_b, hb = fits_image(cache/(PREFIX+'b_f631n_v1_globalmap.fits'))
    methane, hm = fits_image(cache/(PREFIX+'a_fq889n_v1_globalmap.fits'))
    valid = red_a > 0
    correlation = float(np.corrcoef(red_a[valid][::100], color[:, :, 0][valid][::100])[0, 1])
    if correlation < .98:
        raise ValueError('FITS and TIFF orientation/alignment differ')

    # FITS arrays and the north-up color preview have matching row order.
    # Convert both filters to I/F with their README factors, then normalize
    # methane/continuum reflectance. This is an aerosol/high-cloud proxy,
    # not a measured cloud-height or optical-depth product.
    ratio = methane*.00088 / np.maximum(red_a*.00383, .001)
    low, high = [float(x) for x in np.percentile(ratio[100:1700], (2, 98))]
    aerosol = polar_fill(np.clip((ratio-low)/(high-low), 0, 1))
    color_filled = polar_fill(color)
    # RGB resampling in linear light, encoded back to sRGB for BC7.
    srgb = color_filled/255
    linear = np.where(srgb <= .04045, srgb/12.92, ((srgb+.055)/1.055)**2.4)
    channels = []
    for c in range(3):
        channel = np.asarray(Image.fromarray(linear[:, :, c]).resize(SIZE, Image.Resampling.LANCZOS))
        channels.append(np.where(channel <= .0031308, channel*12.92, 1.055*np.maximum(channel, 0)**(1/2.4)-.055))
    rgb = np.uint8(np.clip(np.stack(channels, axis=2), 0, 1)*255+.5)
    Image.fromarray(rgb).save(cache/'jupiter-color.png')

    # Cross-correlate successive observed red-filter strips in the fixed
    # System III frame. Positive pixel lag is positive-east cloud motion.
    elapsed = ((float(hb['EXPSTART'])+float(hb['EXPEND']))-(float(ha['EXPSTART'])+float(ha['EXPEND'])))*43200
    angular = np.zeros(1800, dtype=np.float32)
    confidence = np.zeros(1800, dtype=np.float32)
    eccentricity2 = 1-(66854/71492)**2
    for y in range(200, 1600):
        latitude = np.radians(90-(y+.5)/10)
        parallel_radius = 71492000*np.cos(latitude)/np.sqrt(1-eccentricity2*np.sin(latitude)**2)
        meters_per_pixel = 2*np.pi*parallel_radius/3600
        a, b = red_a[y-5:y+6].copy(), red_b[y-5:y+6].copy()
        a -= a.mean(axis=1, keepdims=True)
        b -= b.mean(axis=1, keepdims=True)
        cc = np.fft.irfft(np.conj(np.fft.rfft(a))*np.fft.rfft(b), n=3600).sum(axis=0)
        max_lag = int(180*elapsed/meters_per_pixel)
        lags = np.arange(-max_lag, max_lag+1)
        lag = int(lags[np.argmax(cc[lags % 3600])])
        quality = float(cc[lag % 3600]/max(np.sqrt(np.sum(a*a)*np.sum(b*b)), 1e-10))
        confidence[y] = quality
        if quality >= .65 and abs(lag) < max_lag:
            # Subpixel parabola around the correlation peak.
            before, peak, after = (float(cc[(lag+i) % 3600]) for i in (-1, 0, 1))
            refinement = np.clip(.5*(before-after)/min(before-2*peak+after, -1e-10), -.5, .5)
            angular[y] = (lag+refinement)*(2*np.pi/3600)/elapsed
    # A 0.7-degree Gaussian smooths individual strip estimates. Fade the
    # untrusted high-latitude edges rather than assigning invented polar jets.
    kernel = np.exp(-np.arange(-21, 22, dtype=np.float32)**2/(2*7**2));kernel /= kernel.sum()
    angular = np.convolve(angular, kernel, mode='same')
    latitudes = 90-(np.arange(1800)+.5)/10
    angular *= np.clip((70-np.abs(latitudes))/5, 0, 1)
    if float(np.max(np.abs(angular))) > .00001:
        raise ValueError('Unreasonable inferred wind rate')
    profile = {'elapsedSeconds': elapsed, 'latitudeOrder': 'north to south, pixel centers, 0.1 degree',
               'angularRadiansPerSecond': [round(float(v), 10) for v in angular],
               'correlation': [round(float(v), 4) for v in confidence]}
    (output/'wind-profile.json').write_text(json.dumps(profile, indent=2)+'\n')
    aerosol_image = Image.fromarray(aerosol).resize(SIZE, Image.Resampling.BILINEAR)
    wind = np.broadcast_to(angular[:, None], (1800, 3600)).copy()
    wind_image = Image.fromarray(wind.astype(np.float32)).resize(SIZE, Image.Resampling.BILINEAR)
    material = np.zeros((2048, 4096, 3), dtype=np.uint8)
    material[:, :, 0] = np.uint8(np.clip(np.asarray(aerosol_image), 0, 1)*255+.5)
    material[:, :, 1] = np.uint8(np.clip(np.asarray(wind_image)/.00001*127+128, 0, 255)+.5)
    Image.fromarray(material).save(cache/'jupiter-atmosphere.png')
    manifest['derivation'] = {
        'conversionVersion': 1, 'sourceDimensions': [3600, 1800], 'runtimeDimensions': list(SIZE),
        'observation': '2025-12-11, OPAL Cycle 32 rotation A; rotation B for inferred zonal winds',
        'coordinates': 'north up; planetographic latitude; left 0 System III west longitude, decreasing right (positive east)',
        'color': 'OPAL F631N red, F502N green, F395N blue visualization composite; arbitrarily scaled, treated as sRGB',
        'polarFill': 'unchanged through 75 deg; smooth transition to 80-deg zonal mean; constant mean beyond 80 deg',
        'fitsTiffCorrelation': correlation, 'aerosolRatioPercentiles': [low, high],
        'materialChannels': 'R: normalized 889nm/631nm I/F ratio proxy; G byte: angular wind / .00001 * 127 + 128 (128 is exact zero)',
        'wind': '11-row red-strip cross-correlation, >=.65 confidence, max 180 m/s search, 0.7-deg Gaussian; fade 65..70 deg',
        'numpy': np.__version__, 'pillow': Image.__version__,
    }
    landmarks = {}
    for name, longitude, latitude in (('Great Red Spot vicinity', 38, -22), ('equatorial clouds', 160, 0),
                                       ('northern belt', 240, 24), ('southern ovals', 80, -40),
                                       ('seam east', .1, 10), ('seam west', 359.9, 10)):
        x, y = int(longitude/360*3600), int((90-latitude)/180*1800)
        landmarks[name] = {'longitudeEast': longitude, 'latitudeGraphic': latitude, 'sourceRgb': [int(v) for v in color[y, x]]}
    manifest['landmarks'] = landmarks
    (output/'sources.json').write_text(json.dumps(manifest, indent=2)+'\n')
    print('Prepared 4K Jupiter color and matched methane/wind atmosphere maps', flush=True)


if __name__ == '__main__':
    main()
