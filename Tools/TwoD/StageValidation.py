#!/usr/bin/env python3
"""Stage a monolithic 2D runtime and cooked assets; verify hashes and run the staged CPU fixture."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime', required=True, type=Path)
    p.add_argument('--compiler', required=True, type=Path)
    p.add_argument('--atlas', required=True, type=Path)
    p.add_argument('--clip', required=True, type=Path)
    p.add_argument('--output', required=True, type=Path)
    args = p.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if (output/'manifest.json').exists():
        raise ValueError('use a fresh package directory')
    runtime = output/args.runtime.name
    shutil.copy2(args.runtime, runtime)
    assets = output/'assets'
    assets.mkdir(exist_ok=True)
    for source, target in [(args.atlas, assets/'atlas.nxcook'), (args.clip, assets/'clip.nxcook')]:
        subprocess.run([str(args.compiler.resolve()), 'cook', str(source.resolve()), str(target)], check=True)
    if sys.platform.startswith('linux'):
        dependencies = subprocess.check_output(['ldd', str(runtime)], text=True)
        if 'Nexora' in dependencies or 'not found' in dependencies:
            raise ValueError('requires monolithic runtime without unresolved engine DLLs')
    hashes = {path.relative_to(output).as_posix():hashlib.sha256(path.read_bytes()).hexdigest()
              for path in [runtime, assets/'atlas.nxcook', assets/'clip.nxcook']}
    (output/'manifest.json').write_text(json.dumps({'schema':'nexora.twod.package.v1', 'files':hashes,
        'runtime':runtime.name, 'editor_required':False,
        'system_runtime_dependencies':'Use the target OS runtime and native graphics driver.',
        'windows_clean_machine_acceptance':'NOT RUN'}, indent=2)+'\n')
    environment = os.environ.copy()
    environment.pop('LD_LIBRARY_PATH', None)
    environment.pop('DYLD_LIBRARY_PATH', None)
    command = [str(runtime), '--atlas', str(assets/'atlas.nxcook'), '--clip', str(assets/'clip.nxcook'),
               '--frames', '8', '--report', str(output/'packaged-stress.csv')]
    run = subprocess.run(command, cwd=output, env=environment, capture_output=True, text=True, timeout=60)
    (output/'acceptance.log').write_text(run.stdout+run.stderr)
    if run.returncode:
        raise RuntimeError(f'staged runtime failed: {run.stdout}{run.stderr}')
    for relative, expected in hashes.items():
        assert hashlib.sha256((output/relative).read_bytes()).hexdigest() == expected
    broken = bytearray((assets/'atlas.nxcook').read_bytes())
    broken[-1] ^= 1
    (assets/'corrupt.nxcook').write_bytes(broken)
    rejected = subprocess.run([str(runtime), '--atlas', str(assets/'corrupt.nxcook'), '--frames', '4',
                               '--report', str(output/'rejected.csv')], cwd=output, env=environment,
                              capture_output=True, text=True, timeout=30)
    (assets/'corrupt.nxcook').unlink()
    (output/'rejected.csv').unlink(missing_ok=True)
    if rejected.returncode == 0:
        raise AssertionError('runtime admitted corrupt cook')
    print('Staged monolithic runtime, cooked async loading, hashes and corruption rejection passed')

if __name__ == '__main__': main()
