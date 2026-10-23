#!/usr/bin/env python3
"""Exercise real PNG slicing + canonical C++ packer, deterministic output and failure publication."""
import importlib.util
from pathlib import Path
import struct
import os
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
        if os.name != 'nt':
            alias = root/'root-alias'
            alias.symlink_to(root, target_is_directory=True)
            assets.compile_manifest(manifest, alias, compiler, output)
            assert output.read_bytes() == first
        for bad in [png[:-1], png+b'trailing', b'not a png']:
            try: assets.png(bad)
            except ValueError: pass
            else: raise AssertionError('invalid PNG accepted')
        transfer = assets.srgb_to_linear_rgba8(bytes(v for i in range(256) for v in (i,i,i,255)))
        golden = 2166136261
        for value in transfer[::4]: golden = ((golden ^ value)*16777619)&0xffffffff
        assert golden == 0x32d9e413
        assert transfer[3::4] == bytes([255])*256
        # Explicit source transfer: reference values independent of the implementation LUT.
        swatches = bytes([0,10,11,0, 64,128,192,64, 255,128,32,128, 128,128,128,255])
        reference = bytes([0,1,1,0, 13,55,134,64, 255,55,4,128, 55,55,55,255])
        color_png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 2,2,8,6,0,0,0))
                     + chunk(b'sRGB', b'\x00') + chunk(b'gAMA', struct.pack('>I',45455))
                     + chunk(b'IDAT', zlib.compress(b'\x00'+swatches[:8]+b'\x00'+swatches[8:])) + chunk(b'IEND',b''))
        assert assets.png(color_png)[2] == swatches  # historical default is unchanged
        assert assets.png(color_png,'sRGB')[2] == reference
        assert assets.srgb_to_linear_rgba8(bytes([128,128,128,0])) == bytes([55,55,55,0])
        (root/'color.png').write_bytes(color_png)
        color_manifest = dict(type='atlas',id=[21,22],width=8,color_space='sRGB',alpha_mode='straight',
                              images=[dict(path='color.png',regions=[dict(id=1,rect=[0,0,2,2])])])
        color_output=root/'color.nx2atlas'
        assets.compile_manifest(color_manifest, root, compiler, color_output)
        cooked=color_output.read_bytes()
        # Atlas wire header 44 + one 20-byte region + length-prefixed pixel array.
        width,height=struct.unpack_from('<II',cooked,24)
        length=struct.unpack_from('<I',cooked,64)[0]
        assert length == width*height*4
        rgba=cooked[68:]
        assert rgba[(width+1)*4:(width+3)*4] == reference[:8]
        assert rgba[(2*width+1)*4:(2*width+3)*4] == reference[8:]
        assert rgba[:4] == reference[:4]  # extrusion happens after RGB conversion
        assets.compile_manifest(color_manifest, root, compiler, color_output)
        assert color_output.read_bytes()==cooked
        for field,value in [('color_space','display-p3'),('color_space',None),('alpha_mode','premultiplied')]:
            bad=dict(color_manifest);bad[field]=value
            try: assets.compile_manifest(bad,root,compiler,color_output)
            except ValueError: pass
            else: raise AssertionError('invalid color contract admitted')
            assert color_output.read_bytes()==cooked
        for kind,payload in [(b'iCCP',b'profile'),(b'gAMA',struct.pack('>I',100000)),(b'sRGB',b'\x05'),(b'cHRM',b'')]:
            bad=color_png.replace(chunk(b'sRGB',b'\x00'),chunk(kind,payload))
            try: assets.png(bad,'sRGB')
            except ValueError: pass
            else: raise AssertionError('unsupported/conflicting profile accepted')
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
