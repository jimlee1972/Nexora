#!/usr/bin/env python3
"""Actual project Editor 2D Game canvas, native pixels and Play lifetime acceptance."""
from collections import Counter
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shutil
import struct
import subprocess
import tempfile
import time
import zlib
from LinuxDisplayAcceptance import request_window_close, start_xvfb, wait_for_window
from LinuxNativeScenePreview import scene_region_pixels

CYCLES = 32
OUTPUT_LIMIT = 4 * 1024 * 1024


def png(path, rect, pixels):
    _, _, width, height = rect
    width, height = min(width, 180), min(height, 200)
    # The shared pixel reader samples a central 180x200 rectangle, clipped by the canvas.
    if len(pixels) != width * height * 3:
        return
    def chunk(kind, data):
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))
    scanlines = b''.join(b'\0' + bytes(pixels[y * width * 3:(y + 1) * width * 3])
                         for y in range(height))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', width, height, 8, 2, 0, 0, 0)) +
                     chunk(b'IDAT', zlib.compress(scanlines)) + chunk(b'IEND', b''))


def srgb(color):
    def channel(value):
        linear = value / 255
        encoded = 12.92 * linear if linear <= .0031308 else 1.055 * linear ** (1 / 2.4) - .055
        return round(encoded * 255)
    return tuple(map(channel, color))


def close(actual, expected):
    return all(abs(a - b) <= 2 for a, b in zip(actual, expected))


def encoding(pixels):
    dominant = Counter(zip(pixels[::3], pixels[1::3], pixels[2::3])).most_common(1)[0][0]
    if close(dominant, (10, 20, 41)):
        return 'UNORM'
    if close(dominant, srgb((10, 20, 41))):
        return 'sRGB'
    return 'unavailable'


def colors(pixels):
    # UI colors and blending are linear; the swapchain can encode sRGB at its attachment.
    # Choose one coherent expected palette from the dominant known canvas background.
    mode = encoding(pixels)
    if mode == 'unavailable':
        return dict(base=0, blended=0, pool=0)
    convert = srgb if mode == 'sRGB' else lambda value: value
    base = [convert(c) for c in ((255, 0, 0), (0, 255, 0))]
    blended = [convert(c) for c in ((127, 0, 128), (0, 127, 128))]
    pool = convert((5, 10, 148))
    result = dict(base=0, blended=0, pool=0)
    for pixel in zip(pixels[::3], pixels[1::3], pixels[2::3]):
        result['base'] += any(close(pixel, c) for c in base)
        result['blended'] += any(close(pixel, c) for c in blended)
        result['pool'] += close(pixel, pool)
    return result


def base_region(pixels):
    red = sum(r > 230 and g < 25 and b < 25
              for r, g, b in zip(pixels[::3], pixels[1::3], pixels[2::3]))
    green = sum(g > 230 and r < 25 and b < 25
                for r, g, b in zip(pixels[::3], pixels[1::3], pixels[2::3]))
    return 'red' if red > green else 'green'


def coverage(pixels):
    return bytes(int((r > 230 or g > 230) and b < 25)
                 for r, g, b in zip(pixels[::3], pixels[1::3], pixels[2::3]))


