#!/usr/bin/env python3
"""Slice bounded RGBA8 PNGs and compile canonical Nexora 2D assets without third-party packages."""
import argparse
import json
import os
import shutil
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib

MAX = 64 * 1024 * 1024

def bounded(path):
    if path.stat().st_size > MAX:
        raise ValueError('input exceeds 64 MiB')
    return path.read_bytes()

def png(data, color_space="linear"):
    if color_space not in ("linear", "sRGB"):
        raise ValueError("color_space must be linear or sRGB")
    profiles = set()
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('invalid PNG signature')
    cursor, compressed, dimensions, ended = 8, bytearray(), None, False
    while cursor + 12 <= len(data):
        size = struct.unpack_from('>I', data, cursor)[0]
        kind = data[cursor+4:cursor+8]
        if size > len(data) - cursor - 12:
            raise ValueError('truncated PNG chunk')
        payload = data[cursor+8:cursor+8+size]
        if zlib.crc32(kind + payload) != struct.unpack_from('>I', data, cursor+8+size)[0]:
            raise ValueError('PNG CRC mismatch')
        if dimensions is None and kind != b'IHDR':
            raise ValueError('IHDR must be first')
        if kind == b'IHDR':
            if dimensions is not None or size != 13:
                raise ValueError('invalid IHDR')
            dimensions = struct.unpack('>IIBBBBB', payload)
            w, h = dimensions[:2]
            if not (1 <= w <= 4096 and 1 <= h <= 4096 and w*h <= 4*1024*1024) or dimensions[2:] != (8, 6, 0, 0, 0):
                raise ValueError('requires bounded noninterlaced RGBA8 PNG')
        elif kind in (b'sRGB', b'gAMA', b'iCCP', b'cHRM'):
            if color_space == 'sRGB':
                if kind in profiles:
                    raise ValueError('duplicate PNG color metadata')
                profiles.add(kind)
                if kind == b'iCCP':
                    raise ValueError('ICC profiles require external normalization to sRGB')
                if kind == b'sRGB' and (size != 1 or payload[0] > 3):
                    raise ValueError('invalid sRGB intent')
                if kind == b'gAMA' and payload != struct.pack('>I', 45455):
                    raise ValueError('PNG gamma conflicts with sRGB')
                if kind == b'cHRM' and payload != struct.pack('>8I', 31270,32900,64000,33000,30000,60000,15000,6000):
                    raise ValueError('PNG chromaticity conflicts with sRGB')
        elif kind == b'IDAT':
            compressed.extend(payload)
        elif kind == b'IEND':
            if size:
                raise ValueError('invalid IEND')
            ended = True
            cursor += 12
            break
        elif kind[:1].isupper():
            raise ValueError('unsupported critical PNG chunk')
        cursor += size + 12
    if not ended or cursor != len(data) or dimensions is None:
        raise ValueError('missing end or trailing PNG bytes')
    stride, expected = w*4, h*(w*4+1)
    decoder = zlib.decompressobj()
    raw = decoder.decompress(compressed, expected+1)
    if len(raw) != expected or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError('invalid bounded PNG payload')
    pixels, previous = bytearray(), bytearray(stride)
    for y in range(h):
        mode = raw[y*(stride+1)]
        row = bytearray(raw[y*(stride+1)+1:(y+1)*(stride+1)])
        if mode > 4:
            raise ValueError('invalid PNG filter')
        for i in range(stride):
            left, up, corner = (row[i-4] if i >= 4 else 0), previous[i], (previous[i-4] if i >= 4 else 0)
            if mode == 1: predictor = left
            elif mode == 2: predictor = up
            elif mode == 3: predictor = (left+up)//2
            elif mode == 4:
                p = left+up-corner
                predictor = min((left, up, corner), key=lambda n: abs(p-n))
            else: predictor = 0
            row[i] = (row[i]+predictor) & 255
        pixels.extend(row)
        previous = row
    if color_space == 'sRGB':
        pixels = srgb_to_linear_rgba8(pixels)
    return w, h, pixels

def srgb_to_linear_rgba8(pixels):
    """IEC 61966-2-1 RGB decode, round to nearest linear UNORM8; never transform alpha."""
    if len(pixels) % 4:
        raise ValueError('RGBA length must be divisible by four')
    table = bytes(int(255 * (c/255/12.92 if c/255 <= 0.04045 else ((c/255+0.055)/1.055)**2.4) + 0.5)
                  for c in range(256))
    out = bytearray(pixels)
    for offset in range(0, len(out), 4):
        for channel in range(3):
            out[offset+channel] = table[out[offset+channel]]
    return out

def integer(v, minimum, maximum):
    if type(v) is not int or not minimum <= v <= maximum:
        raise ValueError('integer outside manifest budget')
    return v

