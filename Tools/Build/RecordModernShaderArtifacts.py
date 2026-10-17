"""Compile and pin the opt-in fixed modern compute recipes; retain compiler/source provenance."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--source-root', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--hlsl-include', required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    manifest = {'native_execution': False, 'compiler': subprocess.check_output(
        [args.compiler, '--version'], text=True).strip(), 'recipes': []}
    arrays = []
    for index, (name, profile) in enumerate([
            ('LongVector', 'cs_6_9'), ('LinearAlgebra', 'cs_6_10'), ('GroupShared', 'cs_6_10')]):
        source = args.source_root / ('Modern' + name + '.hlsl')
        target = args.output / ('Modern' + name + '.dxil')
        command = [args.compiler, '-T', profile, '-E', 'computeMain', '-HV', '2021',
                   '-WX', '-I', args.hlsl_include, '-Fo', str(target), str(source)]
        subprocess.run(command, check=True)
        data = target.read_bytes()
        if len(data) < 32 or data[:4] != b'DXBC':
            raise ValueError('Compiler did not produce a DXIL container')
        if name == 'GroupShared':
            negative = subprocess.run(command + ['-D', 'GROUP_WORDS=16385'],
                                      capture_output=True, text=True)
            if negative.returncode == 0 or 'exceeding explicit limit' not in negative.stderr:
                raise ValueError('GroupSharedLimit negative compilation did not reject the oversized allocation')
            # A failed compiler may remove its output. Rebuild the positive artifact deterministically.
            subprocess.run(command, check=True)
            if target.read_bytes() != data:
                raise ValueError('Positive compilation is not deterministic')
        manifest['recipes'].append({'name': name, 'profile': profile,
            'source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
            'artifact_sha256': hashlib.sha256(data).hexdigest()})
        arrays.append('inline constexpr unsigned char ModernShader' + str(index) + '[]{' +
                      ','.join(str(byte) for byte in data) + '};\n')
    header = '#pragma once\n#include <span>\n#include <cstddef>\nnamespace nexora::rhi::detail {\n'
    header += ''.join(arrays)
    header += 'inline std::span<const unsigned char> ModernShaderBytes(std::size_t kind) {\n switch(kind) {\n'
    for index in range(3):
        header += f'case {index}: return ModernShader{index};\n'
    header += 'default: return {};\n}}\n}\n'
    (args.output / 'ModernShaderArtifactIdentity.h').write_text(header, encoding='utf-8')
    (args.output / 'ModernShaders.manifest.json').write_text(
        json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
