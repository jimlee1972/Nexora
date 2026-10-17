#!/usr/bin/env python3
"""Run a real independent module through export, restart, corruption and path admission."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('export_game', ROOT / 'Tools/TwoD/ExportGame.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


def digest(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def run(app, manifest, saves, success=True, loaded=None, native=False):
    result = subprocess.run([str(app), '--run-2d-project', str(manifest), '--frames', '3',
                             '--backend', 'vulkan' if native and sys.platform.startswith('linux') else 'auto',
                             '--save-directory', str(saves)] + (['--native'] if native else []), capture_output=True, text=True)
    assert (result.returncode == 0) == success, (result.stdout, result.stderr)
    if success:
        report = json.loads(result.stdout)
        assert report['project_mode'] and report['allocations_balanced']
        assert report['surface_backend_requested'] == ('vulkan' if native and sys.platform.startswith('linux') else 'auto' if native else 'none')
        assert report['unfocused_movement_frames'] == 0
        assert not report['reloaded'] and report['visible'] == 0
        assert f'project visits loaded={loaded}' in result.stderr, result.stderr
        assert 'project stopped' in result.stderr
        assert report['ui_frames'] == 3
        if native:
            assert report['native_frames'] == 3 and report['native_ui_draws'] >= 3
    return result


def acceptance(args):
    with tempfile.TemporaryDirectory(prefix='nexora-game-project-') as temporary:
        root = Path(temporary)
        # MSVC's heading can contain bytes undefined in Python's Windows codepage. Import
        # names still need strict admission; replacing heading text must not hide unknown DLLs.
        heading = b'Dump of file C:\\unicode-\x81\x9d\\module.dll\r\n'
        with mock.patch.object(exporter.sys, 'platform', 'win32'):
            with mock.patch.object(exporter.subprocess, 'check_output',
                                   return_value=heading + b'    KERNEL32.dll\r\n'):
                exporter.dependencies(root / 'module.dll', root, False, None)
            with mock.patch.object(exporter.subprocess, 'check_output',
                                   return_value=heading + b'    unknown-third-party.dll\r\n'):
                try:
                    exporter.dependencies(root / 'module.dll', root, False, None)
                except ValueError:
                    pass
                else:
                    raise AssertionError('unknown DLL was hidden by heading decoding')
        source = root / 'source Unicode 遊戲'
        source.mkdir()
        shutil.copy2(args.module, source / args.module.name)
        config = {'schema': 1, 'id': 'com.nexora.test', 'title': 'Project Unicode 遊戲',
                  'scene': 'Main', 'module': args.module.name, 'audio': 'off', 'assets': []}
        descriptor = source / 'game.json'
        descriptor.write_text(json.dumps(config, ensure_ascii=False), encoding='utf-8')
        baseline = digest(source)
        package = root / 'export Unicode 封裝'
        exporter.export(descriptor, args.app, package, allow_development=True, redist=args.redist)
        assert digest(source) == baseline
        before = digest(package)
        inventory = json.loads((package / 'inventory.json').read_text())
        assert inventory['development_test_copy']
        assert all(before[path] == value for path, value in inventory['files'].items())
        app = package / args.app.name
        # Linux development runtime uses the explicitly staged engine libraries.
        if sys.platform.startswith('linux'):
            os.environ['LD_LIBRARY_PATH'] = str(package) + os.pathsep + os.environ.get('LD_LIBRARY_PATH', '')
        manifest = package / 'game.nx2game'
        original = manifest.read_text(encoding='utf-8')
        saves = root / 'saves Unicode 進度'
        run(app, manifest, saves, loaded=0)
        run(app, manifest, saves, loaded=1, native=args.native)
        assert digest(package) == before, 'player modified immutable game package'
        slot = saves / 'visits.nxsav'
        corrupt = bytearray(slot.read_bytes())
        corrupt[-1] ^= 1
        slot.write_bytes(corrupt)
        run(app, manifest, saves, success=False)
        assert slot.read_bytes() == corrupt, 'corrupt save overwritten'
        for bad in (original + 'id "duplicate"\n',
                    original.replace('"Main"', '"Other"'),
                    original.replace('gameplay' + args.module.suffix, '../missing' + args.module.suffix),
                    original + 'wave 1 "missing.wav"\n',
                    original.replace('com.nexora.test', 'CON'),
                    original + 'font 1 "bad.nxfont"\n'):
            (package / 'bad.nxfont').write_bytes(b'not a font')
            manifest.write_text(bad, encoding='utf-8')
            run(app, manifest, root / 'invalid-saves', success=False)
        manifest.write_text(original, encoding='utf-8')
        for bad in ('../outside.so', '/absolute.so', 'bad\\path.so', 'bad:stream.so'):
            config['module'] = bad
            descriptor.write_text(json.dumps(config), encoding='utf-8')
            rejected = root / 'rejected'
            try:
                exporter.export(descriptor, args.app, rejected, allow_development=True, redist=args.redist)
            except (ValueError, OSError):
                pass
            else:
                raise AssertionError('invalid export admitted')
            assert not rejected.exists(), 'partial export published'
    print('Project export: independent module, UI-only game, Unicode paths, process restart, immutable package and corruption rejection passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', required=True, type=Path)
    parser.add_argument('--module', required=True, type=Path)
    parser.add_argument('--redist', type=Path)
    parser.add_argument('--native', action='store_true')
    args = parser.parse_args()
    if args.native:
        sys.path.insert(0, str(ROOT / 'Tests/Showcase'))
        from LinuxVirtualDisplaySmoke import start_xvfb, unavailable
        xvfb = shutil.which('Xvfb')
        if not xvfb:
            sys.exit(unavailable('Xvfb missing'))
        server, display = start_xvfb(xvfb, '800x600x24')
        try:
            if not display:
                sys.exit(unavailable('Xvfb unavailable'))
            os.environ['DISPLAY'] = display
            acceptance(args)
        finally:
            server.terminate()
            server.communicate(timeout=3)
    else:
        acceptance(args)
