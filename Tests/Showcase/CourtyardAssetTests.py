"""Pinned conversion acceptance and rejection of unsupported/corrupt asset payloads."""
import importlib.util
import json
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('courtyard', ROOT / 'Tools/Build/PrepareCourtyardAssets.py')
converter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(converter)


def reject(operation):
    try:
        operation()
    except (ValueError, KeyError, IndexError, struct.error):
        return
    raise AssertionError('Unsupported or corrupt content was accepted')


def change_document(original, change):
    size = struct.unpack_from('<I', original, 12)[0]
    document = json.loads(original[20:20 + size])
    change(document)
    text = json.dumps(document).encode()
    text += b' ' * ((-len(text)) % 4)
    binary_chunk = original[20 + size:]
    return struct.pack('<III', 0x46546C67, 2, 20 + len(text) + len(binary_chunk)) + \
        struct.pack('<II', len(text), 0x4E4F534A) + text + binary_chunk


source = (converter.CONTENT / 'pillar_decorated.gltf.glb').read_bytes()
converted = converter.mesh(source)
assert converted.splitlines()[0] == 'nexora.showcase.mesh.v1 1276 2283'
assert len(converted.splitlines()) == 1278
reject(lambda: converter.mesh(source[:-1]))
reject(lambda: converter.mesh(change_document(source, lambda j: j['nodes'][0].update(scale=[2, 1, 1]))))
reject(lambda: converter.mesh(change_document(source, lambda j: j['buffers'][0].update(uri='external.bin'))))
reject(lambda: converter.mesh(change_document(source, lambda j: j['accessors'][0].update(count=65536))))
reject(lambda: converter.mesh(change_document(source, lambda j: j['bufferViews'][0].update(byteLength=1))))
reject(lambda: converter.mesh(change_document(source, lambda j: j['meshes'][0]['primitives'][0].update(mode=1))))
png = (converter.CONTENT / 'dungeon_texture.png').read_bytes()
reject(lambda: converter.atlas(png[:100]))
corrupt = bytearray(png)
corrupt[50] ^= 1
reject(lambda: converter.atlas(corrupt))
assert len(converter.atlas(png)) == 64 * 64 * 4
print('PASS: adopted GLB/atlas geometry and bounded conversion rejection')
