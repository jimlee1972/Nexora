"""Validate the fixed ray record/root ABI and pin the exact compiled recipe bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def validate(path, entries):
    reflection = json.loads(Path(path).read_text(encoding='utf-8'))
    if {entry['name'] for entry in reflection['entryPoints']} != set(entries):
        raise ValueError('unexpected ray entry points')
    parameters = {parameter['name']: parameter for parameter in reflection['parameters']}
    if set(parameters) != {'scene', 'rays', 'hits'}:
        raise ValueError('unexpected ray global parameters')
    for name, kind, index in [('scene', 'shaderResource', 0), ('rays', 'shaderResource', 1),
                              ('hits', 'unorderedAccess', 1)]:
        binding = parameters[name]['binding']
        if binding['kind'] != kind or binding['index'] != index or binding.get('space', 0) != 0:
            raise ValueError('ray root binding mismatch')
    if parameters['scene']['type']['baseShape'] != 'accelerationStructure':
        raise ValueError('ray scene is not an acceleration structure')
    for name, size, fields in [('rays', 32, [('origin', 0, 12), ('minimum', 12, 4),
                                             ('direction', 16, 12), ('maximum', 28, 4)]),
                               ('hits', 16, [('primitive', 0, 4), ('distance', 4, 4),
                                             ('barycentrics', 8, 8)])]:
        if parameters[name]['type']['baseShape'] != 'structuredBuffer':
            raise ValueError('ray records require structured buffers')
        value = parameters[name]['type']['resultType']
        sizes = [item['value'] for item in value['sizes'] if item['kind'] == 'uniform']
        actual = [(item['name'], item['binding']['offset'], item['binding']['size'])
                  for item in value['fields']]
        if sizes != [size] or actual != fields:
            raise ValueError('ray record ABI mismatch')
        scalar_types = []
        for item in value['fields']:
            scalar = item['type']
            count = scalar.get('elementCount', 1)
            if scalar['kind'] == 'vector':
                scalar = scalar['elementType']
            scalar_types.append((scalar.get('scalarType'), count))
        expected_types = ([('float32', 3), ('float32', 1), ('float32', 3), ('float32', 1)]
                          if name == 'rays' else [('uint32', 1), ('float32', 1), ('float32', 2)])
        if scalar_types != expected_types:
            raise ValueError('ray record scalar/vector types mismatch')


def write_changed(path, content):
    output = Path(path)
    if not output.exists() or output.read_text(encoding='utf-8') != content:
        output.write_text(content, encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('library', 'query', 'library-reflection', 'query-reflection', 'source',
                 'compiler', 'header', 'manifest'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    validate(args.library_reflection, ['rayGeneration', 'rayMiss', 'rayClosest'])
    validate(args.query_reflection, ['inlineQuery'])
    header = '#pragma once\nnamespace nexora::rhi::detail {\n'
    hashes = {}
    for name, path in [('RayLibraryBytes', args.library), ('RayQueryBytes', args.query)]:
        code = Path(path).read_bytes()
        if not code or len(code) > 16 * 1024 * 1024:
            raise ValueError('invalid bounded ray artifact size')
        hashes[name] = hashlib.sha256(code).hexdigest()
        header += f'inline constexpr unsigned char {name}[] = {{\n'
        for offset in range(0, len(code), 24):
            header += ','.join(str(value) for value in code[offset:offset + 24]) + ',\n'
        header += '};\n'
    header += '}\n'
    version = subprocess.run([args.compiler, '-v'], capture_output=True, text=True,
                             timeout=10, check=True)
    manifest = {'schema_version': 1, 'compiler_version': (version.stdout + version.stderr).strip(),
                'source_sha256': hashlib.sha256(Path(args.source).read_bytes()).hexdigest(),
                'artifact_sha256': hashes, 'requested_profiles': ['lib_6_3', 'sm_6_5'], 'fp_mode': 'precise',
                'input_bytes': 32, 'hit_bytes': 16, 'root': ['t0', 't1', 'u1'],
                'native_execution_verified': False}
    manifest['program_key'] = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
    write_changed(args.header, header)
    write_changed(args.manifest, json.dumps(manifest, indent=2) + '\n')
    print('Validated actual ray reflection and pinned exact recipe bytes; native execution pending')


if __name__ == '__main__':
    main()
