import pathlib, subprocess, sys, hashlib, json
root=pathlib.Path(__file__).resolve().parents[1]
phase=sys.argv[1]
assert phase in ("before", "after")
scenes='aurora orbit smoke bloom silk eclipse lattice drift contour plastic_metal ripples mars earth moon jupiter saturn mercury venus uranus neptune pluto sun'.split()
result={}
for scene in scenes:
 out=root/'artifacts/hdr'/phase/scene
 subprocess.run([sys.executable,str(root/'scripts/test_desktop.py'),'--',str(root/'build/Release/VolturaWorlds.exe'),'--capture-dir',str(out),'--scene',scene,'--width','320','--height','180','--frames','12','--grid','24','--debug-gpu'],check=True,timeout=180)
 result[scene]=hashlib.sha256((out/(scene+'.png')).read_bytes()).hexdigest()
 print(scene,result[scene],flush=True)
(root/'artifacts/hdr'/f'{phase}-hashes.json').write_text(json.dumps(result,indent=2))
if phase=='after':
 assert result==json.loads((root/'artifacts/hdr/before-hashes.json').read_text()),'SDR baseline changed'
