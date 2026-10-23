#!/usr/bin/env python3
"""Independent image-UI Zig module exported and launched by the general Player."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('export_game',ROOT/'Tools/TwoD/ExportGame.py')
exporter=importlib.util.module_from_spec(spec);spec.loader.exec_module(exporter)
parser=argparse.ArgumentParser();parser.add_argument('--app',type=Path,required=True);parser.add_argument('--module',type=Path,required=True);parser.add_argument('--shipping',action='store_true');args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='nexora-image-ui-') as temporary:
    root=Path(temporary);shutil.copy2(args.module,root/args.module.name)
    for index,(width,height) in enumerate([(1920,1080),(1024,768),(3441,1081)]):
        desc=dict(schema=1,id='com.nexora.imageui',title='Zig Image UI',scene='Main',module=args.module.name,audio='off',assets=[],display=dict(width=width,height=height,mode='windowed',resizable=True))
        config=root/'game.json';config.write_text(json.dumps(desc),encoding='utf-8')
        output=root/f'package-{index}';exporter.export(config,args.app,output,allow_development=not args.shipping)
        env=os.environ.copy()
        if sys.platform.startswith('linux'):env['LD_LIBRARY_PATH']=str(output)+os.pathsep+env.get('LD_LIBRARY_PATH','')
        result=subprocess.run([str(output/args.app.name),'--run-2d-project',str(output/'game.nx2game'),'--frames','4'],capture_output=True,text=True,env=env,timeout=30,encoding='utf-8')
        assert result.returncode==0,(result.stdout,result.stderr)
        evidence=json.loads(result.stdout)
        assert evidence['ui_frames']==4 and evidence['allocations_balanced']
        assert evidence['final_width']==width and evidence['final_height']==height
        assert '陰陽 生存' in result.stderr
        assert json.loads((output/'inventory.json').read_text())['development_test_copy']==(not args.shipping)
print('PASS: independent Zig image HUD, real raster font, relocated Player/export at three extents')
