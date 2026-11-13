"""Export, relocate and execute source-bearing authored documents without authoring files."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).parent))
from SpriteSourcePlayerAcceptance import pixels


def run(*args, cwd=None, env=None):
    result = subprocess.run([str(x) for x in args], cwd=cwd, capture_output=True, text=True,
                            encoding='utf-8', errors='replace', timeout=90, env=env)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result


def native(package, app, output, phase, studio=None):
    sys.path.insert(0, str(ROOT/'Tests/Showcase'))
    from LinuxVirtualDisplaySmoke import start_xvfb
    from HD2DAcceptance import NativeWindow
    xvfb, display = start_xvfb(shutil.which('Xvfb'), '800x600x24')
    process = reader = window = None
    lines, events = [], queue.Queue()
    try:
        assert display, 'native display unavailable'
        env = dict(os.environ, DISPLAY=display)
        report = output/(phase+'-native.json')
        process = subprocess.Popen([str(package/app.name), '--run-2d-project',
            str(package/'game.nx2game'), '--frames', '30', '--native', '--backend', 'vulkan',
            '--report', str(report)], cwd=output, env=env, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, encoding='utf-8')
        def consume():
            for line in process.stderr:
                lines.append(line)
                if 'AUTHORED SOURCE CAPTURE' in line:
                    events.put(True)
            events.put(False)
        reader = threading.Thread(target=consume)
        reader.start()
        assert events.get(timeout=30), ''.join(lines)
        found = subprocess.check_output([shutil.which('xdotool'), 'search', '--onlyvisible',
            '--name', '^Authored Source Fixture$'], env=env, text=True, timeout=5).splitlines()
        assert len(found) == 1, found
        window = NativeWindow(display)
        window.window = int(found[0])
        path = output/(phase+'.png')
        window.capture(path)
        width, height, sample = pixels(path)
        assert (width, height) == (320, 240), (width, height)
        red, blue = (172, 148) if phase == 'initial' else (148, 172)
        assert sample(red, 108) == (255, 0, 0), (phase, red, sample(red, 108))
        assert sample(blue, 108) == (0, 0, 255), (phase, blue, sample(blue, 108))
        assert max(sample(112, 108)) < 200 and max(sample(208, 108)) < 200
        process.wait(timeout=30)
        stdout = process.stdout.read()
        reader.join(timeout=5)
        assert process.returncode == 0 and not reader.is_alive(), ''.join(lines)
        assert 'AUTHORED SOURCE PASS' in ''.join(lines), ''.join(lines)
        (output/(phase+'-native.stdout.log')).write_text(stdout)
        result = json.loads(report.read_text())
        assert result['status'] == 'PASS' and result['native_frames'] == 30 and result['allocations_balanced']
        if studio:
            document = package/'scenes/0000.scene'
            baseline = document.read_bytes()
            opened = run(studio, '--workspace', package, '--document', 'scenes/0000.scene',
                         '--read-only', '--frames', '6', cwd=output, env=env)
            assert document.read_bytes() == baseline
            assert '2D Studio native preview and document lifecycle passed' in opened.stdout
            (output/(phase+'-studio.log')).write_text(opened.stdout+opened.stderr)
    finally:
        if process and process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        if reader:
            reader.join(timeout=5)
        if window:
            window.close()
        xvfb.terminate()
        xvfb.wait(timeout=5)
        (output/(phase+'-native.stderr.log')).write_text(''.join(lines))


def main():
    parser = argparse.ArgumentParser()
    for flag in ('app', 'fixture', 'module', 'compiler', 'evidence-dir'):
        parser.add_argument('--'+flag, type=Path, required=True)
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--shipping', action='store_true')
    parser.add_argument('--studio', type=Path)
    args = parser.parse_args()
    args.evidence_dir.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='attempt-', dir=args.evidence_dir.resolve()))
    with tempfile.TemporaryDirectory(prefix='nexora-authored-source-') as temporary:
        source = Path(temporary)/'authored Unicode 來源'
        source.mkdir()
        run(args.fixture, source)
        shutil.copy2(args.module, source/args.module.name)
        packages = []
        for phase in ('initial', 'shifted'):
            config = dict(schema=1, id='com.nexora.authored-source', title='Authored Source Fixture',
                scene=phase+'.scene', module=args.module.name, audio='off', assets=[],
                display=dict(width=320, height=240, mode='windowed', resizable=False),
                scenes=[dict(id=[91, 92], path=phase+'.scene')])
            (source/'game.json').write_text(json.dumps(config), encoding='utf-8')
            package = source/(phase+'-package')
            command = [sys.executable, ROOT/'Tools/TwoD/ExportGame.py', source/'game.json',
                '--runtime', args.app, '--compiler', args.compiler, '--output', package]
            if not args.shipping:
                command += ['--allow-development']
            run(*command)
            before = {p.relative_to(package): p.read_bytes() for p in package.rglob('*') if p.is_file()}
            document = source/(phase+'.scene')
            good = document.read_bytes()
            document.write_bytes(good+b'corrupt')
            rejected = subprocess.run([str(x) for x in command], capture_output=True,
                                      text=True, timeout=90)
            assert rejected.returncode != 0, rejected.stdout+rejected.stderr
            assert before == {p.relative_to(package): p.read_bytes()
                              for p in package.rglob('*') if p.is_file()}, 'failed export changed package'
            document.write_bytes(good)
            moved = output/('moved 遊戲 '+phase)
            shutil.move(str(package), moved)
            packages.append((phase, moved))
        config['scene'] = 'initial.scene'
        config['scenes'] = [dict(id=[91, 92], path='initial.scene'),
                            dict(id=[91, 93], path='shifted.scene')]
        (source/'game.json').write_text(json.dumps(config), encoding='utf-8')
        conflict = output/'conflicting-sources'
        command = [sys.executable, ROOT/'Tools/TwoD/ExportGame.py', source/'game.json',
                   '--runtime', args.app, '--compiler', args.compiler, '--output', conflict]
        if not args.shipping:
            command += ['--allow-development']
        run(*command)
        shutil.rmtree(source)
        rejected = subprocess.run([str(conflict/args.app.name), '--run-2d-project',
            str(conflict/'game.nx2game'), '--frames', '30'], cwd=output,
            capture_output=True, text=True, timeout=30)
        assert rejected.returncode != 0 and 'conflicting authored scene Sprite source identity' in rejected.stderr, rejected.stdout+rejected.stderr
        assert 'AUTHORED SOURCE PASS' not in rejected.stderr
        (output/'conflict.log').write_text(rejected.stdout+rejected.stderr)
        results = {}
        for phase, package in packages:
            report = output/(phase+'-cpu.json')
            result = run(package/args.app.name, '--run-2d-project', package/'game.nx2game',
                         '--frames', '30', '--report', report, cwd=output)
            (output/(phase+'-cpu.log')).write_text(result.stdout+result.stderr)
            status = json.loads(report.read_text())
            assert status['status'] == 'PASS' and status['allocations_balanced'], status
            assert 'AUTHORED SOURCE PASS' in result.stderr, result.stderr
            if args.native:
                native(package, args.app, output, phase, args.studio)
            results[phase] = {p.relative_to(package).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in package.rglob('*') if p.is_file()}
        (output/'receipt.json').write_text(json.dumps(dict(source_deleted=True, packages=results), indent=2)+'\n')
    print('PASS authored source document export/relocation, Player ownership and native='+str(args.native))


if __name__ == '__main__':
    main()
