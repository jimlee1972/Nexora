#!/usr/bin/env python3
"""Stage an explicit trusted 2D game descriptor and Shipping player in a fresh directory."""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import tempfile

WINDOWS_OS_IMPORTS = {
    'kernel32.dll', 'kernelbase.dll', 'ntdll.dll', 'user32.dll', 'gdi32.dll', 'imm32.dll',
    'advapi32.dll', 'shell32.dll', 'shlwapi.dll', 'ole32.dll', 'oleaut32.dll', 'comdlg32.dll',
    'comctl32.dll', 'ucrtbase.dll', 'msvcrt.dll', 'ws2_32.dll', 'winmm.dll', 'winhttp.dll',
    'bcrypt.dll', 'crypt32.dll', 'secur32.dll', 'setupapi.dll', 'version.dll', 'dbghelp.dll',
    'd3d12.dll', 'd3d11.dll', 'dxgi.dll', 'd3dcompiler_47.dll', 'vulkan-1.dll', 'xinput1_4.dll',
}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate descriptor field: ' + key)
        result[key] = value
    return result


def checksum(path):
    value = hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda: file.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def local(root, name):
    if not isinstance(name, str) or '\0' in name:
        raise ValueError('invalid source path')
    path = PurePosixPath(name)
    if (not isinstance(name, str) or not name or len(name.encode('utf-8')) > 1024
            or path.is_absolute() or '\\' in name or ':' in name
            or any(part in ('', '.', '..') for part in name.split('/'))):
        raise ValueError('source paths must be contained relative forward-slash paths')
    result = root
    for part in path.parts:
        result = result / part
        if result.is_symlink():
            raise ValueError('symlink source paths are not supported')
    if not result.is_file():
        raise ValueError(f'missing source file: {name}')
    return result


def dependencies(binary, destination, allow_development, redist):
    if sys.platform == 'win32':
        # Only dependency names are ASCII. The source path in dumpbin's heading can use a
        # different Windows console encoding from Python's locale (notably CJK directories).
        output = subprocess.check_output(['dumpbin', '/DEPENDENTS', str(binary)]).decode(
            'ascii', errors='replace')
        names = re.findall(r'^\s*(\S+\.dll)\s*$', output, re.MULTILINE | re.IGNORECASE)
        for name in names:
            lower = name.lower()
            if 'nexora' in lower:
                if 'editor' in lower or not allow_development:
                    raise ValueError('Windows export requires an Editor-free Monolithic Shipping build')
                candidates = [binary.parent] + [Path(p) for p in os.environ.get('PATH', '').split(os.pathsep) if p]
                source = next((p / name for p in candidates if (p / name).is_file()), None)
                if not source:
                    raise ValueError('unresolved engine dependency: ' + name)
                target = destination / name
                if not target.exists():
                    shutil.copy2(source, target)
                    dependencies(source, destination, allow_development, redist)
                continue
            if lower.startswith(('vcruntime', 'msvcp', 'concrt')):
                source = (redist / name) if redist else binary.parent / name
                if not source.is_file() and allow_development:
                    continue  # Explicit test copies use the build host's existing CRT.
                if not source.is_file():
                    raise ValueError(f'{name} missing; pass --vc-redist with the matching x64 CRT directory')
                target = destination / name
                if not target.exists():
                    shutil.copy2(source, target)
                    dependencies(source, destination, allow_development, redist)
            elif lower not in WINDOWS_OS_IMPORTS and not lower.startswith(('api-ms-win-', 'ext-ms-win-')):
                raise ValueError('unhandled non-system DLL dependency: ' + name)
    elif sys.platform.startswith('linux'):
        output = subprocess.check_output(['ldd', str(binary)], text=True)
        if 'not found' in output or 'NexoraEditor' in output:
            raise ValueError('unresolved or Editor dependency')
        for line in output.splitlines():
            if 'Nexora' not in line:
                continue
            if not allow_development:
                raise ValueError('export requires a Monolithic Shipping build')
            source = Path(line.split('=>', 1)[1].strip().split()[0])
            shutil.copy2(source, destination / source.name)
    else:
        raise ValueError('game export currently supports Windows and Linux')


