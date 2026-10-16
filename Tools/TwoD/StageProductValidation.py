#!/usr/bin/env python3
"""Stage the existing Zig 2D host with cooked service-acceptance fixtures, without Editor."""
import argparse
import hashlib
import json
import math
import re
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import wave


def stage(runtime: Path, compiler: Path, output: Path, allow_development=False):
    runtime, compiler, output = runtime.resolve(), compiler.resolve(), output.resolve()
    if not (sys.platform.startswith('linux') or sys.platform == 'win32'):
        raise ValueError('this staging tool supports Linux and Windows validation hosts')
    if output.exists() and any(output.iterdir()):
        raise ValueError('use a fresh empty package directory')
    output.mkdir(parents=True, exist_ok=True)
    target = output / runtime.name
    shutil.copy2(runtime, target)
    files = [target]
    if sys.platform.startswith('linux'):
        dependencies = subprocess.check_output(['ldd', str(runtime)], text=True)
        if 'not found' in dependencies or 'NexoraEditor' in dependencies:
            raise ValueError('unresolved or Editor dependencies')
        for line in dependencies.splitlines():
            if 'Nexora' not in line:
                continue
            if not allow_development:
                raise ValueError('requires Monolithic Shipping; development copy is test-only')
            library = Path(line.split('=>', 1)[1].strip().split()[0])
            shutil.copy2(library, output / library.name)
            files.append(output / library.name)
    if sys.platform == 'win32':
        dumpbin = shutil.which('dumpbin')
        if not dumpbin:
            raise ValueError('Windows staging requires dumpbin in the developer shell')
        pending, seen = [runtime], set()
        while pending:
            binary = pending.pop()
            dependencies = subprocess.check_output([dumpbin, '/DEPENDENTS', str(binary)], text=True)
            for name in re.findall(r'^\s*(\S+\.dll)\s*$', dependencies, re.MULTILINE | re.IGNORECASE):
                if not name.lower().startswith(('nexora', 'libnexora')):
                    continue
                if 'editor' in name.lower() or not allow_development:
                    raise ValueError('requires an Editor-free Monolithic Shipping runtime')
                if name.lower() not in seen:
                    seen.add(name.lower())
                    library = runtime.parent / name
                    shutil.copy2(library, output / name)
                    files.append(output / name)
                    pending.append(library)
    assets = output / 'assets'
    assets.mkdir()
    root = Path(__file__).resolve().parents[2]
    fixtures = root / 'Tools/TwoD/Fixtures'
    with tempfile.TemporaryDirectory(prefix='twod-product-source-', dir=output.parent) as directory:
        source = Path(directory)
        shutil.copy2(fixtures / 'sheet.png', source / 'sheet.png')
        atlas = json.loads((fixtures / 'atlas.json').read_text())
        clip = json.loads((fixtures / 'clip.json').read_text())
        atlas['id'], clip['id'], clip['atlas'] = [1, 2], [3, 4], [1, 2]
        for frame in clip['frames']:
            frame['event'] = 'step'  # Existing Zig sample's event contract.
        for name, manifest in [('atlas', atlas), ('clip', clip)]:
            manifest_file = source / f'{name}.json'
            manifest_file.write_text(json.dumps(manifest), encoding='utf-8')
            canonical = source / f'{name}.nx2{name}'
            subprocess.run([sys.executable, str(root / 'Tools/TwoD/PrepareAssets.py'),
                            str(manifest_file), '--compiler', str(compiler),
                            '--output', str(canonical)], check=True)
            cooked = assets / f'{name}.nxcook'
            subprocess.run([str(compiler), 'cook', str(canonical), str(cooked)], check=True)
            files.append(cooked)
    tone = assets / 'tone.wav'
    with wave.open(str(tone), 'wb') as audio:
        audio.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
        audio.writeframes(b''.join(struct.pack('<h', int(3276 * math.sin(i * 2 * math.pi * 440 / 48000)))
                                   for i in range(4800)))
    files.append(tone)
    hashes = {path.relative_to(output).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
              for path in files}
    manifest = {'schema': 'nexora.twod.service-acceptance.v1', 'runtime': target.name,
                'files': hashes, 'editor_required': False,
                'development_test_copy': allow_development,
                'audible_acceptance': 'NOT VERIFIED', 'windows_clean_host': 'NOT RUN',
                'system_prerequisites': 'Target OS C/C++ runtime and native graphics/audio drivers'}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(stage(args.runtime, args.compiler, args.output), indent=2))


if __name__ == '__main__':
    main()
