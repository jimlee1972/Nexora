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
import struct
import sys
import tempfile

WINDOWS_OS_IMPORTS = {
    'kernel32.dll', 'kernelbase.dll', 'ntdll.dll', 'user32.dll', 'gdi32.dll', 'imm32.dll',
    'advapi32.dll', 'shell32.dll', 'shlwapi.dll', 'ole32.dll', 'oleaut32.dll', 'comdlg32.dll',
    'comctl32.dll', 'ucrtbase.dll', 'msvcrt.dll', 'ws2_32.dll', 'winmm.dll', 'winhttp.dll',
    'bcrypt.dll', 'crypt32.dll', 'secur32.dll', 'setupapi.dll', 'cfgmgr32.dll', 'version.dll', 'dbghelp.dll',
    'd3d12.dll', 'd3d11.dll', 'dxgi.dll', 'd3dcompiler_47.dll', 'xinput1_4.dll',
    'dinput8.dll',
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


def pe_imports(binary):
    """Read bounded PE normal/delay imports without a platform toolchain."""
    if binary.stat().st_size > 256 * 1024 * 1024:
        raise ValueError('PE exceeds 256 MiB')
    data = binary.read_bytes()

    def unpack(fmt, offset):
        size = struct.calcsize(fmt)
        if offset < 0 or offset + size > len(data):
            raise ValueError('truncated PE')
        return struct.unpack_from(fmt, data, offset)

    if data[:2] != b'MZ':
        raise ValueError('invalid DOS signature')
    pe, = unpack('<I', 60)
    if data[pe:pe+4] != b'PE\0\0':
        raise ValueError('invalid PE signature')
    machine, sections, _, _, _, optional_size, _ = unpack('<HHIIIHH', pe+4)
    opt = pe+24
    magic, = unpack('<H', opt)
    if (machine, magic) not in ((0x8664, 0x20b), (0x14c, 0x10b)):
        raise ValueError('unsupported PE architecture')
    directory_offset, count_offset = (112, 108) if magic == 0x20b else (96, 92)
    image_base, = unpack('<Q' if magic == 0x20b else '<I', opt+(24 if magic == 0x20b else 28))
    count, = unpack('<I', opt+count_offset)
    if not 1 <= sections <= 96 or optional_size < directory_offset or count > 16:
        raise ValueError('invalid PE headers')
    if directory_offset + count*8 > optional_size:
        raise ValueError('truncated PE directories')
    headers, = unpack('<I', opt+60)
    ranges = []
    for i in range(sections):
        _, va, size, raw = unpack('<IIII', opt+optional_size+i*40+8)
        if raw + size > len(data):
            raise ValueError('truncated PE section')
        ranges.append((va, size, raw))

    def offset(rva, size):
        if 0 <= rva < headers and rva + size <= min(headers, len(data)):
            return rva
        for va, length, raw in ranges:
            if va <= rva and rva+size <= va+length:
                return raw+rva-va
        raise ValueError('unmapped PE RVA')

    def name_at(rva):
        name = bytearray()
        for i in range(261):
            c = data[offset(rva+i, 1)]
            if c == 0:
                value = name.decode('ascii')
                if not re.fullmatch(r'[A-Za-z0-9_.-]+[.]dll', value, re.IGNORECASE):
                    raise ValueError('invalid PE dependency name')
                return value
            name.append(c)
        raise ValueError('unterminated PE import name')

    names = set()
    for index, width in ((1, 20), (13, 32)):
        if index >= count:
            continue
        rva, size = unpack('<II', opt+directory_offset+index*8)
        if not rva and not size:
            continue
        if not rva or size < width or size > width*4096:
            raise ValueError('invalid PE import directory')
        ended = False
        for i in range(size // width):
            values = unpack('<'+'I'*(width//4), offset(rva+i*width, width))
            if not any(values):
                ended = True
                break
            name = values[3] if index == 1 else values[1]
            if index == 13:
                if values[0] & ~1:
                    raise ValueError('invalid delay import attributes')
                if not values[0] & 1:
                    name -= image_base
            names.add(name_at(name))
        if not ended:
            raise ValueError('unterminated PE import directory')
    return sorted(names, key=str.lower)


def macos_development_dependencies(roots, destination, executable_directory):
    """Copy a trusted Development dyld graph and remove build-tree load paths.

    This deliberately does not provide a signed/notarized macOS Shipping distribution.
    Discover the complete graph before modifying copies; source binaries stay untouched.
    """
    owners, edges, visited = {}, {}, set()

    def expand(value, loader):
        for prefix, base in (('@loader_path', loader), ('@executable_path', executable_directory)):
            if value == prefix or value.startswith(prefix + '/'):
                return base / value[len(prefix):].lstrip('/')
        return Path(value)

    def inspect(source, name, inherited):
        source = source.resolve()
        if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', name) or 'editor' in name.lower():
            raise ValueError('unsupported macOS diagnostic dependency: ' + name)
        if name in owners and owners[name] != source:
            raise ValueError('conflicting macOS dependency filename: ' + name)
        owners[name] = source
        if (source, name) in visited:
            return
        visited.add((source, name))
        commands = subprocess.check_output(['otool', '-l', str(source)], text=True)
        paths = [expand(path, source.parent) for path in re.findall(
            r'\bcmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset \d+\)', commands)] + list(inherited)
        identity = {line.strip() for line in subprocess.check_output(
            ['otool', '-D', str(source)], text=True).splitlines()[1:]}
        imports = subprocess.check_output(['otool', '-L', str(source)], text=True).splitlines()[1:]
        edges[name] = []
        for line in imports:
            imported = line.strip().split(' (compatibility version', 1)[0]
            if imported in identity or imported.startswith(('/usr/lib/', '/System/Library/')):
                continue
            if '.framework/' in imported:
                raise ValueError('non-system macOS framework distribution is unsupported: ' + imported)
            if imported.startswith('@rpath/'):
                candidates = [path / imported[len('@rpath/'):] for path in paths]
            elif imported.startswith(('@loader_path/', '@executable_path/')):
                candidates = [expand(imported, source.parent)]
            elif Path(imported).is_absolute():
                candidates = [Path(imported)]
            else:
                candidates = [source.parent / imported]
            dependency = next((p for p in candidates if p.is_file()), None)
            if dependency is None:
                raise ValueError('unresolved macOS dependency: ' + imported)
            dependency_name = Path(imported).name
            edges[name].append((imported, '@loader_path/' + dependency_name))
            inspect(dependency, dependency_name, paths)

    for source, name in roots:
        inspect(source, name, ())
    for name, source in owners.items():
        shutil.copy2(source, destination / name)
    for name, replacements in edges.items():
        if replacements:
            command = ['install_name_tool']
            for old, new in replacements:
                command += ['-change', old, new]
            subprocess.run(command + [str(destination / name)], check=True)
    # Mach-O mutations invalidate signatures on Apple Silicon. Sign copied dependencies first.
    for name in reversed(list(owners)):
        subprocess.run(['codesign', '--force', '--sign', '-', str(destination / name)], check=True)


def dependencies(binary, destination, allow_development, redist, vulkan_loader=None):
    if sys.platform == 'win32':
        names = pe_imports(binary)
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
                    dependencies(source, destination, allow_development, redist, vulkan_loader)
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
                    dependencies(source, destination, allow_development, redist, vulkan_loader)
            elif lower == 'vulkan-1.dll':
                # The Vulkan loader is not a Windows OS component. Static imports require it
                # even for headless/DX12 startup; never borrow the build machine's PATH copy.
                if (vulkan_loader is None or vulkan_loader.name.lower() != 'vulkan-1.dll' or
                        not vulkan_loader.is_file()):
                    raise ValueError('vulkan-1.dll missing; pass --vulkan-loader with the matching redistributable loader')
                target = destination / 'vulkan-1.dll'
                if not target.exists():
                    shutil.copy2(vulkan_loader, target)
                    dependencies(vulkan_loader, destination, allow_development, redist, vulkan_loader)
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


def export(descriptor, runtime, output, compiler=None, allow_development=False, redist=None,
           vulkan_loader=None, executable_name=None):
    descriptor, runtime, output = descriptor.resolve(), runtime.resolve(), output.absolute()
    if output.exists():
        raise ValueError('output must be a fresh directory')
    executable_name = runtime.name if executable_name is None else executable_name
    if (not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', executable_name)
            or executable_name in ('.', '..')
            or (sys.platform == 'win32' and not executable_name.lower().endswith('.exe'))):
        raise ValueError('runtime filename must be a portable executable name')
    if descriptor.stat().st_size > 1024 * 1024:
        raise ValueError('descriptor exceeds 1 MiB')
    config = json.loads(descriptor.read_text(encoding='utf-8'), object_pairs_hook=unique_object)
    if not isinstance(config, dict):
        raise ValueError('game descriptor must be an object')
    if set(config) - {'schema', 'id', 'title', 'scene', 'module', 'audio', 'assets', 'tile_atlas', 'display', 'capacity', 'audio_capacity'}:
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
    display = config.get('display')
    if 'display' in config:
        if (not isinstance(display, dict) or set(display) != {'width', 'height', 'mode', 'resizable'}
                or any(type(display[k]) is not int or not 1 <= display[k] <= 16384 for k in ('width', 'height'))
                or display['mode'] not in ('windowed', 'borderless')
                or type(display['resizable']) is not bool):
            raise ValueError('invalid display settings')
    capacity = config.get('capacity')
    capacity_keys = ('active_assets', 'active_bytes', 'live_bytes', 'staging_bytes', 'pool_instances')
    if 'capacity' in config:
        bounds = ((1, 4096), (1, 1024**3), (1, 2*1024**3), (1, 128*1024**2), (1, 100000))
        if (not isinstance(capacity, dict) or set(capacity) != set(capacity_keys)
                or any(type(capacity[k]) is not int or not low <= capacity[k] <= high
                       for k, (low, high) in zip(capacity_keys, bounds))
                or capacity['live_bytes'] < capacity['active_bytes']):
            raise ValueError('invalid asset capacity')
    audio_capacity = config.get('audio_capacity')
    if 'audio_capacity' in config:
        keys = ('assets', 'active_bytes', 'live_bytes')
        if (not isinstance(audio_capacity,dict) or set(audio_capacity) != set(keys)
                or any(type(audio_capacity[k]) is not int or not 1 <= audio_capacity[k] <= limit
                       for k,limit in zip(keys,(4096,1024**3,2*1024**3)))
                or audio_capacity['live_bytes'] < audio_capacity['active_bytes']):
            raise ValueError('invalid audio capacity')
    assets = config.get('assets', [])
    if not isinstance(assets, list) or len(assets) > 4096:
        raise ValueError('asset count exceeds 4096')
    module = local(descriptor.parent, config['module'])
    module_suffix = '.dll' if sys.platform == 'win32' else '.dylib' if sys.platform == 'darwin' else '.so'
    if module.suffix.lower() != module_suffix:
        raise ValueError('module must match the target host')
    if sys.platform == 'darwin' and not allow_development:
        raise ValueError('macOS diagnostic export requires --allow-development; Shipping distribution is not provided')
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='nexora-game-export-', dir=output.parent) as temporary:
        staging = Path(temporary) / 'game'
        staging.mkdir()
        shutil.copy2(runtime, staging / executable_name)
        shutil.copy2(module, staging / ('gameplay' + module.suffix))
        if sys.platform == 'darwin':
            macos_development_dependencies([(runtime, executable_name), (module, 'gameplay' + module.suffix)],
                                          staging, runtime.parent)
        else:
            dependencies(runtime, staging, allow_development, redist, vulkan_loader)
            dependencies(module, staging, allow_development, redist, vulkan_loader)
        (staging / 'assets').mkdir()
        quote = lambda value: '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'
        lines = ['NEXORA_2D_GAME 1']
        for key in ('id', 'title', 'scene'):
            lines.append(f'{key} {quote(config[key])}')
        lines.extend([f'module {quote("gameplay" + module.suffix)}', f'audio {quote(audio)}'])
        if audio_capacity is not None:
            lines.append('audio_capacity ' + ' '.join(str(audio_capacity[k]) for k in ('assets','active_bytes','live_bytes')))
        if 'tile_atlas' in config:
            identity = config['tile_atlas']
            if (not isinstance(identity, list) or len(identity) != 2
                    or any(type(value) is not int or value < 0 or value > 2**64-1 for value in identity)
                    or not any(identity)):
                raise ValueError('invalid tile atlas identity')
            lines.append(f'tile_atlas {identity[0]} {identity[1]}')
        ids, paths, total = set(), set(), 0
        preload_targets = []
        for index, entry in enumerate(assets):
            if not isinstance(entry, dict):
                raise ValueError('asset descriptor must be an object')
            if set(entry) - {'kind', 'path', 'id', 'preload'}:
                raise ValueError('unknown asset field')
            kind = entry['kind']
            if kind not in ('atlas', 'clip', 'wave', 'font', 'blob') or entry['path'] in paths:
                raise ValueError('invalid or duplicate asset')
            paths.add(entry['path'])
            source = local(descriptor.parent, entry['path'])
            preload = entry.get('preload', kind != 'blob')
            if type(preload) is not bool or (not preload and kind in ('wave', 'font')) or (preload and kind == 'blob'):
                raise ValueError('invalid asset preload policy; use a deferred blob for game-owned bytes')
            size = source.stat().st_size
            if not 0 < size <= 64 * 1024 * 1024:
                raise ValueError('asset exceeds per-file 64 MiB limit')
            total += size if preload else 0
            if total > 64 * 1024 * 1024:
                raise ValueError('preloaded source assets exceed 64 MiB')
            suffix = '.nxcook' if kind in ('atlas', 'clip') else '.wav' if kind == 'wave' else '.nxfont' if kind == 'font' else '.bin'
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
            if not 0 < target.stat().st_size <= 64 * 1024 * 1024:
                raise ValueError('cooked asset exceeds per-file 64 MiB limit')
            if preload:
                preload_targets.append(target)
            manifest_kind = kind if preload else 'deferred_' + kind
            lines.append(f'{manifest_kind} {prefix}{quote("assets/" + target.name)}')
        if sum(path.stat().st_size for path in preload_targets) > 64 * 1024 * 1024:
            raise ValueError('cooked assets exceed 64 MiB')
        if capacity is not None:
            lines.append('capacity ' + ' '.join(str(capacity[k]) for k in capacity_keys))
        if display is not None:
            lines.append(f"display {display['width']} {display['height']} {display['mode']} {int(display['resizable'])}")
        (staging / 'game.nx2game').write_text('\n'.join(lines) + '\n', encoding='utf-8')
        if sys.platform == 'win32':
            (staging / 'Run.cmd').write_bytes(('@echo off\r\nsetlocal\r\ncd /d "%~dp0"\r\n"' + executable_name +
                '" --run-2d-project game.nx2game %*\r\nset "nexora_game_exit=%ERRORLEVEL%"\r\n'
                'if not "%nexora_game_exit%"=="0" pause\r\nexit /b %nexora_game_exit%\r\n').encode('ascii'))
        else:
            launcher = staging / 'run.sh'
            launcher.write_text('#!/bin/sh\ncd -- "$(dirname -- "$0")" || exit 1\nexec "./' +
                                executable_name + '" --run-2d-project game.nx2game "$@"\n', encoding='utf-8')
            launcher.chmod(0o755)
        inventory = {str(path.relative_to(staging)).replace(os.sep, '/'):
                     checksum(path)
                     for path in sorted(staging.rglob('*')) if path.is_file()}
        (staging / 'inventory.json').write_text(json.dumps({'schema': 1, 'files': inventory,
            'development_test_copy': allow_development}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        staging.rename(output)
    return output


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    sys.stderr.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('descriptor', type=Path)
    parser.add_argument('--runtime', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--executable-name', help='copy the generic host with this portable filename')
    parser.add_argument('--vc-redist', type=Path)
    parser.add_argument('--vulkan-loader', type=Path,
                        help='explicit matching redistributable vulkan-1.dll, required when imported')
    parser.add_argument('--allow-development', action='store_true')
    args = parser.parse_args()
    print(export(args.descriptor, args.runtime, args.output, args.compiler,
                 args.allow_development, args.vc_redist, args.vulkan_loader, args.executable_name))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