def export(descriptor, runtime, output, compiler=None, allow_development=False, redist=None):
    descriptor, runtime, output = descriptor.resolve(), runtime.resolve(), output.absolute()
    if output.exists():
        raise ValueError('output must be a fresh directory')
    if not re.fullmatch(r'[A-Za-z0-9_.-]+', runtime.name):
        raise ValueError('runtime filename must be a portable executable name')
    if descriptor.stat().st_size > 1024 * 1024:
        raise ValueError('descriptor exceeds 1 MiB')
    config = json.loads(descriptor.read_text(encoding='utf-8'), object_pairs_hook=unique_object)
    if not isinstance(config, dict):
        raise ValueError('game descriptor must be an object')
    if set(config) - {'schema', 'id', 'title', 'scene', 'module', 'audio', 'assets', 'tile_atlas'}:
        raise ValueError('unknown game descriptor field')
    if type(config.get('schema')) is not int or config['schema'] != 1 or not re.fullmatch(r'[a-z0-9](?:[a-z0-9._-]{0,126}[a-z0-9])?', config.get('id', '')):
        raise ValueError('invalid game schema or ID')
    for key in ('title', 'scene'):
        text = config.get(key)
        if (not isinstance(text, str) or not text or len(text.encode('utf-8')) > (256 if key == 'scene' else 1024)
                or any(ord(c) < 32 for c in text)):
            raise ValueError(f'invalid {key}')
    audio = config.get('audio', 'optional')
    if audio not in ('off', 'optional', 'required'):
        raise ValueError('invalid audio policy')
    assets = config.get('assets', [])
    if not isinstance(assets, list) or len(assets) > 4096:
        raise ValueError('asset count exceeds 4096')
    module = local(descriptor.parent, config['module'])
    if module.suffix.lower() != ('.dll' if sys.platform == 'win32' else '.so'):
        raise ValueError('module must match the target host')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='nexora-game-export-', dir=output.parent) as temporary:
        staging = Path(temporary) / 'game'
        staging.mkdir()
        shutil.copy2(runtime, staging / runtime.name)
        dependencies(runtime, staging, allow_development, redist)
        shutil.copy2(module, staging / ('gameplay' + module.suffix))
        dependencies(module, staging, allow_development, redist)
        (staging / 'assets').mkdir()
        quote = lambda value: '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'
        lines = ['NEXORA_2D_GAME 1']
        for key in ('id', 'title', 'scene'):
            lines.append(f'{key} {quote(config[key])}')
        lines.extend([f'module {quote("gameplay" + module.suffix)}', f'audio {quote(audio)}'])
        if 'tile_atlas' in config:
            identity = config['tile_atlas']
            if (not isinstance(identity, list) or len(identity) != 2
                    or any(type(value) is not int or value < 0 or value > 2**64-1 for value in identity)
                    or not any(identity)):
                raise ValueError('invalid tile atlas identity')
            lines.append(f'tile_atlas {identity[0]} {identity[1]}')
        ids, paths, total = set(), set(), 0
        for index, entry in enumerate(assets):
            if not isinstance(entry, dict):
                raise ValueError('asset descriptor must be an object')
            if set(entry) - {'kind', 'path', 'id'}:
                raise ValueError('unknown asset field')
            kind = entry['kind']
            if kind not in ('atlas', 'clip', 'wave', 'font') or entry['path'] in paths:
                raise ValueError('invalid or duplicate asset')
            paths.add(entry['path'])
            source = local(descriptor.parent, entry['path'])
            total += source.stat().st_size
            if total > 64 * 1024 * 1024:
                raise ValueError('source assets exceed 64 MiB')
            suffix = '.nxcook' if kind in ('atlas', 'clip') else '.wav' if kind == 'wave' else '.nxfont'
            target = staging / 'assets' / (f'{index:04d}' + suffix)
            if kind in ('atlas', 'clip') and source.suffix.lower() != '.nxcook':
                if not compiler or source.suffix.lower() != '.nx2' + kind:
                    raise ValueError('canonical Atlas/Clip sources require --compiler')
                canonical = Path(temporary) / ('source.nx2' + kind)
                shutil.copy2(source, canonical)
                subprocess.run([str(compiler.resolve()), 'cook', canonical.name,
                                'game/assets/' + target.name], cwd=temporary, check=True)
            else:
                shutil.copy2(source, target)
            prefix = ''
            if kind in ('wave', 'font'):
                value = entry.get('id')
                if (type(value) is not int or value < 1 or value > (32 if kind == 'font' else 2**64-1)
                        or (kind, value) in ids):
                    raise ValueError('invalid or duplicate service asset ID')
                ids.add((kind, value))
                prefix = f'{value} '
            lines.append(f'{kind} {prefix}{quote("assets/" + target.name)}')
        if sum(path.stat().st_size for path in (staging / 'assets').iterdir()) > 64 * 1024 * 1024:
            raise ValueError('cooked assets exceed 64 MiB')
        (staging / 'game.nx2game').write_text('\n'.join(lines) + '\n', encoding='utf-8')
        if sys.platform == 'win32':
            (staging / 'Run.cmd').write_bytes(('@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\n"' + runtime.name +
                '" --run-2d-project game.nx2game %*\r\nset "nexora_game_exit=%ERRORLEVEL%"\r\n'
                'if not "%nexora_game_exit%"=="0" pause\r\nexit /b %nexora_game_exit%\r\n').encode('ascii'))
        else:
            launcher = staging / 'run.sh'
            launcher.write_text('#!/bin/sh\ncd -- "$(dirname -- "$0")" || exit 1\nexec "./' +
                                runtime.name + '" --run-2d-project game.nx2game "$@"\n', encoding='utf-8')
            launcher.chmod(0o755)
        inventory = {str(path.relative_to(staging)).replace(os.sep, '/'):
                     checksum(path)
                     for path in sorted(staging.rglob('*')) if path.is_file()}
        (staging / 'inventory.json').write_text(json.dumps({'schema': 1, 'files': inventory,
            'development_test_copy': allow_development}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        staging.rename(output)
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('descriptor', type=Path)
    parser.add_argument('--runtime', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--vc-redist', type=Path)
    parser.add_argument('--allow-development', action='store_true')
    args = parser.parse_args()
    print(export(args.descriptor, args.runtime, args.output, args.compiler,
                 args.allow_development, args.vc_redist))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
