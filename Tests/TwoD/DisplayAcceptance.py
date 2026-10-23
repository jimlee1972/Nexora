#!/usr/bin/env python3
"""Export and run an independent Zig display consumer; headless evidence is not native acceptance."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('export_game', ROOT/'Tools/TwoD/ExportGame.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', type=Path, required=True)
    parser.add_argument('--module', type=Path, required=True)
    parser.add_argument('--shipping', action='store_true')
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='nexora-display-') as temporary:
        root = Path(temporary)
        shutil.copy2(args.module, root/args.module.name)
        config = dict(schema=1,id='com.nexora.display',title='Zig Display',scene='Main',
                      module=args.module.name,audio='off',assets=[])
        descriptor = root/'game.json'
        for index, (width,height,viewport) in enumerate([
                (1920,1080,(0,0,1920,1080)), (1024,768,(0,96,1024,576)),
                (3441,1081,(459,0,2522,1081))]):
            config['display'] = dict(width=width,height=height,mode='windowed',resizable=True)
            descriptor.write_text(json.dumps(config),encoding='utf-8')
            package = root/f'package-{index}'
            exporter.export(descriptor,args.app,package,allow_development=not args.shipping)
            assert json.loads((package/'inventory.json').read_text())['development_test_copy'] == (not args.shipping)
            assert f'display {width} {height} windowed 1' in (package/'game.nx2game').read_text()
            env = os.environ.copy()
            if sys.platform.startswith('linux'):
                env['LD_LIBRARY_PATH'] = str(package)+os.pathsep+env.get('LD_LIBRARY_PATH','')
            result = subprocess.run([str(package/args.app.name),'--run-2d-project',str(package/'game.nx2game'),
                                     '--frames','4'],capture_output=True,text=True,env=env,timeout=30)
            assert result.returncode == 0, (result.stdout,result.stderr)
            report = json.loads(result.stdout)
            assert report['final_width']==width and report['final_height']==height
            assert tuple(report[f'viewport_{key}'] for key in ('x','y','width','height')) == viewport, report
            assert report['ui_frames']==4 and report['allocations_balanced']
            assert 'SP-01 Zig display consumer verified' in result.stderr
        for index,bad in enumerate([None, {},dict(width=0,height=1080,mode='windowed',resizable=True),
                dict(width=1920.5,height=1080,mode='windowed',resizable=True),
                dict(width=True,height=1080,mode='windowed',resizable=True),
                dict(width=1920,height=1080,mode='exclusive',resizable=True),
                dict(width=1920,height=1080,mode='windowed',resizable=1)]):
            config['display']=bad
            descriptor.write_text(json.dumps(config))
            rejected = root/f'rejected-{index}'
            try:
                exporter.export(descriptor,args.app,rejected,allow_development=True)
            except ValueError:
                pass
            else:
                raise AssertionError(f'invalid display was admitted: {bad}')
            assert not rejected.exists()
        descriptor.write_text('{"schema":1,"display":{},"display":{}}')
        try:
            exporter.export(descriptor,args.app,root/'duplicate',allow_development=True)
        except ValueError:
            pass
        else:
            raise AssertionError('duplicate JSON field admitted')
    print('PASS: Zig display service, independent export, viewport, UI and invalid descriptors')
if __name__ == '__main__':
    main()
