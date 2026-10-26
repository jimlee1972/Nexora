"""Package and natively verify a relocated Windows Development Editor (stdlib only)."""
import argparse
import hashlib
import json
import ntpath
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

from EditorTargetEvidence import smoke_result

OS_IMPORTS = set("kernel32 kernelbase ntdll user32 gdi32 imm32 advapi32 shell32 shlwapi ole32 "
                 "oleaut32 comdlg32 comctl32 ucrtbase msvcrt ws2_32 winmm winhttp bcrypt "
                 "crypt32 secur32 setupapi cfgmgr32 version dbghelp d3d12 d3d11 dxgi "
                 "d3dcompiler_47 xinput1_4 dinput8".split())


def require_gui_subsystem(binary):
    """Reject the console PE subsystem that creates an extra window on Explorer launch."""
    with binary.open('rb') as stream:
        dos = stream.read(64)
        if len(dos) != 64 or dos[:2] != b'MZ':
            raise RuntimeError('Invalid Windows Editor DOS header')
        offset = struct.unpack_from('<I', dos, 60)[0]
        if not 64 <= offset <= 1024 * 1024:
            raise RuntimeError('Invalid Windows Editor PE header offset')
        stream.seek(offset)
        header = stream.read(24)
        if len(header) != 24 or header[:4] != b'PE\0\0' or struct.unpack_from('<H', header, 20)[0] < 70:
            raise RuntimeError('Invalid Windows Editor PE header')
        optional = stream.read(70)
        if len(optional) != 70 or struct.unpack_from('<H', optional)[0] not in (0x10b, 0x20b):
            raise RuntimeError('Invalid Windows Editor optional header')
        if struct.unpack_from('<H', optional, 68)[0] != 2:
            raise RuntimeError('Editor must use the Windows GUI subsystem; console builds create an extra window')


def imports(binary):
    output = subprocess.check_output(['dumpbin', '/DEPENDENTS', str(binary)])
    return re.findall(r'^\s*(\S+\.dll)\s*$', output.decode('ascii', errors='replace'),
                      re.MULTILINE | re.IGNORECASE)


def dependency_closure(editor, redist, reader=imports, openssl_root=None):
    """Resolve engine imports beside the EXE and CRT imports only from explicit redist."""
    engine = {p.name.lower(): p for p in editor.parent.glob('*.dll')}
    crt = {p.name.lower(): p for p in redist.glob('*.dll')}
    found = {editor.name.lower(): editor}
    pending = [editor]
    while pending:
        for name in reader(pending.pop()):
            name = name.lower()
            if name in found or name.removesuffix('.dll') in OS_IMPORTS or name.startswith(('api-ms-win-', 'ext-ms-win-')):
                continue
            if name.startswith('nexora'):
                source = engine.get(name)
            elif name == 'libcrypto-3-x64.dll' and openssl_root is not None:
                source = openssl_root / 'bin' / name
                if not source.is_file():
                    source = None
            elif name.startswith(('vcruntime', 'msvcp', 'concrt')):
                source = crt.get(name)
            else:
                raise RuntimeError(f'Unclassified dependency: {name}')
            if source is None:
                raise RuntimeError(f'Missing dependency: {name}')
            found[name] = source
            pending.append(source)
    return list(found.values())


