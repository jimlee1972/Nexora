"""Validate actual bounded mesh entry/input reflection and identify its compiled program."""
import argparse
import hashlib
import json
from pathlib import Path
from NormalizeShaderReflection import _find_uniform_size


def require(value, message):
    if not value:
        raise ValueError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'include', 'reflection', 'artifact', 'output'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    reflection = json.loads(Path(args.reflection).read_text(encoding='utf-8'))
    entries = reflection['entryPoints']
    require(len(entries) == 1 and entries[0]['name'] == 'meshMain' and
            entries[0]['stage'] == 'mesh', 'expected exactly meshMain at mesh stage')
    parameters = {p['name']: p for p in reflection['parameters']}
    for name, kind in (('frame', 'constantBuffer'), ('meshStatistics', 'shaderResource')):
        binding = parameters[name]['binding']
        require(binding['kind'] == kind and binding['index'] == 0 and
                binding.get('space', 0) == 0, f'{name}: unexpected register/space')
    resource = parameters['meshStatistics']['type']
    require(resource['baseShape'] == 'structuredBuffer' and
            resource['resultType']['scalarType'] == 'uint32', 'mesh statistics must be uint words')
    require(_find_uniform_size(reflection, 'Mesh DXIL') == 64, 'mesh frame must be64 bytes')
    blob = Path(args.artifact).read_bytes()
    require(bool(blob), 'empty mesh artifact')
    manifest = {'schema_version': 1, 'stage': 'mesh', 'entry': 'meshMain', 'profile': 'sm_6_5',
                'source_sha256': hashlib.sha256(Path(args.source).read_bytes() + b'\0' +
                                                Path(args.include).read_bytes()).hexdigest(),
                'artifact_sha256': hashlib.sha256(blob).hexdigest(),
                'frame_bytes': 64, 'statistics_slot': 0, 'visible_count_word': 4,
                'dispatch_groups': [1, 1, 1], 'maximum_vertices': 3, 'maximum_primitives': 1}
    manifest['program_key'] = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
    Path(args.output).write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print('Validated real meshMain stage,64-byte frame and uint statistics input reflection')


if __name__ == '__main__':
    main()
