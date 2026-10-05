"""Convert the pinned CC0 blockout assets into deterministic embedded authoring payloads.

This intentionally accepts only the untransformed, single-primitive GLBs in the inventory;
it is not a general glTF importer. Runtime still consumes AssetCooker/BundleBuilder blobs.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
CONTENT = ROOT / 'Content/Showcase/Courtyard/KayKit'
OUTPUT = ROOT / 'Apps/Showcase/CourtyardAssets.h'
ASSETS = {
    'pillar_decorated.gltf.glb': 'eecf7d454a1b6767a113e2490cb1536134abea0b57329a50cf2250d3ed8e2f65',
    'floor_tile_small_broken_A.gltf.glb': 'd1acc3407f941623b3b01ec9a1751c36a5ca82bb14721fc8f28e5559317edad4',
    'rubble_large.gltf.glb': '0292da1b5788a5a52b616d5d27d3270c676a5fbf9e921b6dd3dc30364d355b54',
    'dungeon_texture.png': 'f9ae182518f908bd09461a56a430b9ca7369812ac10890224cc6f32e7da3e1ee',
}


def mesh(data):
    if len(data) < 28 or len(data) > 4 * 1024 * 1024:
        raise ValueError('GLB size outside the adopted content contract')
    magic, version, length = struct.unpack_from('<III', data)
    if (magic, version, length) != (0x46546C67, 2, len(data)):
        raise ValueError('Invalid GLB header')
    size, kind = struct.unpack_from('<II', data, 12)
    if kind != 0x4E4F534A or size > len(data) - 28:
        raise ValueError('Missing GLB JSON chunk')
    document = json.loads(data[20:20 + size])
    size2, kind2 = struct.unpack_from('<II', data, 20 + size)
    binary = data[28 + size:]
    if kind2 != 0x004E4942 or size2 != len(binary):
        raise ValueError('Invalid GLB binary chunk')
    if document.get('buffers') != [{'byteLength': len(binary)}]:
        raise ValueError('External or mismatched buffers are unsupported')
    nodes = document.get('nodes', [])
    if len(nodes) != 1 or nodes[0].get('mesh') != 0 or any(
            key in nodes[0] for key in ('matrix', 'translation', 'rotation', 'scale', 'children')):
        raise ValueError('Only one untransformed mesh node is supported')
    if len(document['meshes']) != 1 or len(document['meshes'][0]['primitives']) != 1:
        raise ValueError('Only one mesh primitive is supported')
    primitive = document['meshes'][0]['primitives'][0]
    if primitive.get('mode', 4) != 4:
        raise ValueError('Only triangle geometry is supported')

    def accessor(index, kind, component):
        entry = document['accessors'][index]
        if entry['type'] != kind or entry['componentType'] != component or entry.get('sparse'):
            raise ValueError('Unsupported accessor representation')
        count = entry['count']
        if not isinstance(count, int) or count < 1 or count > 65535:
            raise ValueError('Accessor exceeds the scene upload budget')
        view = document['bufferViews'][entry['bufferView']]
        if view.get('buffer', 0) != 0:
            raise ValueError('Accessor uses an external buffer')
        fields = {'VEC3': 3, 'VEC2': 2, 'SCALAR': 1}[kind]
        code = 'f' if component == 5126 else 'H'
        fmt = '<' + code * fields
        width = struct.calcsize(fmt)
        stride = view.get('byteStride', width)
        offset = entry.get('byteOffset', 0)
        start = view.get('byteOffset', 0)
        limit = view['byteLength']
        if stride < width or offset < 0 or start < 0 or limit < 0 or start + limit > len(binary):
            raise ValueError('Invalid accessor byte layout')
        if offset + (count - 1) * stride + width > limit:
            raise ValueError('Accessor exceeds its buffer view')
        values = [struct.unpack_from(fmt, binary, start + offset + i * stride) for i in range(count)]
        if any(not math.isfinite(value) for row in values for value in row):
            raise ValueError('Nonfinite geometry')
        return values

    attributes = primitive['attributes']
    positions = accessor(attributes['POSITION'], 'VEC3', 5126)
    normals = accessor(attributes['NORMAL'], 'VEC3', 5126)
    uvs = accessor(attributes['TEXCOORD_0'], 'VEC2', 5126)
    indices = accessor(primitive['indices'], 'SCALAR', 5123)
    if len(positions) != len(normals) or len(positions) != len(uvs):
        raise ValueError('Mismatched vertex streams')
    if len(indices) % 3 or any(index[0] >= len(positions) for index in indices):
        raise ValueError('Invalid triangle indices')
    rows = [f'nexora.showcase.mesh.v1 {len(positions)} {len(indices)}']
    for position, normal, uv in zip(positions, normals, uvs):
        rows.append(' '.join(format(value, '.9g') for value in position + normal + uv))
    rows.append(' '.join(str(index[0]) for index in indices))
    return '\n'.join(rows) + '\n'


def atlas(data):
    if data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Invalid PNG signature')
    cursor, compressed = 8, bytearray()
    dimensions = None
    while cursor + 12 <= len(data):
        size = struct.unpack_from('>I', data, cursor)[0]
        kind = data[cursor + 4:cursor + 8]
        if size > len(data) - cursor - 12:
            raise ValueError('Truncated PNG chunk')
        payload = data[cursor + 8:cursor + 8 + size]
        checksum = struct.unpack_from('>I', data, cursor + 8 + size)[0]
        if zlib.crc32(kind + payload) != checksum:
            raise ValueError('PNG chunk checksum mismatch')
        if kind == b'IHDR':
            dimensions = struct.unpack('>IIBBBBB', payload)
        elif kind == b'IDAT':
            compressed.extend(payload)
        elif kind == b'IEND':
            break
        cursor += size + 12
    if dimensions != (1024, 1024, 8, 6, 0, 0, 0):
        raise ValueError('Only the pinned 1024x1024 RGBA8 atlas is supported')
    expected = 1024 * (1024 * 4 + 1)
    decoder = zlib.decompressobj()
    raw = decoder.decompress(compressed, expected + 1)
    if len(raw) != expected or not decoder.eof:
        raise ValueError('Invalid bounded atlas payload')
    pixels, previous = bytearray(), bytearray(4096)
    cursor = 0
    for _ in range(1024):
        mode = raw[cursor]
        row = bytearray(raw[cursor + 1:cursor + 4097])
        if mode > 4:
            raise ValueError('Unsupported PNG filter')
        for i in range(4096):
            left = row[i - 4] if i >= 4 else 0
            up = previous[i]
            corner = previous[i - 4] if i >= 4 else 0
            if mode == 1:
                row[i] = (row[i] + left) & 255
            elif mode == 2:
                row[i] = (row[i] + up) & 255
            elif mode == 3:
                row[i] = (row[i] + (left + up) // 2) & 255
            elif mode == 4:
                estimate = left + up - corner
                distances = [abs(estimate - value) for value in (left, up, corner)]
                predictor = (left, up, corner)[distances.index(min(distances))]
                row[i] = (row[i] + predictor) & 255
        pixels.extend(row)
        previous = row
        cursor += 4097
    # The source is a gradient atlas, not surface-detail maps. A bounded 64x64 area-filtered
    # baseline keeps its palette/UV identity; retain the unmodified original with the package.
    reduced = bytearray()
    for y in range(64):
        for x in range(64):
            for channel in range(4):
                total = sum(pixels[((y * 16 + dy) * 1024 + x * 16 + dx) * 4 + channel]
                            for dy in range(16) for dx in range(16))
                reduced.append((total + 128) // 256)
    return reduced


def generate():
    lines = ['// Generated by Tools/Build/PrepareCourtyardAssets.py; do not edit.',
             '#pragma once', '#include <array>', '#include <cstdint>', '#include <span>', '#include <string_view>',
             '// clang-format off', 'namespace nexora::showcase::courtyard_content {']
    mesh_index = 0
    for name, expected in ASSETS.items():
        data = (CONTENT / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError(f'Pinned source hash mismatch: {name}')
        if name.endswith('.glb'):
            rows = mesh(data).splitlines()
            lines.append(f'inline constexpr std::array<std::string_view, {len(rows)}> mesh{mesh_index}Rows{{')
            lines.extend(json.dumps(row + '\n') + ',' for row in rows)
            lines.append('};')
            mesh_index += 1
        else:
            pixels = atlas(data)
    lines += ['inline constexpr std::array<std::span<const std::string_view>, 3> meshes{mesh0Rows, mesh1Rows, mesh2Rows};',
              'inline constexpr std::array<std::uint8_t, 16384> atlas{']
    for start in range(0, len(pixels), 32):
        lines.append(','.join(str(value) for value in pixels[start:start + 32]) + ',')
    lines += ['};', '} // namespace nexora::showcase::courtyard_content', '// clang-format on', '']
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    content = generate()
    if args.check:
        if not OUTPUT.is_file() or OUTPUT.read_text() != content:
            raise SystemExit('CourtyardAssets.h differs from the pinned source conversion')
    else:
        OUTPUT.write_text(content)


if __name__ == '__main__':
    main()
