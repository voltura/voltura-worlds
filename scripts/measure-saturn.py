"""Alternate Jupiter/Saturn native offscreen timing runs with identical settings."""
import argparse
import json
from pathlib import Path
import statistics
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('executable',type=Path);p.add_argument('--output',type=Path,default=ROOT/'artifacts/saturn/performance');a=p.parse_args();report={}
    for w,h in ((1920,1080),(3840,2160)):
        runs={'jupiter':[],'saturn':[]}
        for repeat in range(3):
            for scene in (('jupiter','saturn') if repeat%2==0 else ('saturn','jupiter')):
                folder=a.output/f'{w}x{h}-{scene}-{repeat}';folder.mkdir(parents=True,exist_ok=True)
                command=[sys.executable,str(ROOT/'scripts/test_desktop.py'),'--',str(a.executable.resolve()),'--capture-dir',str(folder),'--scene',scene,'--width',str(w),'--height',str(h),'--frames','300','--timing-warmup','60','--quality','high']
                run=subprocess.run(command,capture_output=True,text=True,timeout=300)
                if run.returncode:raise RuntimeError(run.stdout+run.stderr)
                metrics=json.loads((folder/'metrics.json').read_text());assert metrics['timingSampleCount']==240
                runs[scene].append(metrics)
        medians={scene:statistics.median(m['gpuMedianMs'] for m in values) for scene,values in runs.items()}
        ratio=medians['saturn']/medians['jupiter']
        report[f'{w}x{h}']={'gpuMedianMs':medians,'saturnToJupiterRatio':ratio,'within25PercentTarget':ratio<=1.25,'textureGpuBytes':{s:values[0]['textureGpuBytes'] for s,values in runs.items()},'adapter':runs['saturn'][0]['adapter']}
        print(f'{w}x{h}: '+json.dumps(report[f'{w}x{h}']),flush=True)
    report['evidence']='Three alternating runs per scene/resolution; 300 frames, 60 warmup; native offscreen GPU timestamps, not physical desktop performance.'
    (a.output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
if __name__=='__main__':main()
