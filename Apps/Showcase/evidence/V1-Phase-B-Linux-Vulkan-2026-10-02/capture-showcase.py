import os, sys, subprocess, time, json
from pathlib import Path
from PIL import ImageGrab
sys.path.insert(0, '/workspace/Nexora/Tests/Showcase')
from LinuxVirtualDisplaySmoke import start_xvfb
out=Path('/workspace/Nexora/build/linux-development/artifacts/showcase-linux-vulkan-interactive')
out.mkdir(exist_ok=True)
server,display=start_xvfb('Xvfb','1280x720x24')
assert display
env=os.environ.copy(); env['DISPLAY']=display
command=['/workspace/Nexora/build/linux-development/Apps/Showcase/NexoraShowcase','--mode=interactive','--scene=rendering','--backend=vulkan','--frames=600','--no-reload','--gameplay-module=static',f'--report={out / "scene.json"}']
try:
 with (out/'stdout.log').open('w') as stdout, (out/'stderr.log').open('w') as stderr:
  app=subprocess.Popen(command,env=env,stdout=stdout,stderr=stderr)
  window=subprocess.check_output(['xdotool','search','--sync','--onlyvisible','--name','^Nexora Showcase$'],env=env,timeout=5,text=True).splitlines()[0]
  deadline=time.monotonic()+5
  while time.monotonic()<deadline:
   im=ImageGrab.grab(xdisplay=display)
   # Rendering Room background is dark blue, cube lit blue: retain an actual non-background frame.
   pixels=list(im.resize((64,36)).getdata())
   if any(b>120 and b>r*1.1 for r,g,b in pixels):
    im.save(out/'rendering-room.png')
    break
  else: raise RuntimeError('No visible lit cube captured')
  subprocess.run(['xdotool','keydown','--window',window,'d'],env=env,check=True)
  time.sleep(.03)
  subprocess.run(['xdotool','keyup','--window',window,'d'],env=env,check=True)
  code=app.wait(timeout=30)
 report=json.loads((out/'scene.json').read_text())
 assert code==0 and report['interaction_overlay']['camera_moves']>0, report.keys()
 print('PASS',report['interaction_overlay'])
 (out/'launch.json').write_text(json.dumps({'command':command,'exit_code':code,'camera_moves':report['interaction_overlay']['camera_moves'],'capture':'rendering-room.png','scope':'Xvfb/lavapipe; physical display unverified'},indent=2))
finally:
 if 'app' in locals() and app.poll() is None:
  app.terminate(); app.wait(timeout=3)
 server.terminate();server.wait(timeout=3)