def stage_game_build_runtime(package, profile, redist, cmake_root, ninja, zig_root, ninja_license,
                             reader=imports):
    """Install the private game host and compiler resources; never copy an engine checkout."""
    records = profile.read_text(encoding='utf-8').splitlines()
    if not records or records[0] != 'NEXORA_GAME_RUNTIME_1':
        raise RuntimeError('Invalid trusted game runtime profile')
    player = None
    libraries = []
    for line in records[1:]:
        key, separator, value = line.partition('=')
        if not separator or key not in ('player', 'runtime') or not value:
            raise RuntimeError('Invalid game runtime inventory record')
        source = Path(value)
        if not source.is_absolute():
            if source.name != value or value in ('.', '..'):
                raise RuntimeError('Installed game inventory requires flat names')
            source = profile.parent / source
        if key == 'player':
            if player is not None:
                raise RuntimeError('Duplicate game host')
            player = source
        else:
            libraries.append(source)
    if player is None or not libraries:
        raise RuntimeError('Incomplete game runtime profile')
    runtime = package / 'Runtime'
    runtime.mkdir()
    found = {}
    for source in dependency_closure(player, redist, reader) + libraries:
        name = source.name.lower()
        if (name in found and found[name].resolve() != source.resolve() and
                digest(found[name]) != digest(source)):
            raise RuntimeError('Conflicting installed runtime filename: ' + name)
        found[name] = source
    for source in found.values():
        shutil.copy2(source, runtime / source.name)
    installed_libraries = sorted(path.name for name, path in found.items() if name != player.name.lower())
    (runtime / 'GameRuntime.profile').write_text('NEXORA_GAME_RUNTIME_1\nplayer=' + player.name +
        '\n' + ''.join('runtime=' + name + '\n' for name in installed_libraries), encoding='utf-8')
    tools = package / 'Tools'
    cmake_bin = tools / 'cmake/bin'
    cmake_bin.mkdir(parents=True)
    for source in dependency_closure(cmake_root / 'bin/cmake.exe', redist, reader):
        shutil.copy2(source, cmake_bin / source.name)
    # CMake resolves its module/SDK data relative to bin, independent of PATH and the build PC.
    shutil.copytree(cmake_root / 'share', tools / 'cmake/share')
    shutil.copy2(ninja, tools / 'ninja.exe')
    zig = tools / 'zig'
    zig.mkdir()
    shutil.copy2(zig_root / 'zig.exe', zig / 'zig.exe')
    shutil.copytree(zig_root / 'lib', zig / 'lib')
    licenses = package / 'licenses/game-build-tools'
    licenses.mkdir(parents=True)
    shutil.copy2(zig_root / 'LICENSE', licenses / 'Zig-MIT.txt')
    shutil.copy2(ninja_license, licenses / 'Ninja-license.txt')
    copyrights = list((cmake_root / 'doc').rglob('Copyright.txt'))
    if not copyrights:
        raise RuntimeError('CMake distribution copyright notices are missing')
    shutil.copytree(cmake_root / 'doc', licenses / 'CMake')
    return {'runtime_files': len(found), 'cmake': 'Tools/cmake/bin/cmake.exe',
            'ninja': 'Tools/ninja.exe', 'zig': 'Tools/zig/zig.exe',
            'resources': 'Compiler standard libraries and CMake modules; no Nexora/game source checkout'}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def relocated_environment(source):
    # Windows environment names are case-insensitive; plain dict copies are not.
    environment = {key.upper(): value for key, value in source.items()}
    windows = environment['SYSTEMROOT']
    environment['PATH'] = ';'.join((ntpath.join(windows, 'System32'), windows))
    return environment


