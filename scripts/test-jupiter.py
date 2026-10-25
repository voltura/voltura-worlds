"""Native Jupiter data alignment, oblate shape, atmosphere, controls and lifecycle."""
import argparse
import json
import math
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'.deps/python'))
from PIL import Image, ImageChops, ImageDraw, ImageStat


def dot(a, b):
    return sum(x*y for x,y in zip(a,b))


def unit(a):
    scale = math.sqrt(dot(a,a))
    return tuple(v/scale for v in a)


def rotate(p, axis, angle):
    c,s = math.cos(angle),math.sin(angle)
    x,y,z = p
    if axis == 'x':
        return x,c*y-s*z,s*y+c*z
    if axis == 'y':
        return c*x+s*z,y,-s*x+c*z
    return c*x-s*y,s*x+c*y,z


def surface(x,y,phase,width=640,height=360):
    # Independent orthographic ray/ellipsoid intersection, no camera drift.
    qx = ((x+.5-width/2)/min(width,height)-.075)/.375
    qy = (-(y+.5-height/2)/min(width,height)-.008)/.375
    spin = math.radians(-38)+(phase+1/60)*2*math.pi/900
    def to_body(p):
        return rotate(rotate(rotate(p,'z',-math.radians(3.13)),'x',-math.radians(8)),'y',-spin)
    origin,ray = to_body((qx,qy,3)),to_body((0,0,-1))
    metric = (1,(71492/66854)**2,1)
    a = sum(ray[i]**2*metric[i] for i in range(3))
    b = sum(origin[i]*ray[i]*metric[i] for i in range(3))
    c = sum(origin[i]**2*metric[i] for i in range(3))-1
    discriminant = b*b-a*c
    if discriminant <= 0:
        return None
    distance = (-b-math.sqrt(discriminant))/a
    position = tuple(origin[i]+ray[i]*distance for i in range(3))
    normal = unit(tuple(position[i]*metric[i] for i in range(3)))
    world = rotate(rotate(rotate(normal,'y',spin),'x',math.radians(8)),'z',math.radians(3.13))
    coord = math.atan2(normal[0],normal[2])/(2*math.pi)%1,.5-math.asin(normal[1])/math.pi
    return normal,world,coord


def display(rgb,exposure=.2):
    linear = [(c/255/12.92 if c/255 <= .04045 else ((c/255+.055)/1.055)**2.4)*exposure for c in rgb]
    peak = max(max(linear),.00001)
    return [255*(.62*(1-math.exp(-c))+.38*c*(1-math.exp(-peak))/peak)**(1/2.2) for c in linear]