def uuid(value):
    if not isinstance(value, list) or len(value) != 2:
        raise ValueError('UUID requires [high, low]')
    high, low = (integer(v, 0, 2**64-1) for v in value)
    if not (high or low): raise ValueError('zero UUID')
    return struct.pack('<QQ', high, low)

def source_path(root, value):
    if not isinstance(value, str): raise ValueError('image path must be text')
    path = (root/value).resolve()
    if not path.is_relative_to(root): raise ValueError('image path leaves manifest directory')
    return path

def images(manifest, root):
    output = []
    color_space = manifest.get('color_space', 'linear')
    if color_space not in ('linear', 'sRGB'):
        raise ValueError('color_space must be linear or sRGB')
    if manifest.get('alpha_mode', 'straight') != 'straight':
        raise ValueError('only straight alpha is supported')
    if not isinstance(manifest['images'], list) or not 1 <= len(manifest['images']) <= 1024:
        raise ValueError('image count budget')
    for source in manifest['images']:
        width, height, rgba = png(bounded(source_path(root, source['path'])), color_space)
        regions = source.get('regions')
        if 'grid' in source:
            if regions is not None: raise ValueError('choose rectangles or grid')
            cellw, cellh = (integer(v, 1, 4096) for v in source['grid'])
            if width % cellw or height % cellh: raise ValueError('grid must divide image')
            if (width//cellw)*(height//cellh) > 1024: raise ValueError('grid region budget')
            first = integer(source.get('first_id', 1), 1, 2**32-1)
            regions = [{'id':first+i, 'rect':[x, y, cellw, cellh]}
                       for i, (x, y) in enumerate(( (x, y) for y in range(0, height, cellh) for x in range(0, width, cellw)))]
        if not regions or len(regions) > 1024: raise ValueError('region budget')
        for region in regions:
            rid = integer(region['id'], 1, 2**32-1)
            x, y, w, h = (integer(v, 0, 4096) for v in region['rect'])
            if not w or not h or x+w > width or y+h > height: raise ValueError('region outside image')
            pixels = b''.join(rgba[((y+row)*width+x)*4:((y+row)*width+x+w)*4] for row in range(h))
            output.append(struct.pack('<IIII', rid, w, h, len(pixels))+pixels)
            if len(output) > 1024 or sum(map(len, output)) > MAX-64: raise ValueError('source budget')
    return output

def compile_manifest(manifest, root, compiler, output):
    root = root.resolve(strict=True)
    kind = manifest['type']
    if kind == 'atlas':
        parts = images(manifest, root)
        nearest = manifest.get('nearest', False)
        if type(nearest) is not bool: raise ValueError('nearest must be bool')
        data = struct.pack('<II', 0x4932444e, 1)+uuid(manifest['id'])+struct.pack('<IIII', integer(manifest.get('width', 1024), 3, 4096), integer(manifest.get('maximum_height', 4096), 3, 4096), int(nearest), len(parts))+b''.join(parts)
        command = 'pack'
    elif kind == 'clip':
        frames = manifest['frames']
        if not 1 <= len(frames) <= 4096: raise ValueError('frame budget')
        loop = manifest.get('loop', True)
        if type(loop) is not bool: raise ValueError('loop must be bool')
        data = struct.pack('<II', 0x4332444e, 1)+uuid(manifest['id'])+uuid(manifest['atlas'])+struct.pack('<II', int(loop), len(frames))
        for f in frames:
            event = f.get('event', '').encode('ascii')
            if len(event) > 256: raise ValueError('event budget')
            data += struct.pack('<IdI', integer(f['region'], 1, 2**32-1), float(f['seconds']), len(event))+event
        command = 'clip'
    else: raise ValueError('unknown manifest type')
    with tempfile.TemporaryDirectory(prefix='nexora-2d-') as temp:
        packet = Path(temp)/'source.bin'
        packet.write_bytes(data)
        # The legacy native compiler accepts narrow argv on Windows. Keep its IO
        # arguments ASCII-relative; Python preserves Unicode project/output paths.
        compiled = Path(temp)/'compiled.asset'
        subprocess.run([str(compiler.resolve()), command, packet.name, compiled.name],
                       cwd=temp, check=True)
        # Publish on the output filesystem; compiler/copy/replace failures retain
        # the previous output and temporary files never become the published asset.
        with tempfile.TemporaryDirectory(prefix='.nx2-publish-', dir=output.parent) as publication:
            staged = Path(publication)/'asset'
            shutil.copyfile(compiled, staged)
            os.replace(staged, output)

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('manifest', type=Path)
    p.add_argument('--compiler', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    manifest = json.loads(bounded(args.manifest))
    compile_manifest(manifest, args.manifest.resolve().parent, args.compiler.resolve(), args.output.resolve())

if __name__ == '__main__':
    main()
