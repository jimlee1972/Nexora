#!/usr/bin/env python3
"""Real main-scene module-free Play, immutable source pixels and owner teardown."""
import argparse
import ctypes
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

from LinuxDisplayAcceptance import (physical_key_arguments, request_window_close,
                                    start_xvfb, wait_for_window)
from LinuxNativeScenePreview import XImage, channel, scene_region_pixels

CYCLES = 32
OUTPUT_LIMIT = 4 * 1024 * 1024


def screenshot(display_name, window, path):
    """Retain the full native window, including both canvases and product controls."""
    x11 = ctypes.CDLL('libX11.so.6')
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                              ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]
    x11.XGetImage.restype = ctypes.POINTER(XImage)
    x11.XGetPixel.argtypes = [ctypes.POINTER(XImage), ctypes.c_int, ctypes.c_int]
    x11.XGetPixel.restype = ctypes.c_ulong
    x11.XDestroyImage.argtypes = [ctypes.POINTER(XImage)]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(display_name.encode())
    if not display:
        raise RuntimeError('Native screenshot display unavailable')
    try:
        image = x11.XGetImage(display, window, 0, 0, 1280, 720,
                              ctypes.c_ulong(-1).value, 2)
        if not image:
            raise RuntimeError('Native screenshot readback failed')
        try:
            rows = bytearray()
            for y in range(720):
                rows.append(0)
                for x in range(1280):
                    pixel = x11.XGetPixel(image, x, y)
                    rows.extend(channel(pixel, mask) for mask in
                                (image.contents.red_mask, image.contents.green_mask,
                                 image.contents.blue_mask))
            def chunk(kind, data):
                return (struct.pack('!I', len(data)) + kind + data +
                        struct.pack('!I', zlib.crc32(kind + data)))
            path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack(
                '!2I5B', 1280, 720, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(rows)) +
                chunk(b'IEND', b''))
        finally:
            x11.XDestroyImage(image)
    finally:
        x11.XCloseDisplay(display)


def source_mask(pixels):
    return bytes(1 if r > 250 and g < 3 and b < 3 else
                 2 if b > 250 and r < 3 and g < 3 else 0
                 for r, g, b in zip(pixels[::3], pixels[1::3], pixels[2::3]))


