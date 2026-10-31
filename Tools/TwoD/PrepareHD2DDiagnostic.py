#!/usr/bin/env python3
"""Reproduce explicit DEV ONLY HD2D fixtures with the real paired-mask asset tools."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
import zlib
import PrepareAssets as assets
import PrepareEmission as emission

ROOT = Path(__file__).resolve().parents[2]
BASE = [0x48443244, 1]
MASK = [0x48443244, 2]
LINKS = [0x48443244, 3]


def png(path, pixels):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    raw = b''.join(b'\0' + pixels[y * 64 * 4:(y + 1) * 64 * 4] for y in range(16))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 64, 16, 8, 6, 0, 0, 0))
                     + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def generate(compiler, output, check=False):
    with tempfile.TemporaryDirectory(prefix='nexora-hd2d-fixtures-') as directory:
        stage = Path(directory)
        base = bytearray(64 * 16 * 4)
        mask = bytearray(64 * 16 * 4)
        for offset in range(3, len(mask), 4): mask[offset] = 255
        def pixel(data, x, y, rgba): data[(y * 64 + x) * 4:(y * 64 + x + 1) * 4] = bytes(rgba)
        for y in range(8):
            for x in range(8):
                v = 42 if (x // 2 + y // 2) % 2 else 52
                pixel(base, x, y, (v, v, v, 255))
        pixel(base, 8, 0, (255, 255, 255, 255))
        for y in range(12):
            for x in range(8):
                border = x in (0, 7) or y in (0, 11)
                pixel(base, 16 + x, y, (92, 32, 8, 255) if border else (190, 140, 70, 255))
                if 2 <= x <= 5 and 3 <= y <= 8: pixel(mask, 16 + x, y, (255, 255, 255, 255))
        for y in range(12):
            for x in range(12): pixel(base, 24 + x, y, (55, 70, 90, 255))
        for y in range(8):
            for x in range(8):
                pixel(base, 40 + x, y, (160, 160, 160, 255))
                if x in (3, 4) or y in (3, 4): pixel(mask, 40 + x, y, (255, 255, 255, 255))
                if x in (0, 7) or y in (0, 7): pixel(base, 48 + x, y, (255, 12, 8, 255))
        png(stage/'base.png', base); png(stage/'mask.png', mask)
        regions = [dict(id=i + 1, rect=r) for i, r in enumerate([
            [0, 0, 8, 8], [8, 0, 1, 1], [16, 0, 8, 12], [24, 0, 12, 12], [40, 0, 8, 8], [48, 0, 8, 8]])]
        manifest = dict(type='atlas', id=BASE, width=64, nearest=True, color_space='linear',
                        images=[dict(path='base.png', regions=regions)])
        (stage/'base.json').write_text(json.dumps(manifest), encoding='utf-8')
        companion = dict(schema=1, id=LINKS, base_manifest='base.json', mask_id=MASK, mask_width=32,
                         images=[dict(source='base.png', mask='mask.png')])
        (stage/'emission.json').write_text(json.dumps(companion), encoding='utf-8')
        assets.compile_manifest(manifest, stage, compiler, stage/'base.nx2atlas')
        result = emission.prepare(stage/'emission.json', compiler, stage/'catalog.json')
        products = {'base.nx2atlas': stage/'base.nx2atlas', 'mask.nx2atlas': stage/result['mask'],
                    'links.nx2emission': stage/result['links']}
        # Reuse the licensed diagnostic raster corpus; copy its attribution/license with it.
        for name in ('ui_fixture.nxfont', 'NOTICE.md', 'OFL.txt'):
            products[name] = ROOT/'Gameplay/Zig/src/fixtures'/name
        inventory = {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in products.items()}
        for name, path in products.items():
            target = output/name
            if check:
                if not target.is_file() or target.read_bytes() != path.read_bytes():
                    raise ValueError(f'diagnostic fixture differs: {name}')
            else:
                output.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(path, target)
        return inventory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT/'Bindings/Zig/diagnostic')
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    print(json.dumps(generate(args.compiler.resolve(), args.output, args.check), sort_keys=True))


if __name__ == '__main__':
    main()
