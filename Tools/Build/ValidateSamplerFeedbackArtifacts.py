"""Validate fixed feedback bindings/record ABI and pin real compiled recipe bytes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def reflection(path, entry, expected):
    data = json.loads(Path(path).read_text(encoding='utf-8'))
    if [item['name'] for item in data['entryPoints']] != [entry]:
        raise ValueError('unexpected feedback entry points')
    parameters = {item['name']: item for item in data['parameters']}
    if set(parameters) != set(expected):
        raise ValueError('unexpected feedback globals')
    for name, (kind, index) in expected.items():
        binding = parameters[name]['binding']
        if binding['kind'] != kind or binding['index'] != index or binding.get('space', 0) != 0:
            raise ValueError('feedback root binding mismatch')
    return parameters


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('write', 'pack', 'write-reflection', 'pack-reflection', 'source', 'pack-source',
                 'compiler', 'header', 'manifest'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    write = reflection(args.write_reflection, 'writeFeedback',
                       {'sampled': ('shaderResource', 0), 'pointClamp': ('samplerState', 0),
                        'feedback': ('unorderedAccess', 0), 'requests': ('shaderResource', 1),
                        'RequestCount': ('constantBuffer', 0)})
    packed = reflection(args.pack_reflection, 'packFeedback',
                        {'decoded': ('shaderResource', 0), 'packed': ('unorderedAccess', 1)})
    if (write['feedback']['type'].get('access') != 'feedback' or
        write['feedback']['type'].get('resultType', {}).get('name') != 'SAMPLER_FEEDBACK_MIN_MIP'):
        raise ValueError('opaque output is not a feedback texture')
    record = write['requests']['type']['resultType']
    fields = [(item['name'], item['binding']['offset'], item['binding']['size'])
              for item in record['fields']]
    if fields != [('uv', 0, 8), ('mip', 8, 4), ('enabled', 12, 4)]:
        raise ValueError('feedback request record ABI mismatch')
    if [item['value'] for item in record['sizes'] if item['kind'] == 'uniform'] != [16]:
        raise ValueError('feedback request stride mismatch')
    types = []
    for item in record['fields']:
        value = item['type']
        count = value.get('elementCount', 1)
        scalar = value['elementType'] if value['kind'] == 'vector' else value
        types.append((scalar.get('scalarType'), count))
    if types != [('float32', 2), ('uint32', 1), ('uint32', 1)]:
        raise ValueError('feedback request scalar/vector ABI mismatch')
    constants = write['RequestCount']['type']['elementType']
    if [(item['name'], item['binding']['offset'], item['binding']['size'])
        for item in constants['fields']] != [('count', 0, 4), ('reserved', 4, 12)]:
        raise ValueError('feedback constants ABI mismatch')
    if (packed['decoded']['type']['resultType'].get('scalarType') != 'uint32' or
        packed['packed']['type']['resultType'].get('scalarType') != 'uint32'):
        raise ValueError('decoded feedback must use integer loads')
    header = '#pragma once\nnamespace nexora::rhi::detail {\n'
    hashes = {}
    for name, path in [('FeedbackWriteBytes', args.write), ('FeedbackPackBytes', args.pack)]:
        code = Path(path).read_bytes()
        if not code or len(code) > 16 * 1024 * 1024:
            raise ValueError('invalid feedback shader size')
        hashes[name] = hashlib.sha256(code).hexdigest()
        header += f'inline constexpr unsigned char {name}[] = {{\n'
        for offset in range(0, len(code), 24):
            header += ','.join(str(value) for value in code[offset:offset + 24]) + ',\n'
        header += '};\n'
    header += '}\n'
    version = subprocess.run([args.compiler, '-v'], capture_output=True, text=True,
                             timeout=10, check=True)
    manifest = {'schema_version': 1, 'compiler_version': (version.stdout + version.stderr).strip(),
                'source_sha256': [hashlib.sha256(Path(path).read_bytes()).hexdigest()
                                  for path in (args.source, args.pack_source)],
                'artifact_sha256': hashes, 'requested_profiles': ['sm_6_5', 'sm_6_0'],
                'fp_mode': 'precise', 'request_bytes': 16, 'paired_extent': 64,
                'paired_mips': 7, 'region_extent': 16, 'decoded_extent': 4,
                'native_execution_verified': False}
    for path, content in [(args.header, header), (args.manifest, json.dumps(manifest, indent=2) + '\n')]:
        output = Path(path)
        if not output.exists() or output.read_text(encoding='utf-8') != content:
            output.write_text(content, encoding='utf-8')
    print('Validated real feedback reflection and exact recipe bytes; native execution pending')


if __name__ == '__main__':
    main()
