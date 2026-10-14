#!/usr/bin/env python3
"""Exercise real PNG slicing + canonical C++ packer, deterministic output and failure publication."""
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import zlib

spec = importlib.util.spec_from_file_location('assets', Path(__file__).parents[2]/'Tools/TwoD/PrepareAssets.py')
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)

def chunk(kind, data):
    return struct.pack('>I', len(data))+kind+data+struct.pack('>I', zlib.crc32(kind+data))

def main():
    compiler = Path(sys.argv[1])
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        raw = b'\x00'+bytes([255,0,0,255,0,255,0,255])+b'\x00'+bytes([0,0,255,255,255,255,255,255])
        png = b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR', struct.pack('>IIBBBBB', 2,2,8,6,0,0,0))+chunk(b'IDAT', zlib.compress(raw))+chunk(b'IEND', b'')
        (root/'sheet.png').write_bytes(png)
        manifest = {'type':'atlas','id':[11,12],'width':8,'nearest':True,'images':[{'path':'sheet.png','grid':[1,1]}]}
        output = root/'atlas.nx2atlas'
        assets.compile_manifest(manifest, root, compiler, output)
        first = output.read_bytes()
        assets.compile_manifest(manifest, root, compiler, output)
        assert output.read_bytes() == first
        manifest['images'][0] = {'path':'sheet.png','regions':[{'id':4,'rect':[1,1,1,1]},{'id':3,'rect':[0,1,1,1]},{'id':2,'rect':[1,0,1,1]},{'id':1,'rect':[0,0,1,1]}]}
        assets.compile_manifest(manifest, root, compiler, output)
        assert output.read_bytes() == first
        for bad in [png[:-1], png+b'trailing', b'not a png']:
            try: assets.png(bad)
            except ValueError: pass
            else: raise AssertionError('invalid PNG accepted')
        manifest['images'][0]['path'] = '../outside.png'
        try: assets.compile_manifest(manifest, root, compiler, output)
        except ValueError: pass
        else: raise AssertionError('path escape accepted')
        assert output.read_bytes() == first
        clip = {'type':'clip','id':[13,14],'atlas':[11,12],'frames':[{'region':1,'seconds':0.1,'event':'tick'},{'region':2,'seconds':0.2}]}
        assets.compile_manifest(clip, root, compiler, root/'clip.nx2clip')
        before = (root/'clip.nx2clip').read_bytes()
        assets.compile_manifest(clip, root, compiler, root/'clip.nx2clip')
        assert before == (root/'clip.nx2clip').read_bytes()
    print('PNG slicing, packing, clips and failed publication passed')

if __name__ == '__main__': main()
