"""GPU upload/readback roundtrip and malformed capture-volume rejection."""
import math
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

scripts = Path(__file__).resolve().parent
with tempfile.TemporaryDirectory(prefix='Voltura-reference-') as directory:
    root = Path(directory)
    n = 48
    data = bytearray()
    for z in range(n):
        for y in range(n):
            for x in range(n):
                density = .2*math.exp(-sum((a-b)**2 for a,b in zip((x/n,y/n,z/n),(.35,.6,.45)))/.02)
                data += struct.pack('<4e', density*.2, density*.3, density*.5, density)
    source = root/'reference.bin'
    source.write_bytes(data)
    base = [sys.executable,str(scripts/'test_desktop.py'),'--',sys.argv[1],
            '--scene','smoke','--grid','24','--width','640','--height','360',
            '--frames','2','--debug-gpu','--fluid-diagnostics']
    subprocess.run(base+['--capture-dir',str(root/'valid'),'--smoke-reference-volume',str(source)],check=True)
    assert (root/'valid'/'density-rgba16f.bin').read_bytes() == data, 'Upload changed frozen density'
    # Decouple frozen density resolution from the velocity-grid-derived default.
    override = base.copy()
    override[override.index('--grid')+1] = '32'
    subprocess.run(override+['--capture-dir',str(root/'override'),
                            '--smoke-reference-volume',str(source),
                            '--reference-density-grid','48'],check=True)
    assert (root/'override'/'density-rgba16f.bin').read_bytes() == data, 'Override changed frozen density'
    for value in ['0','23','49','388','-4','48garbage']:
        result = subprocess.run(base+['--capture-dir',str(root/('invalid-grid-'+value)),
                                     '--smoke-reference-volume',str(source),
                                     '--reference-density-grid',value])
        assert result.returncode != 0, f'Accepted invalid reference grid: {value}'
    for extra in [[], ['--smoke-reference-volume',str(source),'--wide-density'],
                  ['--smoke-reference-volume',str(source),'--transport-test','maccormack']]:
        result = subprocess.run(base+['--capture-dir',str(root/'invalid-mode'),
                                     '--reference-density-grid','48']+extra)
        assert result.returncode != 0, 'Accepted incompatible reference grid mode'
    for name, payload in [('truncated',data[:-2]),('extended',data+b'\0\0'),
                          ('nan',struct.pack('<H',0x7e00)+data[2:]),
                          ('negative',struct.pack('<H',0xbc00)+data[2:])]:
        invalid = root/(name+'.bin')
        invalid.write_bytes(payload)
        result = subprocess.run(base+['--capture-dir',str(root/name),'--smoke-reference-volume',str(invalid)])
        assert result.returncode != 0, f'Accepted invalid reference: {name}'
    print('PASS: frozen reference roundtrips including grid override are byte-identical; malformed inputs and incompatible modes rejected.')
