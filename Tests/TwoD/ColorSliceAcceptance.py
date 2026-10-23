#!/usr/bin/env python3
"""Generate sRGB art fixtures, use the real packer/exporter and run the independent Zig module."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
ROOT = Path(__file__).resolve().parents[2]
def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
prepare = load('prepare', ROOT/'Tools/TwoD/PrepareAssets.py')
exporter = load('export_game', ROOT/'Tools/TwoD/ExportGame.py')
def chunk(kind, payload):
    return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload))
def fixture(root, compiler):
    colors=[(192,160,120),(64,128,192),(128,128,128),(255,192,64),(0,0,0),(255,32,32),(128,128,128)]
    rows=[]
    for y in range(32):
        row=bytearray([0])
        for region,rgb in enumerate(colors):
            for x in range(32):
                alpha=255
                if region in (3,4): alpha=round(180*max(0,1-((x-15.5)**2+(y-15.5)**2)/240)**2)
                if region==5: alpha=255 if min(x,y,31-x,31-y)<3 else 0
                row.extend((*rgb,alpha))
        rows.append(row)
    (root/'sheet.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',224,32,8,6,0,0,0))+
        chunk(b'sRGB',b'\x00')+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b''))
    manifest=dict(type='atlas',id=[44,4],width=256,color_space='sRGB',alpha_mode='straight',
                  images=[dict(path='sheet.png',grid=[32,32])])
    prepare.compile_manifest(manifest,root,compiler,root/'slice.nx2atlas')
    subprocess.run([str(compiler),'cook',str(root/'slice.nx2atlas'),str(root/'slice.nxcook')],check=True,timeout=30)
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--app',type=Path,required=True)
    parser.add_argument('--module',type=Path,required=True)
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--shipping',action='store_true')
    args=parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='nexora-color-') as temporary:
        root=Path(temporary);fixture(root,args.compiler)
        shutil.copy2(args.module,root/args.module.name)
        for index,(width,height) in enumerate([(1920,1080),(1024,768),(3441,1081)]):
            config=dict(schema=1,id='com.nexora.color',title='Zig Color Slice',scene='Main',module=args.module.name,
                        audio='off',assets=[dict(kind='atlas',path='slice.nxcook')],
                        display=dict(width=width,height=height,mode='windowed',resizable=True))
            descriptor=root/'game.json';descriptor.write_text(json.dumps(config),encoding='utf-8')
            output=root/f'package-{index}'
            exporter.export(descriptor,args.app,output,allow_development=not args.shipping)
            env=os.environ.copy()
            if sys.platform.startswith('linux'): env['LD_LIBRARY_PATH']=str(output)+os.pathsep+env.get('LD_LIBRARY_PATH','')
            result=subprocess.run([str(output/args.app.name),'--run-2d-project',str(output/'game.nx2game'),'--frames','4'],
                                  capture_output=True,text=True,env=env,timeout=30,encoding='utf-8')
            assert result.returncode==0,(result.stdout,result.stderr)
            report=json.loads(result.stdout)
            assert report['ui_frames']==4 and report['visible']==5 and report['allocations_balanced'],report
            assert 'SP-04 PASS:' in result.stderr,result.stderr
            assert json.loads((output/'inventory.json').read_text())['development_test_copy']==(not args.shipping)
    print('PASS: explicit sRGB import/cook, Zig-only layered fallback, reduced effects, independent Player export at three sizes')
if __name__=='__main__': main()