def difference(a,b):
    return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)/3


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable',type=Path)
    parser.add_argument('--output',type=Path,default=ROOT/'artifacts/jupiter/regressions')
    args = parser.parse_args()
    results = {}
    def capture(name,**options):
        folder = args.output/name;folder.mkdir(parents=True,exist_ok=True)
        command = [sys.executable,str(ROOT/'scripts/test_desktop.py'),'--',str(args.executable.resolve()),
                   '--capture-dir',str(folder),'--scene','jupiter','--debug-gpu']
        for key,value in {'width':640,'height':360,'frames':1,**options}.items():
            command.append('--'+key.replace('_','-'))
            if value is not True:
                command.append(str(value))
        run = subprocess.run(command,capture_output=True,text=True,timeout=240)
        if run.returncode:
            raise RuntimeError(f'{name}: {run.stdout}\n{run.stderr}')
        with Image.open(folder/'jupiter.png') as image:
            return image.convert('RGB')

    baseline = capture('baseline')
    assert max(hi for lo,hi in baseline.getextrema()) > 80,'Jupiter is blank'
    metrics = json.loads((args.output/'baseline/metrics.json').read_text())
    assert metrics['sceneSamples'] == 1,'Jupiter allocated redundant MSAA'
    black = capture('black',brightness=0)
    assert max(hi for lo,hi in black.getextrema()) <= 1,'Zero brightness leaves light'
    frozen = capture('frozen',movement=0)
    later = capture('frozen-audio',movement=0,start_time=450,reaction='expressive',strength=1,energy=1)
    assert difference(frozen,later) == 0,'Movement zero does not freeze time/audio'
    audio = capture('audio',reaction='expressive',strength=1,energy=1)
    assert difference(baseline,audio) > .01,'Audio has no effect'
    for key,phase in (('detail',0),('flow',225),('turbulence',225),('density',0)):
        low = capture(key+'-zero',start_time=phase,**{key:0})
        high = capture(key+'-two',start_time=phase,**{key:2})
        change = difference(low,high)
        assert change > .001,f'{key} has no independent effect'
        results[key+'MeanDifference255'] = change
    for palette in (1,2,3):
        assert difference(baseline,capture(f'palette-{palette}',palette=palette)) > .01
    for quality in ('eco','high'):
        assert difference(baseline,capture('quality-'+quality,quality=quality)) == 0
    complete = capture('full-turn',start_time=900)
    results['fullTurnMeanDifference255'] = difference(baseline,complete)
    assert results['fullTurnMeanDifference255'] < .1,'Jupiter composition did not repeat in 900 seconds'
    wind_start = capture('wind-start',start_time=225,flow=0,turbulence=0,jupiter_probe=1)
    wind_end = capture('wind-end',start_time=225,flow=0,turbulence=2,jupiter_probe=1)
    assert difference(wind_start,wind_end) > .01,'Inferred wind map does not advect observed clouds'
    # Wind affects clouds only: background and spheroid coverage stay fixed.
    assert difference(wind_start.crop((0,0,100,360)),wind_end.crop((0,0,100,360))) == 0

    manifest = json.loads((ROOT/'assets/jupiter/sources.json').read_text())
    with Image.open(ROOT/'assets/jupiter/runtime/jupiter-color.dds') as source:
        original = source.convert('RGB')
        for name,landmark in manifest['landmarks'].items():
            x = int(landmark['longitudeEast']/360*original.width)%original.width
            y = min(int((90-landmark['latitudeGraphic'])/180*original.height),original.height-1)
            assert max(abs(a-b) for a,b in zip(original.getpixel((x,y)),landmark['sourceRgb'])) < 18,f'Source conversion mismatch: {name}'
        reference = original.resize((1024,512),Image.Resampling.LANCZOS)
    with Image.open(ROOT/'assets/jupiter/runtime/jupiter-atmosphere.dds') as source:
        # BC5 is two independently linear data channels, not sRGB imagery.
        atmosphere = source.convert('RGB').resize((1024,512),Image.Resampling.BILINEAR)
    side = unit((.65,.20))
    light = (side[0]*math.sin(math.radians(8)),side[1]*math.sin(math.radians(8)),math.cos(math.radians(8)))
    for phase in (0,225,450,675):
        albedo = capture(f'geography-{phase}',start_time=phase,flow=0,turbulence=0,jupiter_probe=1,brightness=.2)
        haze = capture(f'atmosphere-{phase}',start_time=phase,flow=0,turbulence=0,jupiter_probe=2,brightness=.2)
        night = capture(f'night-{phase}',start_time=phase,flow=0,turbulence=0,brightness=.03)
        errors,haze_errors,dark = [],[],[]
        for y in range(70,290,8):
            for x in range(225,470,8):
                point = surface(x,y,phase)
                if point is None or point[1][2] < .3:
                    continue
                normal,world,(u,v) = point
                rgb = reference.getpixel((int(u*1024)%1024,min(int(v*512),511)))
                errors.extend(abs(a-b) for a,b in zip(albedo.getpixel((x,y)),display(rgb)))
                h = atmosphere.getpixel((int(u*1024)%1024,min(int(v*512),511)))[0]/255
                expected = 255*(1-math.exp(-h*.2))**(1/2.2)
                haze_errors.extend(abs(c-expected) for c in haze.getpixel((x,y)))
                if dot(world,light) < -.2:
                    dark.append(max(night.getpixel((x,y))))
        error = sum(errors)/len(errors);haze_error = sum(haze_errors)/len(haze_errors)
        assert error < 9,f'Incorrect System III longitude, latitude or spin at {phase}: {error}'
        assert haze_error < 9,f'Methane proxy misaligned at {phase}: {haze_error}'
        assert not dark or max(dark) <= 1,'Atmosphere adds uniform night illumination'
        results[f'geography{phase}MeanError255'] = error
        results[f'atmosphere{phase}MeanError255'] = haze_error
        results[f'night{phase}Max255'] = max(dark,default=0)
        assert difference(baseline,night) > 1

    mask = capture('oblate-mask',flow=0,turbulence=0,jupiter_probe=3)
    # Select the central connected globe, excluding isolated bright catalog
    # stars that legitimately exceed the same intensity threshold.
    silhouette = mask.convert('L').point(lambda v:255 if v>75 else 0)
    ImageDraw.floodfill(silhouette,(347,177),128)
    predicted,actual = 0,0
    mismatches = 0
    for y in range(360):
        for x in range(640):
            inside = surface(x,y,0) is not None
            visible = silhouette.getpixel((x,y)) == 128
            predicted += inside;actual += visible;mismatches += inside != visible
    assert mismatches < 100,f'Spheroid silhouette differs from measured radii: {mismatches}'
    results['oblateMaskPixels'] = {'predicted':predicted,'actual':actual,'mismatches':mismatches}
    daylight = capture('near-full-lighting',flow=0,turbulence=0,jupiter_probe=4)
    day_mask = daylight.convert('L').point(lambda v:255 if v>75 else 0)
    ImageDraw.floodfill(day_mask,(347,177),128)
    day_pixels = sum(v==128 for v in day_mask.get_flattened_data())
    fraction = day_pixels/actual
    assert .99 < fraction <= 1.001,f'Jupiter lighting introduced a broad dark crescent: {fraction}'
    unlit = capture('narrow-night-limb',flow=0,turbulence=0,brightness=.03)
    night_pixels = []
    for y in range(45,315):
        for x in range(205,290):
            point = surface(x,y,0)
            if point and point[1][2]>.115 and dot(point[1],light)<-.004:
                night_pixels.append(max(unlit.getpixel((x,y))))
    assert night_pixels and max(night_pixels)<=1,'Near-full night limb is lit or untested'
    results['illuminatedDiscFraction'] = fraction
    results['narrowNightSamples'] = len(night_pixels)
    results['narrowNightMax255'] = max(night_pixels)
    profile = json.loads((ROOT/'assets/jupiter/wind-profile.json').read_text())
    assert 33000 < profile['elapsedSeconds'] < 35000
    angular = profile['angularRadiansPerSecond']
    assert len(angular) == 1800 and max(angular) > .000001 and min(angular) < 0,'Zonal wind profile has lost opposing jets'
    assert max(abs(v) for v in angular) <= .00001
    assert all(v == 0 for v in angular[:200]+angular[1600:]),'Invented polar winds'
    capture('lifecycle',validate_lifecycle=True,frames=18)
    capture('fallback-lifecycle',validate_lifecycle=True,single_sample=True,frames=18)
    capture('allocation-large',width=1280,height=720)
    large = json.loads((args.output/'allocation-large/metrics.json').read_text())
    assert abs(large['estimatedGpuBytes']-metrics['estimatedGpuBytes']-(1280*720-640*360)*17) < 200000,'Unexpected scene depth allocation'
    results['passed'] = True
    (args.output/'results.json').write_text(json.dumps(results,indent=2)+'\n')
    print('PASS Jupiter: OPAL geography, methane alignment, wind advection, oblate shape, controls, frozen audio and GPU lifecycle')


if __name__ == '__main__':
    main()