def main():
    parser = argparse.ArgumentParser()
    for key in ('editor', 'fixture', 'module', 'xvfb', 'xdotool', 'evidence-dir'):
        parser.add_argument('--' + key, required=True)
    args = parser.parse_args()
    evidence = Path(args.evidence_dir)
    evidence.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='nexora-game-2d-'))
    state = Path(tempfile.mkdtemp(prefix='nexora-game-2d-state-'))
    subprocess.run([args.fixture, '--write-fixture', str(root)], check=True)
    module = Path(args.module).name
    shutil.copy2(args.module, root / 'Content' / module)
    layout = 'schema=1\n'
    for name, panel in [('Project', 'project'), ('Hierarchy', 'hierarchy'), ('Inspector', 'inspector'),
                        ('Console', 'console'), ('Content', 'content'), ('Scene', 'scene'),
                        ('Game', 'game'), ('Profiler', 'profiler'), ('Build', 'build')]:
        layout += f'[Window][{name}###nexora.{panel}]\n'
        layout += ('Pos=640,40\nSize=620,640\nCollapsed=0\n\n' if panel == 'game' else
                   'Pos=0,40\nSize=620,640\nCollapsed=0\n\n' if panel == 'scene' else
                   'Pos=0,700\nSize=100,20\nCollapsed=1\n\n')
    (root / '.nexora/editor-layout.ini').write_text(layout)
    server, display = start_xvfb(args.xvfb, '1280x720x24')
    editor = None
    captured = bytearray()
    stdout = tempfile.TemporaryFile()
    started = time.monotonic()
    cycle = 0
    try:
        if display is None:
            raise RuntimeError('Xvfb did not start')
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(state), NEXORA_GAME_TWOD_UI_TEST_EVIDENCE='1')
        editor = subprocess.Popen([args.editor, f'--project={root}', '--graphical',
            '--native-scene-preview', '--frames=100000', f'--recent-projects={state / "recent"}',
            f'--gameplay-library=Content/{module}'], env=env, stdout=stdout, stderr=subprocess.PIPE)
        window = int(wait_for_window(args.xdotool, env))

        def pump(duration=.02):
            end = time.monotonic() + duration
            while time.monotonic() < end:
                if not select.select([editor.stderr], [], [], max(0, end - time.monotonic()))[0]:
                    break
                data = os.read(editor.stderr.fileno(), 65536)
                if not data:
                    break
                captured.extend(data)
                if len(captured) > OUTPUT_LIMIT:
                    raise RuntimeError('2D acceptance exceeded its output budget')
            if editor.poll() is not None:
                raise RuntimeError(f'Editor exited early: {editor.returncode}')
            if b'2d fixture stop live=0' in captured or b'2d fixture destroy live=0' in captured:
                raise RuntimeError('2D services expired before module teardown')

        def wait(condition, message, timeout=8):
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                pump(.05)
                if condition():
                    return
            raise RuntimeError(message)

        def send(*command):
            subprocess.run([args.xdotool, *map(str, command)], env=env, check=True)
            pump(.05)

        def observation():
            lines = re.findall(rb'game 2d ui [^\r\n]+', captured)
            return lines[-1] if lines else b''

        def value(name):
            found = re.search(name.encode() + rb'=([0-9.,]+)', observation())
            return tuple(map(float, found[1].split(b','))) if found else None

        def click(point):
            send('mousemove', '--window', window, int(point[0]), int(point[1]))
            send('click', 1)
            pump(.1)

        def rect():
            current = value('viewport')
            if not current:
                raise RuntimeError('2D canvas is absent')
            return tuple(map(int, current))

        def pixels():
            return scene_region_pixels(display, window, rect(), stride=1)

        def source_pixels():
            views = re.findall(rb'native scene viewport: (\d+) (\d+) (\d+) (\d+)', captured)
            return scene_region_pixels(display, window, tuple(map(int, views[-1])))

        wait(lambda: b'native scene viewport:' in captured, 'Source 3D Scene did not render', 15)
        pump(.2)
        baseline = {p: p.read_bytes() for p in (root / 'Content').rglob('*') if p.is_file()}
        baseline[root / '.nexora/scenes/Main.scene'] = (root / '.nexora/scenes/Main.scene').read_bytes()
        source_baseline = source_pixels()
        send('windowfocus', window)
        phases = []
        for cycle in range(1, CYCLES + 1):
            send('key', '--delay', '30', 'F5')
            wait(lambda: captured.count(b'2d fixture start\n') == cycle, 'Module did not start')
            wait(lambda: value('combo') is not None, 'View selector did not appear')
            if value('viewport') is not None:
                raise RuntimeError('New Play session did not default to 3D')
            click(value('combo'))
            wait(lambda: value('two') is not None, '2D option did not open')
            click(value('two'))
            wait(lambda: value('viewport') is not None and value('textures') == (2,),
                 'Two frozen Atlases did not reach the 2D Game canvas')
            send('key', '--delay', '30', 'F6')
            wait(lambda: value('state') == (2,), 'Play did not pause')
            wait(lambda: all(v > 50 for v in colors(pixels()).values()),
                 'Actual native multi-Atlas/alpha/pool pixels are absent')
            paused = pixels()
            pump(.15)
            if pixels() != paused:
                raise RuntimeError('Paused canvas advanced without a tick')
            if cycle == 1:
                phases.append(dict(colors(paused), region=base_region(paused)))
                png(evidence / 'paused.png', rect(), paused)
                old_steps = int(value('steps')[0])
                send('key', '--delay', '30', 'F10')
                wait(lambda: value('steps') == (old_steps + 1,), 'Manual Step was not observed')
                wait(lambda: base_region(pixels()) != base_region(paused),
                     'Manual Step did not change the frozen Clip region')
                stepped = pixels()
                phases.append(dict(colors(stepped), region=base_region(stepped)))
                png(evidence / 'stepped.png', rect(), stepped)
                pump(.15)
                if pixels() != stepped:
                    raise RuntimeError('Rendering advanced a paused animation')
                send('key', '--delay', '30', 'F6')
                wait(lambda: value('state') == (1,), 'Play did not resume')
                x, y, w, h = rect()
                click((x + w // 2, y + h // 2))
                pump(.1)
                send('mousemove', '--window', window, x + w // 2 + 1, y + h // 2 + 1)
                pump(.1)
                send('keydown', 'd')
                wait(lambda: b'2d fixture move x=' in captured, '2D canvas did not capture input')
                pump(.3)
                send('keyup', 'd')
                send('key', '--delay', '30', 'F6')
                wait(lambda: value('state') == (2,), 'Movement did not pause')
                wait(lambda: coverage(pixels()) != coverage(stepped), 'Input did not move Sprite geometry')
            if source_pixels() != source_baseline:
                raise RuntimeError('2D Play changed concurrent source 3D pixels')
            send('key', '--delay', '30', 'F5')
            wait(lambda: value('state') == (0,) and value('textures') == (0,),
                 'Stop did not clear the 2D preview registrations')
            if captured.count(b'2d fixture stop live=1') != cycle or captured.count(b'2d fixture destroy live=1') != cycle:
                raise RuntimeError('2D module lifetime order/teardown is unbalanced')
            if source_pixels() != source_baseline or any(p.read_bytes() != data for p, data in baseline.items()):
                raise RuntimeError('Play changed authoring source pixels or bytes')
        request_window_close(str(window), env)
        _, tail = editor.communicate(timeout=15)
        captured.extend(tail)
        stdout.seek(0)
        output = stdout.read(OUTPUT_LIMIT + 1)
        if editor.returncode or len(output) > OUTPUT_LIMIT:
            raise RuntimeError('Editor shutdown or stdout budget failed')
        if re.search(rb'Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+', captured + output):
            raise RuntimeError('Native Vulkan validation failed')
        report = dict(cycles=CYCLES, native_game_pixels='passed', alpha_pool_atlases=2,
            actual_view_click='passed', actual_view_clicks=CYCLES, session_defaults_3d=CYCLES,
            pause_step_input_stop='passed', source_pixels='unchanged',
            source_bytes='unchanged', validation_errors=0, phases=phases, swapchain_encoding=encoding(paused),
            elapsed_seconds=round(time.monotonic() - started, 3))
        (evidence / 'acceptance.json').write_text(json.dumps(report, indent=2) + '\n')
        print('Main Game 2D native evidence: ' + json.dumps(report, sort_keys=True))
        return 0
    except Exception as error:
        try:
            if editor is not None and editor.poll() is None and value('viewport'):
                sample = pixels()
                png(evidence / 'failure.png', rect(), sample)
                common = Counter(zip(sample[::3], sample[1::3], sample[2::3])).most_common(12)
                print('2D failure pixels:', colors(sample), common, flush=True)
        except Exception:
            pass
        raise RuntimeError(f'Main Game 2D cycle {cycle}: {error}; tail={bytes(captured[-8192:])!r}') from error
    finally:
        (evidence / 'editor.log').write_bytes(captured)
        if editor is not None and editor.poll() is None:
            editor.kill()
            editor.communicate(timeout=5)
        server.terminate()
        server.communicate(timeout=5)
        stdout.close()
        shutil.rmtree(root, ignore_errors=True)
        shutil.rmtree(state, ignore_errors=True)


if __name__ == '__main__':
    raise SystemExit(main())
