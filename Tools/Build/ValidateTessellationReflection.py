"""Validate actual Slang entry-stage/layout output and identify the bounded programs."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from NormalizeShaderReflection import _find_uniform_size


def require(value, message):
    if not value:
        raise ValueError(message)


def entries(path, expected):
    data = json.loads(Path(path).read_text(encoding='utf-8'))
    actual = {entry['name']: entry['stage'] for entry in data['entryPoints']}
    require(len(actual) == len(data['entryPoints']) and actual == expected,
            f'{path}: shader entry/stage mismatch: {actual}')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'include', 'native', 'fallback-spirv', 'fallback-metal', 'output'):
        parser.add_argument('--' + name, required=True)
    for name in ('control-dxil', 'evaluation-dxil', 'fallback-dxil'):
        parser.add_argument('--' + name)
    parser.add_argument('--artifact', action='append', required=True)
    args = parser.parse_args()
    entries(args.native, {'tessControlMain': 'hull', 'tessEvaluationMain': 'domain'})
    spirv = entries(args.fallback_spirv, {'preTessellatedVertexMain': 'vertex'})
    metal = entries(args.fallback_metal,
                    {'preTessellatedVertexMain': 'vertex', 'fragmentMain': 'fragment'})
    require(_find_uniform_size(spirv, 'SPIR-V fallback') == 64 and
            _find_uniform_size(metal, 'Metal fallback') == 64, 'fallback frame layout must be64 bytes')
    dxil = [args.control_dxil, args.evaluation_dxil, args.fallback_dxil]
    require(all(dxil) or not any(dxil), 'DXIL stage reflections must be provided together')
    if all(dxil):
        entries(dxil[0], {'tessControlMain': 'hull'})
        entries(dxil[1], {'tessEvaluationMain': 'domain'})
        fallback = entries(dxil[2], {'preTessellatedVertexMain': 'vertex'})
        require(_find_uniform_size(fallback, 'DXIL fallback') == 64, 'DXIL frame layout')
    artifacts = []
    for path in args.artifact:
        blob = Path(path).read_bytes()
        require(bool(blob), f'{path}: empty shader artifact')
        if Path(path).name == 'PreTessellated.spv':
            require(len(blob) % 4 == 0 and len(blob) >= 20, 'fallback SPIR-V envelope')
            words = struct.unpack('<' + 'I' * (len(blob) // 4), blob)
            index = 5
            while index < len(words):
                count, opcode = words[index] >> 16, words[index] & 65535
                require(count > 0 and count <= len(words) - index, 'fallback SPIR-V instruction')
                require(not (opcode == 17 and count >= 2 and words[index + 1] == 3),
                        'baked fallback must not require the Tessellation capability')
                index += count
        artifacts.append({'name': Path(path).name, 'sha256': hashlib.sha256(blob).hexdigest()})
    source_hash = hashlib.sha256(Path(args.source).read_bytes() + b'\0' +
                                 Path(args.include).read_bytes()).hexdigest()
    manifest = {'schema_version': 1, 'source': 'Shaders/Tessellation.slang',
                'included_source': 'Shaders/Triangle.slang', 'source_sha256': source_hash,
                'native': {'topology': 'patch_list', 'control_points': 3, 'factor': 4,
                           'stages': ['vertex', 'hull', 'domain', 'fragment']},
                'fallback': {'topology': 'triangle_list', 'vertex_count': 48,
                             'stages': ['vertex', 'fragment'], 'frame_bytes': 64},
                'new_stage_dxil_profile': 'sm_6_0' if all(dxil) else None,
                'canonical_graphics_dxil_profile': 'sm_6_6' if all(dxil) else None, 'artifacts': artifacts}
    manifest['program_key'] = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
    Path(args.output).write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    targets = 'SPIR-V/Metal' + ('/DXIL' if all(dxil) else '')
    print(f'Validated real hull/domain stages and64-byte {targets} fallback reflection')


if __name__ == '__main__':
    main()
