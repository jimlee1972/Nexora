#!/usr/bin/env python3
"""Actual paired slicing/repacking, linear data, immutable publication and exported inventory."""
import copy
import importlib.util
import json
import hashlib
import shutil
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'Tools/TwoD'))
import PrepareEmission as emission
import ExportGame as exporter


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def png(width, height, red):
    raw = b''.join(b'\0' + bytes([red, red, red, 255]) * width for _ in range(height))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'sRGB', b'\0') + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def main():
    compiler = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)/'配對素材 DEV ONLY'
        root.mkdir()
        (root/'base.png').write_bytes(png(4, 4, 193))
        (root/'mask.png').write_bytes(png(4, 4, 128))
        base = dict(type='atlas', id=[11, 12], color_space='sRGB', nearest=True, width=8,
                    images=[dict(path='base.png', grid=[2, 2], first_id=31, cells=[0, 2, 3])])
        (root/'base.json').write_text(json.dumps(base), encoding='utf-8')
        config = dict(schema=1, id=[21, 22], base_manifest='base.json', mask_id=[13, 14], mask_width=16,
                      images=[dict(source='base.png', mask='mask.png')])
        manifest, output = root/'emission.json', root/'out'/'catalog.json'
        manifest.write_text(json.dumps(config), encoding='utf-8')
        result = emission.prepare(manifest, compiler, output)
        first = output.read_bytes()
        mask = (output.parent/result['mask']).read_bytes()
        links = (output.parent/result['links']).read_bytes()
        assert struct.unpack_from('<I', mask, 36)[0] == 0  # linear sampling for data mask
        count = struct.unpack_from('<I', mask, 40)[0]
        assert count == 3 and [struct.unpack_from('<I', mask, 44 + i * 20)[0] for i in range(3)] == [31, 33, 34]
        pixels = mask[48 + count * 20:]
        assert 128 in pixels[::4] and 55 not in pixels[::4]  # sRGB-tagged mask remains raw data
        assert len(links) == 84 and struct.unpack_from('<6I', links, 60) == (31, 31, 33, 33, 34, 34)
        assert emission.prepare(manifest, compiler, output) == result and output.read_bytes() == first
        bad_configs = []
        for field, value in [('schema', True), ('mask_width', True), ('mask_id', [11, 12]),
                             ('images', []), ('base_manifest', '../outside.json')]:
            bad = copy.deepcopy(config); bad[field] = value; bad_configs.append(bad)
        for bad in bad_configs:
            manifest.write_text(json.dumps(bad), encoding='utf-8')
            try: emission.prepare(manifest, compiler, output)
            except (ValueError, OSError, subprocess.CalledProcessError): pass
            else: raise AssertionError('bad companion accepted')
            assert output.read_bytes() == first and (output.parent/result['mask']).read_bytes() == mask
        manifest.write_text(json.dumps(config), encoding='utf-8')
        (root/'mask.png').write_bytes(png(2, 2, 128))
        try: emission.prepare(manifest, compiler, output)
        except ValueError: pass
        else: raise AssertionError('mismatched source canvas accepted')
        assert output.read_bytes() == first
        # Invalid dependency metadata must not replace an existing native compiler output.
        invalid = bytearray(links); invalid[60:64] = struct.pack('<I', 999)
        (root/'invalid.nx2emission').write_bytes(invalid)
        emission.assets.compile_manifest(base, root, compiler, root/'base.nx2atlas')
        target = root/'preserved.nx2emission'; target.write_bytes(links)
        rejected = subprocess.run([str(compiler), 'emission', 'invalid.nx2emission', target.name,
                         'base.nx2atlas', str(output.parent/result['mask'])], cwd=root, capture_output=True)
        assert rejected.returncode != 0 and target.read_bytes() == links
        if len(sys.argv) == 4:
            app, module = Path(sys.argv[2]).resolve(), Path(sys.argv[3]).resolve()
            shutil.copyfile(module, root/module.name)
            descriptor = dict(schema=1, id='com.nexora.emission', title='DEV ONLY emission inventory',
                scene='Main', module=module.name, audio='off', assets=[
                    dict(kind='atlas', path='base.nx2atlas'),
                    dict(kind='atlas', path='out/'+result['mask'], preload=False),
                    dict(kind='blob', path='out/'+result['links'], preload=False)])
            (root/'game.json').write_text(json.dumps(descriptor), encoding='utf-8')
            package = root/'exported'
            exporter.export(root/'game.json', app, package, compiler=compiler, allow_development=True)
            inventory = json.loads((package/'inventory.json').read_text(encoding='utf-8'))
            for name, digest in inventory['files'].items():
                assert hashlib.sha256((package/name).read_bytes()).hexdigest() == digest
            files = [p.read_bytes() for p in (package/'assets').iterdir()]
            assert any(links == data for data in files) and any(mask in data for data in files)
            relocated = root/'relocated'
            shutil.copytree(package, relocated)
            assert (relocated/'inventory.json').read_bytes() == (package/'inventory.json').read_bytes()

    print('Paired emission source/crop/repack/linear data/atomic catalog PASS')


if __name__ == '__main__':
    main()
