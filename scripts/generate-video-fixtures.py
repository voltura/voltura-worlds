"""Generate synthetic native-video fixtures; FFmpeg is a development-only tool."""
from pathlib import Path
import sys, subprocess, json
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.deps/python'))
import imageio_ffmpeg
ffmpeg=imageio_ffmpeg.get_ffmpeg_exe()
out=ROOT/'artifacts/broader-video/fixtures';out.mkdir(parents=True,exist_ok=True)
import hashlib
stamp=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
if (out/'generation.json').exists():
 previous=json.loads((out/'generation.json').read_text())
 if previous.get('sourceHash')==stamp and all((out/name).is_file() for name,status in previous['fixtures'].items() if status['generated']):
  print('Synthetic video fixtures already current');sys.exit(0)
results={}
def run(name,*options,optional=False):
 command=[ffmpeg,'-hide_banner','-loglevel','error','-y',*options,str(out/name)]
 p=subprocess.run(command,capture_output=True,text=True,timeout=120)
 if p.returncode:
  if not optional:raise RuntimeError(name+': '+p.stderr)
  results[name]={'generated':False,'reason':p.stderr[-1000:]}
 else:results[name]={'generated':True}
def source(duration='2',size='320x180'):
 return ['-f','lavfi','-i',f'testsrc2=size={size}:rate=30','-t',duration,'-an']
for name,codec in [('h264.mp4','libx264'),('vp8.webm','libvpx'),('vp9.webm','libvpx-vp9'),('vp9.mkv','libvpx-vp9'),('wmv.wmv','wmv2'),('mjpeg.avi','mjpeg')]:
 run(name,*source(),'-c:v',codec,'-threads','2','-pix_fmt','yuvj420p' if codec=='mjpeg' else 'yuv420p')
for ext in ['mov','mkv','m4v']:
 run('h264.'+ext,'-i',str(out/'h264.mp4'),'-an','-c:v','copy')
for degrees in [90,180,270]:
 run(f'rotate-{degrees}.mov','-display_rotation:v:0',str(degrees),'-i',str(out/'h264.mp4'),'-c','copy')
run('sar.mp4',*source(),'-vf','setsar=2/1','-c:v','libx264','-pix_fmt','yuv420p')
run('portrait.mp4',*source(size='180x320'),'-c:v','libx264','-pix_fmt','yuv420p')
run('vfr.mp4',*source(duration='3'),'-vf',"select='if(lt(t,1),1,not(mod(n,3)))'",'-fps_mode','vfr','-c:v','libx264','-pix_fmt','yuv420p')
run('hdr.mp4',*source(),'-vf','setparams=color_primaries=bt2020:color_trc=smpte2084:colorspace=bt2020nc','-c:v','libx264','-x264-params','colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc','-movflags','write_colr','-color_trc','smpte2084','-color_primaries','bt2020','-colorspace','bt2020nc','-pix_fmt','yuv420p')
run('hevc.mp4',*source(),'-c:v','libx265','-x265-params','pools=1:log-level=error','-pix_fmt','yuv420p',optional=True)
run('av1.mkv',*source(duration='1'),'-c:v','libaom-av1','-cpu-used','8','-threads','2','-pix_fmt','yuv420p',optional=True)
run('audio-only.mp4','-f','lavfi','-i','sine=frequency=440','-t','1','-c:a','aac')
run('protected.mp4','-i',str(out/'h264.mp4'),'-c','copy','-encryption_scheme','cenc-aes-ctr','-encryption_key','0123456789abcdef0123456789abcdef','-encryption_kid','fedcba9876543210fedcba9876543210',optional=True)
run('rates.mp4',*source(duration='12'),'-c:v','libx264','-pix_fmt','yuv420p')
run('markers.mp4','-f','lavfi','-i','color=c=black:size=320x180:rate=30','-t','2','-vf','drawbox=x=0:y=0:w=80:h=180:color=red:t=fill,drawbox=x=80:y=0:w=80:h=180:color=green:t=fill,drawbox=x=160:y=0:w=80:h=180:color=blue:t=fill,drawbox=x=240:y=0:w=80:h=180:color=yellow:t=fill','-an','-c:v','libx264','-pix_fmt','yuv420p')
run('markers-vertical.mp4','-i',str(out/'markers.mp4'),'-vf','transpose=1','-an','-c:v','libx264','-pix_fmt','yuv420p')
# A genuinely PQ-tagged bitstream with no container colr tag verifies SPS checks.
data=bytearray((out/'hdr.mp4').read_bytes());at=data.find(b'colr')
if at>=0:data[at:at+4]=b'free'
(out/'hdr-vui.mp4').write_bytes(data);results['hdr-vui.mp4']={'generated':True}
results['malformed.mp4']={'generated':True}
(out/'malformed.mp4').write_bytes(b'\0\0\0\x20ftypisom'+b'\0'*25)
(out/'generation.json').write_text(json.dumps({'sourceHash':stamp,'fixtures':results},indent=2))
print('Generated synthetic video fixtures in',out)