def main():
    parser = argparse.ArgumentParser()
    for key in ('editor', 'fixture', 'xvfb', 'xdotool', 'evidence-dir'):
        parser.add_argument('--' + key, required=True)
    args = parser.parse_args()
    evidence = Path(args.evidence_dir) / ('attempt-' + str(time.monotonic_ns()))
    evidence.mkdir(parents=True)
    root = Path(tempfile.mkdtemp(prefix='nexora-authored-preview-'))
    state = Path(tempfile.mkdtemp(prefix='nexora-authored-preview-state-'))
    subprocess.run([args.fixture, '--write-authored-fixture', str(root)], check=True)
    if (root / 'project.build.json').exists() or any(
            p.suffix in ('.so', '.dll', '.dylib') for p in root.rglob('*')):
        raise RuntimeError('Preview fixture unexpectedly contains a package configuration or module')
    layout = 'schema=1\n'
    for name, panel in [('Project', 'project'), ('Hierarchy', 'hierarchy'),
                        ('Inspector', 'inspector'), ('Console', 'console'),
                        ('Content', 'content'), ('Scene', 'scene'), ('Game', 'game'),
                        ('Profiler', 'profiler'), ('Build', 'build')]:
        layout += f'[Window][{name}###nexora.{panel}]\n'
        layout += ('Pos=640,40\nSize=620,640\nCollapsed=0\n\n' if panel == 'game' else
                   'Pos=0,40\nSize=620,640\nCollapsed=0\n\n' if panel == 'scene' else
                   'Pos=0,700\nSize=100,20\nCollapsed=1\n\n')
    (root / '.nexora/editor-layout.ini').write_text(layout)
    server, display = start_xvfb(args.xvfb, '1280x720x24')
    editor = None
    window = None
    captured = bytearray()
    stdout = tempfile.TemporaryFile()
    started = time.monotonic()
    cycle = 0
    try:
        if display is None:
            raise RuntimeError('Xvfb did not start')
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(state),
                   NEXORA_GAME_TWOD_UI_TEST_EVIDENCE='1')
        editor = subprocess.Popen([args.editor, f'--project={root}', '--graphical',
            '--native-scene-preview', '--frames=100000',
            f'--recent-projects={state / "recent"}'],
            env=env, stdout=stdout, stderr=subprocess.PIPE)
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
                    raise RuntimeError('Preview output exceeded its budget')
            if editor.poll() is not None:
                raise RuntimeError(f'Editor exited early: {editor.returncode}')

        def wait(condition, message, timeout=8):
            end = time.monotonic() + timeout
            while time.monotonic() < end:
                pump(.05)
                if condition():
                    return
            raise RuntimeError(message)

        def send(*command):
            subprocess.run([args.xdotool, *map(str, physical_key_arguments(command, env))],
                           env=env, check=True)
            pump(.05)

        def value(name):
            lines = re.findall(rb'game 2d ui [^\r\n]+', captured)
            match = re.search(name.encode() + rb'=([0-9.,]+)', lines[-1]) if lines else None
            return tuple(map(float, match[1].split(b','))) if match else None

        def click(point):
            send('mousemove', '--window', window, int(point[0]), int(point[1]))
            send('click', 1)
            pump(.1)

        def game_pixels():
            return scene_region_pixels(display, window, tuple(map(int, value('viewport'))), stride=1)

        def scene_pixels():
            views = re.findall(rb'native scene viewport: (\d+) (\d+) (\d+) (\d+)', captured)
            return scene_region_pixels(display, window, tuple(map(int, views[-1])))

        wait(lambda: b'native scene viewport:' in captured, 'Native authoring Scene missing', 15)
        pump(.25)
        baseline = {p: p.read_bytes() for p in (root / 'Content').rglob('*') if p.is_file()}
        scene_path = root / '.nexora/scenes/Main.scene'
        baseline[scene_path] = scene_path.read_bytes()
        original_scene = scene_pixels()
        send('windowfocus', window)
        masks = []
        for cycle in range(1, CYCLES + 1):
            send('key', '--delay', '30', 'F5')
            wait(lambda: value('state') == (1,) and value('module') == (0,) and
                 value('bound') == (1,) and value('combo') is not None,
                 'No-module Play did not bind its authored preview')
            if value('viewport') is not None:
                raise RuntimeError('A new preview session did not default to 3D')
            click(value('combo'))
            wait(lambda: value('two') is not None, '2D selector did not open')
            click(value('two'))
            wait(lambda: value('viewport') is not None and value('textures') == (2,),
                 'Authored source Atlases did not reach the Game canvas')
            send('key', '--delay', '30', 'F6')
            wait(lambda: value('state') == (2,), 'Preview did not pause')
            wait(lambda: source_mask(game_pixels()).count(1) > 200 and
                 source_mask(game_pixels()).count(2) > 200,
                 'Actual native red/blue authored source pixels missing')
            mask = source_mask(game_pixels())
            if masks and mask != masks[0]:
                raise RuntimeError('Restart changed immutable source geometry')
            masks.append(mask)
            if cycle == 1:
                screenshot(display, window, evidence / 'paused.png')
                steps = value('steps')[0]
                send('key', '--delay', '30', 'F10')
                wait(lambda: value('steps') == (steps + 1,), 'Paused manual Step missing')
                pump(.15)
                if source_mask(game_pixels()) != mask:
                    raise RuntimeError('No-module Step changed static source geometry')
                screenshot(display, window, evidence / 'stepped.png')
                send('key', '--delay', '30', 'F6')
                wait(lambda: value('state') == (1,), 'Preview did not resume')
            if scene_pixels() != original_scene:
                raise RuntimeError('Preview changed concurrent authoring Scene pixels')
            send('key', '--delay', '30', 'F5')
            wait(lambda: value('state') == (0,) and value('textures') == (0,) and
                 value('bound') == (0,) and value('viewport') is None,
                 'Stop retained a preview owner or native Atlas registrations')
            if scene_pixels() != original_scene or any(p.read_bytes() != b for p, b in baseline.items()):
                raise RuntimeError('Preview changed authoring pixels or source bytes')
        screenshot(display, window, evidence / 'stopped.png')
        request_window_close(str(window), env)
        _, tail = editor.communicate(timeout=15)
        captured.extend(tail)
        stdout.seek(0)
        output = stdout.read(OUTPUT_LIMIT + 1)
        if editor.returncode or len(output) > OUTPUT_LIMIT:
            raise RuntimeError('Editor shutdown/output budget failed')
        if re.search(rb'Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+', captured + output):
            raise RuntimeError('Vulkan validation failed')
        report = dict(cycles=CYCLES, module_loaded=False, native_source_pixels='passed',
                      red_pixels=masks[0].count(1), blue_pixels=masks[0].count(2),
                      source_mask_sha256=hashlib.sha256(masks[0]).hexdigest(),
                      pause_step_resume_stop='passed', exact_restart_geometry=True,
                      authoring_pixels='unchanged', source_bytes='unchanged',
                      validation_errors=0, evidence=str(evidence),
                      elapsed_seconds=round(time.monotonic() - started, 3))
        (evidence / 'acceptance.json').write_text(json.dumps(report, indent=2) + '\n')
        print('Module-free source Play evidence: ' + json.dumps(report, sort_keys=True))
        return 0
    except Exception as error:
        if editor is not None and editor.poll() is None and window is not None:
            try:
                screenshot(display, window, evidence / 'failure.png')
            except Exception as capture_error:
                (evidence / 'screenshot-error.log').write_text(str(capture_error))
        raise RuntimeError(f'Authored preview cycle {cycle}: {error}; evidence={evidence}; '
                           f'tail={bytes(captured[-8192:])!r}') from error
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
