"""Compile and validate the fixed SM6.9 SER ray library and a same-profile negative."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    base = [args.compiler, '-T', 'lib_6_9', '-HV', '2021', '-WX', '-Gis',
            '-exports', 'rayGeneration;rayMiss;rayClosest', str(args.source)]
    artifact = args.output / 'ReorderedRays.dxil'
    subprocess.run(base + ['-Fo', str(artifact)], check=True)
    data = artifact.read_bytes()
    dump = subprocess.check_output([args.compiler, '-dumpbin', str(artifact)], text=True)
    for name, fields, size in [('rays', [('float3', 'origin', 0), ('float', 'minimum', 12),
            ('float3', 'direction', 16), ('float', 'maximum', 28)], 32),
            ('hits', [('uint', 'primitive', 0), ('float', 'distance', 4), ('float2', 'barycentrics', 8)], 16)]:
        section = dump.split('Resource bind info for ' + name, 1)[1].split('; }', 1)[0]
        for kind, field, offset in fields:
            if not re.search(rf'\b{kind}\s+{field};\s*; Offset:\s*{offset}\b', section):
                raise ValueError(f'SER reflection mismatch: {field}')
        if not re.search(rf'\$Element;\s*; Offset:\s*0 Size:\s*{size}\b', section):
            raise ValueError(f'SER reflection stride mismatch: {name}')
    for name, register in [('scene', 't0'), ('rays', 't1'), ('hits', 'u1')]:
        if not re.search(rf'^; {name}\s+.*\b{register}\s+1$', dump, re.MULTILINE):
            raise ValueError(f'SER reflection binding mismatch: {name}')
    for operation in ['hitObject_TraceRay', 'maybeReorderThread', 'hitObject_Invoke']:
        if operation.lower() not in dump.lower():
            raise ValueError(f'Missing actual SER instruction: {operation}')
    negative = args.output / 'ReorderedRaysWrongIdentity.dxil'
    subprocess.run(base + ['-D', 'NEXORA_RAY_IDENTITY_NEGATIVE=1', '-Fo', str(negative)], check=True)
    if negative.read_bytes() == data:
        raise ValueError('Same-profile negative failed to change bytecode')
    header = '#pragma once\nnamespace nexora::rhi::detail {\ninline constexpr unsigned char SerRayLibraryBytes[]{'
    header += ','.join(str(byte) for byte in data) + '};\n}\n'
    (args.output / 'SerArtifactIdentity.h').write_text(header, encoding='utf-8')
    manifest = {'compiler': subprocess.check_output([args.compiler, '--version'], text=True).strip(),
        'source_sha256': hashlib.sha256(args.source.read_bytes()).hexdigest(),
        'artifact_sha256': hashlib.sha256(data).hexdigest(), 'profile': 'lib_6_9',
        'payload_bytes': 16, 'ray_stride': 32, 'native_execution': False,
        'actual_thread_reordering': 'requires separate OPTIONS22 query; WARP does not reorder'}
    (args.output / 'ReorderedRays.manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
