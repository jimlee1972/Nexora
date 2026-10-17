"""Identify a real DXC library; native entry/node ABI validation is performed by RHI."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'artifact', 'compiler', 'output'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    artifact = Path(args.artifact).read_bytes()
    if not artifact:
        raise ValueError('empty Work Graph library')
    version = subprocess.run([args.compiler, '--version'], capture_output=True, text=True,
                             timeout=10, check=False)
    compiler_version = (version.stdout + version.stderr).strip() if version.returncode == 0 else None
    manifest = {'schema_version': 1, 'profile_requested': 'lib_6_8',
                'compiler': Path(args.compiler).name, 'compiler_version': compiler_version,
                'source_sha256': hashlib.sha256(Path(args.source).read_bytes()).hexdigest(),
                'artifact_sha256': hashlib.sha256(artifact).hexdigest(),
                'program': 'NexoraVisibleExpansion', 'entry': 'producer', 'child': 'consumer',
                'input_bytes': 8, 'maximum_records': 8, 'visible_record_words': 5,
                'native_execution_verified': False}
    manifest['program_key'] = hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()
    Path(args.output).write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Compiled optional Work Graph library; DXC version={compiler_version}; native validation pending')


if __name__ == '__main__':
    main()