def verify(package, evidence):
    """Run outside the build tree with a restricted DLL search PATH; preserve all logs."""
    require_gui_subsystem(package / 'NexoraEditor.exe')
    evidence.mkdir(parents=True, exist_ok=False)
    with tempfile.TemporaryDirectory(prefix='Nexora relocated 陰陽 ') as temporary:
        root = Path(temporary)
        relocated = root / 'Editor package'
        shutil.copytree(package, relocated)
        cwd = root / 'empty working directory'
        cwd.mkdir()
        executable = relocated / 'NexoraEditor.exe'
        environment = relocated_environment(os.environ)
        environment['NEXORA_EDITOR_STARTUP_EVIDENCE'] = '1'
        recent = '--recent-projects=' + str(root / 'recent-projects')

        def run(label, arguments, expected=0, frames=None, use_recents=True, timeout=90):
            try:
                result = subprocess.run([str(executable), *arguments, *([recent] if use_recents else [])], cwd=cwd,
                                        env=environment, capture_output=True, timeout=timeout)
            except subprocess.TimeoutExpired as error:
                output = (error.stdout or b'') + b'\n' + (error.stderr or b'')
                (evidence / (label + '.log')).write_bytes(output)
                raise RuntimeError(f'{label} timed out; see {evidence}') from error
            output = (result.stdout + b'\n' + result.stderr).decode('utf-8', errors='replace')
            (evidence / (label + '.log')).write_text(output, encoding='utf-8')
            if 'editor GUI startup: console_attached=0' not in output:
                raise RuntimeError(f'{label} did not verify a console-free GUI entrypoint; see {evidence}')
            if result.returncode != expected or (frames and smoke_result(result.returncode, output, frames) != 'pass'):
                raise RuntimeError(f'{label} failed: exit {result.returncode}; see {evidence}; '
                                   f'output tail: {output[-4096:]}')
            return result.stdout.decode('utf-8')

        help_text = run('help', ['--help'])
        if '--create-project=' not in help_text or '--project-name=' not in help_text:
            raise RuntimeError('Packaged Editor does not support project CLI')
        run('default-browser', ['--frames=8'], frames=8)
        project = root / '陰陽 CLI project'
        name = 'Onmyoji 陰陽 Survivors'
        created = json.loads(run('create', ['--create-project=' + str(project), '--project-name=' + name]))
        if created['project'] != name:
            raise RuntimeError('Created project name mismatch')
        original = (project / 'project.nexora').read_bytes()
        run('reopen', ['--project=' + str(project), '--read-only'])
        run('reject-existing', ['--create-project=' + str(project), '--project-name=Replacement'], expected=1)
        run('graphical-open', ['--project=' + str(project), '--graphical', '--frames=120'], frames=120)
        for name in ('Main.overview.camera', 'Main.preview.camera'):
            if not (project / '.nexora/scenes' / name).is_file():
                raise RuntimeError(f'Graphical close did not persist {name} under Unicode project path')
        run('graphical-create', ['--create-project=' + str(root / 'graphical 陰陽 project'),
                                '--project-name=' + name, '--graphical', '--frames=8'], frames=8)
        if (project / 'project.nexora').read_bytes() != original:
            raise RuntimeError('Reopen or duplicate creation changed project identity')
        if (relocated / 'Runtime/GameRuntime.profile').is_file():
            repository = Path(__file__).resolve().parents[2]
            sys.path.insert(0, str(repository / 'Tests/EditorImGui'))
            from PackagedPlayProject import write_fixture
            authored = root / 'Scene build source'
            authored.mkdir()
            write_fixture(authored)
            (authored / 'Scripts').mkdir()
            shutil.copy2(repository / 'Tests/Runtime/GameSceneScript/game.zig', authored / 'Scripts/game.zig')
            (authored / 'Scripts/project.json').write_text(
                '{"schema":1,"module_name":"PortableGame","script":"game.zig"}')
            output = root / 'Game output'
            result = json.loads(run('script-game-build', ['--build-game=' + str(authored),
                '--output=' + str(output), '--script-project=Scripts/project.json'],
                use_recents=False, timeout=180))
            if result['status'] != 'PUBLISHED_GAME':
                raise RuntimeError('Installed script game was not published')
            commands = (output / 'Build.commands').read_text(encoding='utf-8')
            if not all(name in commands for name in ('Tools/cmake/bin/cmake.exe', 'Tools/ninja.exe',
                                                       'Tools/zig/zig.exe')):
                raise RuntimeError('Game build did not use the shipped tools')
            catalog = subprocess.run([str(output / 'Game.exe'), '--verify-game-package',
                str(output / 'Game.nxgame')], cwd=cwd, env=environment,
                capture_output=True, timeout=15)
            data = json.loads(catalog.stdout) if catalog.returncode == 0 else {}
            if data.get('scene_count') != 2 or data.get('asset_count') != 4 or data.get('start_scene') != '00000000-0000-0001-0000-000000000002':
                raise RuntimeError('Game catalog did not exclude the third scene or deduplicate shared assets')
            hashes = {p.name: digest(p) for p in output.iterdir() if p.is_file()}
            hidden = root / 'Scene build source.hidden'
            if not authored.resolve().is_relative_to(root.resolve()) or not hidden.resolve().is_relative_to(root.resolve()):
                raise RuntimeError('Fixture relocation escaped verification directory')
            authored.rename(hidden)
            native = subprocess.run([str(output / 'Game.exe'), '--run-game-package',
                str(output / 'Game.nxgame'), '--backend=dx12',
                '--gameplay-module=' + str(output / 'Game.dll'), '--frames=100'],
                cwd=cwd, env=environment, capture_output=True, timeout=90)
            (evidence / 'script-game-native.stdout.log').write_bytes(native.stdout)
            (evidence / 'script-game-native.stderr.log').write_bytes(native.stderr)
            report = json.loads(native.stdout) if native.returncode == 0 else {}
            if (native.returncode != 0 or report.get('scene_commits') != 3 or
                    report.get('presented_frames') != 100 or not report.get('drained') or
                    any(digest(output / name) != value for name, value in hashes.items())):
                raise RuntimeError('Relocated authored-source-free game execution failed')
            write_json(evidence / 'game-build.json', {'status': 'PASS', 'runtime': report,
                       'hashes': hashes, 'scope': 'Installed tools, script build and bounded native run; no physical pixel claim'})
    write_json(evidence / 'result.json', {'status': 'PASS', 'platform': 'Windows native process',
                                        'editor_subsystem': 'Windows GUI (2)',
                                        'console_attached': False,
                                        'physical_host_acceptance': 'NOT_RUN'})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--redist', type=Path, required=True)
    parser.add_argument('--imgui-license', type=Path, required=True)
    parser.add_argument('--openssl-root', type=Path)
    parser.add_argument('--game-runtime-profile', type=Path)
    parser.add_argument('--cmake-root', type=Path)
    parser.add_argument('--ninja', type=Path)
    parser.add_argument('--zig-root', type=Path)
    parser.add_argument('--ninja-license', type=Path)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt' or not re.fullmatch('[0-9a-f]{40}', args.commit):
        parser.error('Requires Windows and full source commit SHA')
    game_tools = (args.game_runtime_profile, args.cmake_root, args.ninja, args.zig_root, args.ninja_license)
    if any(game_tools) and not all(game_tools):
        parser.error('Game builds require the runtime profile and all explicit tool/resource/license paths')
    require_gui_subsystem(args.editor.resolve())
    if args.output.exists():
        parser.error('Output must be new; existing packages are never overwritten')
    args.output.mkdir(parents=True)
    try:
        package = args.output / 'NexoraEditor-Windows-x64'
        package.mkdir()
        for source in dependency_closure(args.editor.resolve(), args.redist.resolve(),
                                         openssl_root=args.openssl_root.resolve() if args.openssl_root else None):
            shutil.copy2(source, package / source.name)
        licenses = package / 'licenses'
        licenses.mkdir()
        shutil.copy2(Path(__file__).resolve().parents[2] / 'LICENSE', licenses / 'Nexora-MIT.txt')
        shutil.copy2(args.imgui_license, licenses / 'DearImGui-MIT.txt')
        if args.openssl_root:
            openssl_license = next((args.openssl_root / name for name in ('LICENSE.txt', 'LICENSE', 'LICENSE.md')
                                    if (args.openssl_root / name).is_file()), None)
            if openssl_license is None:
                raise RuntimeError('OpenSSL installation license is missing')
            shutil.copy2(openssl_license, licenses / 'OpenSSL-license.txt')
        tool_info = None
        if all(game_tools):
            tool_info = stage_game_build_runtime(package, args.game_runtime_profile.resolve(),
                args.redist.resolve(), args.cmake_root.resolve(), args.ninja.resolve(),
                args.zig_root.resolve(), args.ninja_license.resolve())
        if not tool_info:
            (package / 'Run.cmd').write_bytes(b'@echo off\r\n"%~dp0NexoraEditor.exe" %*\r\n')
        (package / 'README.txt').write_text(
            'Nexora Editor - Windows x64 Development, DX12\n'
            'Double-click NexoraEditor.exe to open the project browser; no arguments required.\n'
            'Use NexoraEditor.exe --help for explicit project/create CLI commands.\n'
            'Keep this entire directory together; do not replace only an old launcher EXE.\n' +
            ('Runtime/ and Tools/ are private game-build dependencies. Keep them with the Editor.\n'
             'No Nexora/game source checkout, extra 2D apps, or game module is included.\n'
             'Tools/ includes compiler standard-library and CMake module resources for game builds.\n'
             if tool_info else 'No Player, asset compiler, source, Zig compiler, or game module is included.\n') +
            'Microsoft C++ runtime DLLs are redistributed under Visual Studio runtime terms:\n'
            'https://visualstudio.microsoft.com/license-terms/\n'
            'Hosted CI smoke does not replace physical Windows DPI/IME/game acceptance.\n', encoding='utf-8')
        write_json(package / 'build-info.json', {'schema': 1, 'source_commit': args.commit,
                   'configuration': 'Development', 'architecture': 'x64', 'backend': 'DX12',
                   'build_testing': True, 'game_build_tools': tool_info})
        verify(package, args.output / 'verification')
        write_json(package / 'inventory.json', {
            p.relative_to(package).as_posix(): digest(p) for p in sorted(package.rglob('*')) if p.is_file()})
        archive = Path(shutil.make_archive(str(args.output / ('NexoraEditor-Windows-x64-' + args.commit[:12])),
                                         'zip', root_dir=args.output, base_dir=package.name))
        archive.with_suffix('.zip.sha256').write_text(digest(archive) + '  ' + archive.name + '\n', encoding='ascii')
        print(archive)
    except Exception:
        # Preserve failed staging and logs, but never publish a successful ZIP on failure.
        raise


if __name__ == '__main__':
    main()
