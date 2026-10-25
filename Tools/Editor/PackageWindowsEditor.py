"""Package and natively verify a relocated Windows Development Editor (stdlib only)."""
import argparse
import hashlib
import json
import ntpath
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from EditorTargetEvidence import smoke_result

OS_IMPORTS = set("kernel32 kernelbase ntdll user32 gdi32 imm32 advapi32 shell32 shlwapi ole32 "
                 "oleaut32 comdlg32 comctl32 ucrtbase msvcrt ws2_32 winmm winhttp bcrypt "
                 "crypt32 secur32 setupapi cfgmgr32 version dbghelp d3d12 d3d11 dxgi "
                 "d3dcompiler_47 xinput1_4 dinput8".split())


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
    evidence.mkdir(parents=True, exist_ok=False)
    with tempfile.TemporaryDirectory(prefix='Nexora relocated 陰陽 ') as temporary:
        root = Path(temporary)
        relocated = root / 'Editor package'
        shutil.copytree(package, relocated)
        cwd = root / 'empty working directory'
        cwd.mkdir()
        executable = relocated / 'NexoraEditor.exe'
        environment = relocated_environment(os.environ)
        recent = '--recent-projects=' + str(root / 'recent-projects')

        def run(label, arguments, expected=0, frames=None):
            try:
                result = subprocess.run([str(executable), *arguments, recent], cwd=cwd,
                                        env=environment, capture_output=True, timeout=90)
            except subprocess.TimeoutExpired as error:
                output = (error.stdout or b'') + b'\n' + (error.stderr or b'')
                (evidence / (label + '.log')).write_bytes(output)
                raise RuntimeError(f'{label} timed out; see {evidence}') from error
            output = (result.stdout + b'\n' + result.stderr).decode('utf-8', errors='replace')
            (evidence / (label + '.log')).write_text(output, encoding='utf-8')
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
    write_json(evidence / 'result.json', {'status': 'PASS', 'platform': 'Windows hosted runner',
                                        'physical_host_acceptance': 'NOT_RUN'})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--editor', type=Path, required=True)
    parser.add_argument('--redist', type=Path, required=True)
    parser.add_argument('--imgui-license', type=Path, required=True)
    parser.add_argument('--openssl-root', type=Path, required=True)
    parser.add_argument('--commit', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if os.name != 'nt' or not re.fullmatch('[0-9a-f]{40}', args.commit):
        parser.error('Requires Windows and full source commit SHA')
    if args.output.exists():
        parser.error('Output must be new; existing packages are never overwritten')
    args.output.mkdir(parents=True)
    try:
        package = args.output / 'NexoraEditor-Windows-x64'
        package.mkdir()
        for source in dependency_closure(args.editor.resolve(), args.redist.resolve(),
                                         openssl_root=args.openssl_root.resolve()):
            shutil.copy2(source, package / source.name)
        licenses = package / 'licenses'
        licenses.mkdir()
        shutil.copy2(Path(__file__).resolve().parents[2] / 'LICENSE', licenses / 'Nexora-MIT.txt')
        shutil.copy2(args.imgui_license, licenses / 'DearImGui-MIT.txt')
        openssl_license = next((args.openssl_root / name for name in ('LICENSE.txt', 'LICENSE', 'LICENSE.md')
                                if (args.openssl_root / name).is_file()), None)
        if openssl_license is None:
            raise RuntimeError('OpenSSL installation license is missing')
        shutil.copy2(openssl_license, licenses / 'OpenSSL-license.txt')
        (package / 'Run.cmd').write_bytes(b'@echo off\r\n"%~dp0NexoraEditor.exe" %*\r\n')
        (package / 'README.txt').write_text(
            'Nexora Editor - Windows x64 Development, DX12\n'
            'Double-click NexoraEditor.exe to open the project browser; no arguments required.\n'
            'Use NexoraEditor.exe --help for explicit project/create CLI commands.\n'
            'Keep this entire directory together; do not replace only an old launcher EXE.\n'
            'No Player, asset compiler, source, Zig compiler, or game module is included.\n'
            'Microsoft C++ runtime DLLs are redistributed under Visual Studio runtime terms:\n'
            'https://visualstudio.microsoft.com/license-terms/\n'
            'Hosted CI smoke does not replace physical Windows DPI/IME/game acceptance.\n', encoding='utf-8')
        write_json(package / 'build-info.json', {'schema': 1, 'source_commit': args.commit,
                   'configuration': 'Development', 'architecture': 'x64', 'backend': 'DX12',
                   'build_testing': True})
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
