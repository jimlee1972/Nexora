#!/usr/bin/env python3
"""Create and verify a relocatable Windows x64 script-only 2D development SDK."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
import zipfile

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('game_export', ROOT / 'Tools/TwoD/ExportGame.py')
EXPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORT)


def digest(path):
    return EXPORT.checksum(path)


def files(root):
    result = {}
    for p in sorted(root.rglob('*')):
        if p.is_symlink():
            raise ValueError('SDK must not contain symlinks')
        if p.is_file() and p != root / 'sdk.json':
            result[p.relative_to(root).as_posix()] = digest(p)
    return result


def verify(root):
    manifest = json.loads((root / 'sdk.json').read_text(encoding='utf-8'), object_pairs_hook=EXPORT.unique_object)
    expected = dict(schema=1, target='x86_64-windows-msvc', backend='dx12',
                    configuration='Shipping', link_mode='Monolithic', abi=3, zig='0.14.0', ninja='1.13.2')
    if any(manifest.get(k) != v for k, v in expected.items()):
        raise ValueError('incompatible SDK contract')
    if (not re.fullmatch(r'[0-9a-f]{40}', manifest.get('source_commit', ''))
            or not re.fullmatch(r'\d+\.\d+\.\d+', manifest.get('version', ''))):
        raise ValueError('invalid SDK identity')
    actual = files(root)
    if actual != manifest.get('files'):
        raise ValueError('SDK inventory mismatch (modified, missing or extra files)')
    required = ('bin/NexoraGameHost.exe', 'bin/NexoraTwoDAssetCompiler.exe',
                'Tools/Ninja/ninja.exe', 'Tools/Zig/build.zig', 'Tools/Zig/game_entry.zig',
                'Bindings/Zig/nexora.zig', 'CMake/Modules/NexoraGameProject.cmake')
    if any(p not in actual for p in required):
        raise ValueError('incomplete SDK')
    return manifest


def package(args):
    cache = (args.build / 'CMakeCache.txt').read_text(encoding='utf-8')
    for key, value in dict(CMAKE_BUILD_TYPE='Shipping', NEXORA_LINK_MODE='Monolithic',
                          NEXORA_ENABLE_EDITOR='OFF', NEXORA_ENABLE_VULKAN_BACKEND='OFF',
                          NEXORA_2D_GAME_TARGET='NexoraGameHost', NEXORA_2D_GAME_PROJECT='game.nx2game').items():
        if not re.search(r'^'+key+r':[^=]+='+value+r'$', cache, re.M):
            raise ValueError('SDK producer configuration mismatch: '+key)
    if not re.fullmatch(r'[0-9a-f]{40}', args.commit) or not re.fullmatch(r'\d+\.\d+\.\d+', args.version):
        raise ValueError('invalid SDK version/source')
    args.output.mkdir(parents=True, exist_ok=False)
    root = args.output / 'sdk'
    (root / 'bin').mkdir(parents=True)
    for name in ('NexoraGameHost.exe', 'NexoraTwoDAssetCompiler.exe'):
        candidates = list(args.build.rglob(name))
        if len(candidates) != 1:
            raise ValueError('expected one '+name)
        shutil.copy2(candidates[0], root / 'bin' / name)
        EXPORT.dependencies(candidates[0], root / 'bin', False, args.redist)
    for directory in ('Bindings/Zig', 'Tools/Zig'):
        shutil.copytree(ROOT / directory, root / directory, ignore=shutil.ignore_patterns('__pycache__', '.zig-cache', 'zig-out'))
    for name in ('CMake/Modules/NexoraGameProject.cmake', 'Tools/TwoD/ExportGame.py',
                 'Tools/TwoD/PrepareAssets.py', 'Tools/SDK/SDK.py', 'Tools/SDK/README.md'):
        target = root / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(ROOT / name, target)
    (root / 'Tools/Ninja').mkdir(parents=True)
    shutil.copy2(args.ninja, root / 'Tools/Ninja/ninja.exe')
    if subprocess.check_output([str(args.ninja), '--version'], text=True).strip() != '1.13.2':
        raise ValueError('Ninja must be 1.13.2')
    (root / 'licenses').mkdir()
    shutil.copy2(ROOT / 'LICENSE', root / 'licenses/Nexora-MIT.txt')
    shutil.copy2(args.ninja_license, root / 'licenses/Ninja-Apache-2.0.txt')
    (root / 'licenses/Microsoft-Runtime.txt').write_text(
        'Microsoft Visual C++ runtime libraries are redistributed under Visual Studio runtime terms.\n'
        'https://visualstudio.microsoft.com/license-terms/\n'
        'The Universal CRT and DX12 are Windows components; a supported Windows installation and driver are required.\n', encoding='utf-8')
    manifest = dict(schema=1, version=args.version, source_commit=args.commit,
                    target='x86_64-windows-msvc', backend='dx12', configuration='Shipping',
                    link_mode='Monolithic', abi=3, zig='0.14.0', ninja='1.13.2', files=files(root))
    (root / 'sdk.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    verify(root)
    archive = args.output / f'Nexora-2D-SDK-Windows-x64-{args.version}.zip'
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as z:
        for p in sorted(root.rglob('*')):
            if p.is_file():
                z.write(p, p.relative_to(root).as_posix())
    archive.with_suffix('.zip.sha256').write_text(digest(archive)+'  '+archive.name+'\n', encoding='ascii')
    shutil.copy2(root / 'sdk.json', args.output / 'sdk.json')
    print(archive)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    check = commands.add_parser('verify')
    check.add_argument('root', type=Path)
    pack = commands.add_parser('package')
    for field in ('build', 'output', 'redist', 'ninja', 'ninja-license'):
        pack.add_argument('--'+field, type=Path, required=True)
    pack.add_argument('--commit', required=True)
    pack.add_argument('--version', default='0.1.0')
    args = parser.parse_args()
    if args.command == 'verify':
        print(json.dumps(verify(args.root), indent=2))
    else:
        package(args)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
