"""Record the actual native 100-second courtyard tour on Xvfb; no hardware acceptance claim."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'Tests/Showcase'))
from LinuxVirtualDisplaySmoke import start_xvfb

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('executable',type=Path)
parser.add_argument('--output',type=Path,required=True)
parser.add_argument('--quality',choices=['basic','standard','high'])
args=parser.parse_args()
output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
for tool in ['Xvfb','xdotool','ffmpeg','ffprobe']:
    if not shutil.which(tool):raise SystemExit(f'{tool} is required for this offline evidence tool')
server,display=start_xvfb(shutil.which('Xvfb'),'1280x720x24')
app=capture=None
try:
    if display is None:raise RuntimeError('Xvfb did not start')
    environment=os.environ.copy();environment['DISPLAY']=display
    executable=args.executable.resolve();report=output/'tour.json'
    command=[str(executable),'--tour=visual','--backend=vulkan','--clean-view','--vsync=off',
             '--no-reload','--gameplay-module=static',f'--report={report}']
    if args.quality:command.append(f'--quality={args.quality}')
    with (output/'stdout.log').open('w') as stdout,(output/'stderr.log').open('w') as stderr:
        started=time.monotonic()
        app=subprocess.Popen(command,env=environment,stdout=stdout,stderr=stderr)
        subprocess.run(['xdotool','search','--sync','--onlyvisible','--name','^Nexora Showcase$'],
                       env=environment,capture_output=True,check=True,timeout=15)
        capture=subprocess.Popen(['ffmpeg','-hide_banner','-loglevel','error','-y','-f','x11grab',
          '-framerate','15','-video_size','1280x720','-draw_mouse','0','-i',display+'+0,0','-c:v','libx264',
          '-threads','2','-preset','fast','-crf','23','-pix_fmt','yuv420p',str(output/'visual-tour.mp4')],
          env=environment,stdin=subprocess.PIPE,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,text=True)
        app.wait(timeout=180)
        elapsed=time.monotonic()-started
        _,errors=capture.communicate('q\n',timeout=15)
        if app.returncode or capture.returncode:raise RuntimeError(f'Native/capture failure: {app.returncode}/{capture.returncode}: {errors}')
    data=json.loads(report.read_text());tour=data['runtime_rooms']['tour'];native=data['windowed_evidence']
    assert data['status']=='PASS' and tour['kind']=='visual' and tour['seconds']==100 and tour['paused']
    assert native['executed'] and native['backend']=='vulkan' and not native['backend_fallback']
    assert native['overlay_frames']==0 and native['native_scene_draws']>100
    assert native['surface_acquires']==native['surface_presents']+native['surface_recoverable_presents']
    video=json.loads(subprocess.check_output(['ffprobe','-v','error','-show_entries',
                  'format=duration:stream=width,height,codec_name','-of','json',str(output/'visual-tour.mp4')],text=True))
    assert 90 <= float(video['format']['duration']) <= 120, 'Native recording duration is outside the visual tour budget'
    for second in [5,30,55,80,95]:
        subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-y','-ss',str(second),
                        '-i',str(output/'visual-tour.mp4'),'-frames:v','1',str(output/f'shot-{second}.png')],check=True)
    result={'scope':'Linux Xvfb actual Vulkan application/video; software GPU is not target hardware',
            'command':command,'build':data['build'],'wall_seconds':elapsed,'video':video,
            'software_rasterizer':native['software_rasterizer'],'physical_display_verified':False,
            'executable_sha256':hashlib.sha256(executable.read_bytes()).hexdigest(),
            'video_sha256':hashlib.sha256((output/'visual-tour.mp4').read_bytes()).hexdigest(),
            'tour_seconds':100,'overlay_frames':0}
    (output/'recording.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(result,indent=2))
finally:
    if app and app.poll() is None:app.kill();app.wait(timeout=5)
    if capture and capture.poll() is None:capture.kill();capture.wait(timeout=5)
    server.terminate();server.wait(timeout=3)
