"""Exercise the actual Zig SDK/source API in a relocated canonical Player, including pixels."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import shutil
import struct
import subprocess
import sys
import tempfile
import threading
import time
import zlib

ROOT = Path(__file__).resolve().parents[2]


def run(*args, cwd=None, log=None):
    result = subprocess.run([str(a) for a in args], cwd=cwd, capture_output=True, text=True,
                            encoding='utf-8', errors='replace', timeout=240)
    if log:
        log.write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result


def pixels(path):
    data = path.read_bytes()
    width, height = struct.unpack_from('>II', data, 16)
    assert data[24:29] == bytes([8, 2, 0, 0, 0]), 'expected RGB8 capture'
    compressed = bytearray()
    offset = 8
    while offset < len(data):
        size, = struct.unpack_from('>I', data, offset)
        if data[offset+4:offset+8] == b'IDAT':
            compressed.extend(data[offset+8:offset+8+size])
        offset += size + 12
    raw = zlib.decompress(compressed)
    assert len(raw) == height * (width * 3 + 1)
    assert all(raw[y * (width * 3 + 1)] == 0 for y in range(height))
    def sample(x, y):
        i = y * (width * 3 + 1) + 1 + x * 3
        return tuple(raw[i:i+3])
    return width, height, sample


def repack_pixels(phase, sample):
    colors = dict(R=(255, 0, 0), G=(0, 255, 0), B=(0, 0, 255), Y=(255, 255, 0),
                  C=(0, 255, 255), M=(255, 0, 255), W=(255, 255, 255), K=(0, 0, 0))
    # Hand-authored screen-space expectations; the pool's negative X scale composes with each flip.
    scene, pool = {'base': ('RGBY', 'GRYB'), 'flip-x': ('GRYB', 'RGBY'),
                   'flip-y': ('BYRG', 'YBGR'), 'flip-xy': ('YBGR', 'BYRG'),
                   'repacked': ('KWMC', 'WKCM')}[phase]
    # Trim local bounds [-2,0]x[2,4] mirror around the noncentral source Pivot.
    scene_x, pool_x = ((124, 148), (196, 220)) if phase in ('flip-x', 'flip-xy', 'repacked') else (
        (76, 100), (244, 268))
    ys = (180, 204) if phase in ('flip-y', 'flip-xy', 'repacked') else (36, 60)
    occupied = set()
    for pattern, xs in ((scene, scene_x), (pool, pool_x)):
        for color, (x, y) in zip(pattern, ((xs[0], ys[0]), (xs[1], ys[0]), (xs[0], ys[1]), (xs[1], ys[1]))):
            occupied.add((x, y))
            rgb = sample(x, y)
            assert all(abs(a-b) <= 8 for a, b in zip(rgb, colors[color])), (phase, x, y, rgb, color)
    for x in (76, 100, 124, 148, 196, 220, 244, 268):
        for y in (36, 60, 180, 204):
            if (x, y) not in occupied:
                assert max(sample(x, y)) < 200, (phase, 'stale geometry', x, y, sample(x, y))
    for x in (52, 172, 292):
        for y in (12, 48, 84, 156, 192, 228):
            assert max(sample(x, y)) < 200, (phase, 'geometry drift', x, y, sample(x, y))


def native(package, app, output, repack=False):
    sys.path.insert(0, str(ROOT/'Tests/Showcase'))
    from LinuxVirtualDisplaySmoke import start_xvfb
    from HD2DAcceptance import NativeWindow
    xvfb, xdotool = shutil.which('Xvfb'), shutil.which('xdotool')
    if not xvfb or not xdotool:
        raise RuntimeError('source Player pixels require Xvfb and xdotool')
    server, display = start_xvfb(xvfb, '800x600x24')
    process = reader = window = None
    lines, events, captures = [], queue.Queue(), {}
    try:
        if not display:
            raise RuntimeError('source Player display unavailable')
        env = dict(os.environ, DISPLAY=display)
        process = subprocess.Popen([str(package/app.name), '--run-2d-project', str(package/'game.nx2game'),
            '--frames', '30', '--native', '--backend', 'vulkan', '--report', str(output/'native.json')],
            cwd=output, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding='utf-8')
        def consume():
            for line in process.stderr:
                lines.append(line)
                if 'SOURCE PLAYER CAPTURE ' in line:
                    events.put(line.split('SOURCE PLAYER CAPTURE ', 1)[1].strip())
        reader = threading.Thread(target=consume)
        reader.start()
        window = NativeWindow(display)
        deadline = time.monotonic() + 60
        while process.poll() is None or not events.empty():
            if time.monotonic() >= deadline:
                raise RuntimeError('native source Player deadline exceeded')
            try:
                phase = events.get(timeout=0.1)
            except queue.Empty:
                continue
            found = subprocess.check_output([xdotool, 'search', '--onlyvisible', '--name',
                '^Sprite Source Fixture$'], env=env, text=True, timeout=5).splitlines()
            assert len(found) == 1, found
            window.window = int(found[0])
            path = output/(phase+'.png')
            captures[phase] = window.capture(path)
            width, height, sample = pixels(path)
            assert (width, height) == (320, 240), (width, height)
            if repack:
                repack_pixels(phase, sample)
            else:
                colored = (124, 244) if phase == 'red' else (76, 292)
                empty = (76, 292) if phase == 'red' else (124, 244)
                for x in colored:
                    rgb = sample(x, 120)
                    assert (rgb[0] > 240 and rgb[1] < 5 and rgb[2] < 5) if phase == 'red' else (
                        rgb[2] > 240 and rgb[0] < 5 and rgb[1] < 5), (phase, x, rgb)
                for x in empty:
                    assert max(sample(x, 120)) < 200, (phase, x, sample(x, 120))
        stdout = process.stdout.read()
        process.wait(timeout=5)
        reader.join(timeout=5)
        (output/'native.stdout.log').write_text(stdout)
        (output/'native.stderr.log').write_text(''.join(lines))
        assert process.returncode == 0 and not reader.is_alive(), ''.join(lines)
        expected = {'base', 'flip-x', 'flip-y', 'flip-xy', 'repacked'} if repack else {'red', 'blue'}
        assert set(captures) == expected, captures
        assert 'SOURCE PLAYER PASS' in ''.join(lines) and 'SOURCE PLAYER FAIL' not in ''.join(lines)
        report = json.loads((output/'native.json').read_text())
        assert report['status'] == 'PASS' and report['native_frames'] == 30 and report['allocations_balanced']
        (output/'captures.json').write_text(json.dumps(captures, indent=2)+'\n')
    finally:
        if process and process.poll() is None:
            process.kill(); process.wait(timeout=5)
        if reader:
            reader.join(timeout=5)
        if window:
            window.close()
        server.terminate(); server.wait(timeout=5)
        if lines:
            (output/'native.stderr.log').write_text(''.join(lines))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--app', required=True, type=Path)
    parser.add_argument('--zig', required=True)
    parser.add_argument('--cmake', required=True)
    parser.add_argument('--evidence-dir', required=True, type=Path)
    parser.add_argument('--native', action='store_true')
    parser.add_argument('--catalog', action='store_true')
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--shipping', action='store_true')
    parser.add_argument('--repack', action='store_true')
    args = parser.parse_args()
    if args.catalog and not args.compiler:
        parser.error('--catalog requires --compiler')
    if args.repack and not args.catalog:
        parser.error('--repack requires --catalog')
    evidence = args.evidence_dir.resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='attempt-', dir=evidence))
    (evidence/'latest.txt').write_text(output.name+'\n')
    with tempfile.TemporaryDirectory(prefix='nexora-source-player-') as temporary:
        root = Path(temporary)
        source = root/'source Unicode 遊戲'
        source.mkdir()
        fixture = 'SpriteSourceRepackPlayer.zig' if args.repack else 'SpriteSourcePlayer.zig'
        shutil.copy2(ROOT/'Tests/TwoD'/fixture, source/'game.zig')
        (source/'catalog_mode.zig').write_text('pub const enabled = ' + str(args.catalog).lower() + ';\n')
        for name, color in [('red', bytes([255, 0, 0, 255])), ('blue', bytes([0, 0, 255, 255]))]:
            atlas = struct.pack('<IIQQIIIII', 0x4132444e, 1, 8, 9, 4, 4, 1, 1, 1)
            atlas += struct.pack('<IIIII', 1, 1, 1, 2, 2) + struct.pack('<I', 64) + color * 16
            (source/(name+'.nx2atlas')).write_bytes(atlas)
        if args.repack:
            for name, size, origin, palette in (
                    ('red', 4, (1, 1), [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0)]),
                    ('blue', 8, (4, 3), [(0, 255, 255), (255, 0, 255), (255, 255, 255), (0, 0, 0)])):
                texels = bytearray(bytes([31, 47, 63, 255]) * (size * size))
                for index, color in enumerate(palette):
                    x, y = origin[0] + index % 2, origin[1] + index // 2
                    texels[(y*size+x)*4:(y*size+x+1)*4] = bytes((*color, 255))
                atlas = struct.pack('<IIQQIIIII', 0x4132444e, 1, 8, 9, size, size, 1, 1, 1)
                atlas += struct.pack('<IIIII', 1, *origin, 2, 2) + struct.pack('<I', len(texels)) + texels
                (source/(name+'.nx2atlas')).write_bytes(atlas)
        for name, pivot in [('source', 3), ('shifted', 4)]:
            payload = struct.pack('<IIQQQQ7I3f4I', 0x5032444e, 1, 11, 12, 8, 9,
                                  1, 6, 6, 1, 1, 2, 2, pivot, 1, 1, 0, 0, 0, 0)
            assert len(payload) == 96
            (source/(name+'.nx2sprite')).write_bytes(payload)
        assets = []
        if args.catalog:
            for name, kind in [('red', 'atlas'), ('source', 'sprite')]:
                run(args.compiler, 'cook', name+'.nx2'+kind, name+'.nxcook', cwd=source,
                    log=output/(name+'-cook.log'))
                # The catalog game cannot embed its initial source/Atlas; only replacements stay embedded.
                (source/(name+'.nx2'+kind)).unlink()
            # Player must admit the Atlas first, independent of descriptor order.
            assets = [dict(kind='sprite', path='source.nxcook'), dict(kind='atlas', path='red.nxcook')]
        (source/'project.json').write_text(json.dumps(dict(schema=1, module_name='SourceFixture', script='game.zig')))
        (source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.25)\nproject(SourceFixture NONE)\n'
            f'include("{ROOT.as_posix()}/CMake/Modules/NexoraGameProject.cmake")\n'
            'nexora_add_game_module(game PROJECT "${CMAKE_SOURCE_DIR}/project.json" OUTPUT "${CMAKE_BINARY_DIR}/module" ZIG "${ZIG}")\n')
        run(args.cmake, '-S', source, '-B', root/'build', '-DZIG='+args.zig,
            log=output/'sdk-configure.log')
        run(args.cmake, '--build', root/'build', '--target', 'game', log=output/'sdk-build.log')
        suffix = '.dll' if sys.platform == 'win32' else '.dylib' if sys.platform == 'darwin' else '.so'
        modules = list((root/'build/module').rglob('*SourceFixture'+suffix))
        assert len(modules) == 1, modules
        shutil.copy2(modules[0], source/('module'+suffix))
        (source/'game.json').write_text(json.dumps(dict(schema=1, id='com.nexora.source-fixture',
            title='Sprite Source Fixture', scene='Main', module='module'+suffix, audio='off', assets=assets,
            display=dict(width=320, height=240, mode='windowed', resizable=False))))
        run(sys.executable, ROOT/'Tools/TwoD/ExportGame.py', source/'game.json', '--runtime', args.app,
            '--output', root/'export', *([] if args.shipping else ['--allow-development']),
            log=output/'export.log')
        package = root/'relocated'/'runtime only 遊戲'
        package.parent.mkdir()
        shutil.move(root/'export', package)
        shutil.rmtree(source)
        before = {str(p.relative_to(package)): hashlib.sha256(p.read_bytes()).hexdigest()
                  for p in package.rglob('*') if p.is_file()}
        (output/'package.sha256.json').write_text(json.dumps(before, indent=2, sort_keys=True)+'\n')
        shutil.copy2(package/'inventory.json', output/'export-inventory.json')
        assert json.loads((package/'inventory.json').read_text())['development_test_copy'] == (not args.shipping)
        if args.catalog:
            manifest = (package/'game.nx2game').read_text()
            assert 'sprite "assets/0000.nxcook"' in manifest and 'atlas "assets/0001.nxcook"' in manifest
            cooked = package/'assets/0000.nxcook'
            original = cooked.read_bytes()
            cooked.write_bytes(original[:-1]+bytes([original[-1] ^ 1]))
            rejected = subprocess.run([str(package/args.app.name), '--run-2d-project',
                str(package/'game.nx2game'), '--frames', '30'], cwd=output,
                capture_output=True, text=True, encoding='utf-8', timeout=30)
            (output/'corrupt-catalog.log').write_text(rejected.stdout+rejected.stderr)
            assert rejected.returncode and 'project cooked hash/type/identity' in rejected.stderr
            assert not rejected.stdout and 'SOURCE PLAYER START' not in rejected.stderr
            cooked.write_bytes(original)
        result = run(package/args.app.name, '--run-2d-project', package/'game.nx2game', '--frames', '30',
                     '--report', output/'headless.json', cwd=output, log=output/'headless.log')
        (output/'headless.stderr.log').write_text(result.stderr)
        report = json.loads((output/'headless.json').read_text())
        assert report['status'] == 'PASS' and report['allocations_balanced'] and report['visible'] == 2
        assert 'SOURCE PLAYER PASS' in result.stderr and 'SOURCE PLAYER FAIL' not in result.stderr
        if args.catalog:
            assert 'SOURCE PLAYER FILE CATALOG' in result.stderr
        if args.repack:
            assert 'SOURCE PLAYER REPACK atlas=8:9 source=11:12 stable handles' in result.stderr
        if args.native:
            native(package, args.app, output, args.repack)
        after = {str(p.relative_to(package)): hashlib.sha256(p.read_bytes()).hexdigest()
                 for p in package.rglob('*') if p.is_file()}
        assert before == after, 'Player changed the runtime-only package'
    print('PASS real Zig source Sprite/pool/registry, replacement and relocated Player; '
          + ('file catalog; ' if args.catalog else 'module-embedded initial content; ')
          + ('asymmetric flip/repack; ' if args.repack else '')
          + ('native pixels passed' if args.native else 'native pixels not selected')
          + '; evidence=' + str(output))


if __name__ == '__main__':
    main()
