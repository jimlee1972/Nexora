#!/usr/bin/env python3
"""Actual configured Editor: one-pass mixed pixels, readonly32-cycle Play and SDK scene changes."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import (physical_key_arguments, request_window_close,
                                    start_xvfb, wait_for_window)
from LinuxNativeScenePreview import XImage, channel, scene_region_pixels
from LinuxAuthoredPreview import screenshot


def require(value, message):
    if not value:
        raise RuntimeError(message)


def hashes(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(root.rglob('*')) if p.is_file()}


def source_mask(rgb):
    return bytes(1 if r > 252 and g < 3 and b < 3 else
                 2 if g > 252 and r < 3 and b < 3 else
                 3 if b > 252 and r < 3 and g < 3 else
                 4 if r > 252 and g > 252 and b < 3 else 0
                 for r, g, b in zip(rgb[::3], rgb[1::3], rgb[2::3]))


def native_pixels(display_name, window, viewport):
    """Read the complete physical canvas; the older Scene helper samples only its center."""
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
    require(display, 'native canvas display unavailable')
    image = None
    try:
        x, y, width, height = viewport
        image = x11.XGetImage(display, window, x, y, width, height,
                              ctypes.c_ulong(-1).value, 2)
        require(image, 'native canvas readback failed')
        rgb = bytearray()
        for y in range(height):
            for x in range(width):
                pixel = x11.XGetPixel(image, x, y)
                rgb.extend(channel(pixel, mask) for mask in
                           (image.contents.red_mask, image.contents.green_mask,
                            image.contents.blue_mask))
        return bytes(rgb)
    finally:
        if image:
            x11.XDestroyImage(image)
        x11.XCloseDisplay(display)


def expected_mask(width, height, positions):
    # Independent fixture oracle: source4x4, trim[1,1,2,2], Pivot[2,3], PPU1,
    # camera vertical20; the positive-X entity has a negative X scale.
    require(height == 180 and width % 2 == 0, 'unexpected native canvas geometry')
    unit = height // 20
    mask = bytearray(width * height)
    for position in positions:
        left = width // 2 + position * unit - unit
        top = height // 2
        for y in range(unit * 2):
            for x in range(unit * 2):
                column = x // unit
                if position > 0:
                    column = 1 - column
                mask[(top + y) * width + left + x] = 1 + (y // unit) * 2 + column
    return bytes(mask)


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'module', 'xvfb', 'xdotool', 'evidence-dir'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    evidence = Path(args.evidence_dir) / ('configured-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    root = Path(tempfile.mkdtemp(prefix='nexora-configured-sprites-'))
    server = editor = window = None
    proof = {'passed': False, 'cycles': 32, 'readonly': True, 'captures': [], 'phases': [],
             'evidence_dir': str(evidence)}
    try:
        project = root / 'project 來源'
        subprocess.run([args.fixture, '--write-fixture', str(project)], check=True,
                       capture_output=True, timeout=20)
        (project / '.nexora/scenes').mkdir(parents=True, exist_ok=True)
        shutil.copyfile(project / 'Title.scene', project / '.nexora/scenes/Main.scene')
        settings = json.loads((project / 'project.build.json').read_text())
        for index, entry in enumerate(settings['scenes'], 1):
            entry['id'] = f'00000000-0000-0001-0000-{index:012x}'
        settings['start_scene'] = settings['scenes'][1]['id']
        (project / 'project.build.json').write_text(json.dumps(settings))
        # Title differs visibly, so additive/replace must use the remapped candidate's poses.
        title = project / 'Title.scene'
        text = title.read_text()
        text = re.sub(r'(?m)^(2 0 )-4 ', r'\g<1>-6 ', text)
        text = re.sub(r'(?m)^(3 0 )4 ', r'\g<1>6 ', text)
        require(text != title.read_text(), 'Title source positions were not changed')
        title.write_text(text)
        layout = 'schema=1\n'
        for name, panel in [('Scene', 'scene'), ('Game', 'game'), ('Project', 'project'),
                            ('Hierarchy', 'hierarchy'), ('Inspector', 'inspector'),
                            ('Content', 'content'), ('Console', 'console'),
                            ('Profiler', 'profiler'), ('Build', 'build')]:
            layout += f'[Window][{name}###nexora.{panel}]\n'
            layout += ('Pos=640,40\nSize=620,640\nCollapsed=0\n' if panel == 'game' else
                       'Pos=0,40\nSize=620,640\nCollapsed=0\n' if panel == 'scene' else
                       'Pos=0,700\nSize=100,20\nCollapsed=1\n') + '\n'
        (project / '.nexora/editor-layout.ini').write_text(layout)
        module = project / Path(args.module).name
        shutil.copyfile(args.module, module)
        legacy = root / 'legacy'
        shutil.copytree(project, legacy)
        for scene in [legacy / 'Title.scene', legacy / 'Level.scene',
                      legacy / '.nexora/scenes/Main.scene']:
            scene.write_bytes(b''.join(line for line in scene.read_bytes().splitlines(keepends=True)
                                       if b'"editor.sprite.source"' not in line))
        originals = hashes(project)
        server, display = start_xvfb(args.xvfb, '1280x720x24')
        require(display is not None, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display,
                   NEXORA_PACKAGED_SPRITE_UI_TEST_EVIDENCE='1', XDG_STATE_HOME=str(root / 'state'))

        def launch(path, label, automatic=False, gameplay=False):
            nonlocal editor
            log = evidence / (label + '.log')
            out = evidence / (label + '.stdout.log')
            command = [args.editor, '--graphical', '--native-scene-preview', '--read-only',
                       '--frames=100000', '--project=' + str(path),
                       '--recent-projects=' + str(root / (label + '-recent'))]
            if automatic:
                command.append('--play')
            if gameplay:
                command.append('--gameplay-library=' + module.name)
            with log.open('wb') as err, out.open('wb') as stdout:
                editor = subprocess.Popen(command,
                    env=dict(env, NEXORA_LOG_DIR=str(evidence / (label + '-core'))),
                    stdout=stdout, stderr=err)
            window = int(wait_for_window(args.xdotool, env))

            def observation():
                data = log.read_bytes()
                require(len(data) < 4 * 1024 * 1024, 'native evidence output exceeded4MiB')
                lines = re.findall(rb'^packaged sprite ui (.*)$', data, re.M)
                return dict(part.decode().split('=', 1) for part in lines[-1].split()) if lines else {}

            def wait(predicate, reason, timeout=10):
                until = time.monotonic() + timeout
                while time.monotonic() < until:
                    require(editor.poll() is None, 'Editor exited: ' + log.read_text()[-4096:])
                    current = observation()
                    if predicate(current):
                        return current
                    time.sleep(.02)
                raise RuntimeError(reason + ': ' + log.read_text()[-4096:])

            def key(name):
                subprocess.run([args.xdotool, *physical_key_arguments(
                    ('key', '--delay', '30', name), env)], env=env, check=True, timeout=3)

            subprocess.run([args.xdotool, 'windowfocus', str(window)], env=env,
                           check=True, timeout=3)
            wait(lambda s: s.get('focused') == '1' and s.get('admitted') == '1' and
                 b'native scene viewport:' in log.read_bytes(), 'processed native input readiness')
            return log, window, observation, wait, key

        def close(log, window):
            request_window_close(str(window), env)
            editor.wait(timeout=15)
            require(editor.returncode == 0, 'native shutdown failed: ' + log.read_text()[-4096:])
            text = log.read_text()
            require(not re.search(r'Validation Error|VUID-|SYNC-HAZARD-', text), 'Vulkan validation')
            require(re.search(r'graphical evidence: .*ui_rejected=0 ', text), 'stale native UI textures')

        log, window, observation, wait, key = launch(legacy, 'legacy', automatic=True)
        state = wait(lambda s: s.get('state') == '1' and 'viewport' in s, 'legacy canvas missing')
        key('F6')
        wait(lambda s: s.get('state') == '2', 'legacy Pause')
        viewport = tuple(map(int, state['viewport'].split(',')))
        time.sleep(.15)
        baseline = native_pixels(display, window, viewport)
        screenshot(display, window, evidence / 'legacy.png')
        close(log, window)
        log, window, observation, wait, key = launch(project, 'cycles')
        original_scene = None
        for cycle in range(1, 33):
            key('F5')
            state = wait(lambda s: s.get('state') == '1' and s.get('textures') == '1' and
                         s.get('visible') == '2' and 'viewport' in s, 'source startup')
            key('F6')
            state = wait(lambda s: s.get('state') == '2', 'source Pause')
            actual_viewport = tuple(map(int, state['viewport'].split(',')))
            require(actual_viewport == viewport, 'legacy/source canvas changed')
            wanted = expected_mask(viewport[2], viewport[3], [-4, 4])
            wait(lambda _: source_mask(native_pixels(display, window, viewport)) == wanted,
                 'native crop/Pivot/mirror pixels missing')
            actual = native_pixels(display, window, viewport)
            width, height = viewport[2:]
            # Strict interior3D comparison against an independently running source-free Editor.
            center = [(y * width + x) * 3 for y in range(height // 2 + 4, height // 2 + 10)
                      for x in range(width // 2 - 3, width // 2 + 3)]
            require(all(actual[i:i + 3] == baseline[i:i + 3] for i in center),
                    '3D pixels changed under source overlay')
            require(len(set(baseline[i:i + 3] for i in center)) > 0 and
                    baseline[center[0]:center[0] + 3] != baseline[:3], '3D baseline is background')
            view = re.findall(rb'native scene viewport: (\d+) (\d+) (\d+) (\d+)', log.read_bytes())[-1]
            authored = scene_region_pixels(display, window, tuple(map(int, view)))
            if original_scene is None:
                original_scene = authored
            require(authored == original_scene and hashes(project) == originals,
                    'Play changed readonly authoring pixels/files')
            if cycle == 1:
                screenshot(display, window, evidence / 'paused.png')
                steps = int(state['steps'])
                key('F10')
                wait(lambda s: int(s.get('steps', '0')) == steps + 1, 'manual Step missing')
                require(source_mask(native_pixels(display, window, viewport)) == wanted,
                        'Step changed static source geometry')
                screenshot(display, window, evidence / 'stepped.png')
                key('F6')
                wait(lambda s: s.get('state') == '1', 'Resume missing')
            key('F5')
            wait(lambda s: s.get('state') == '0' and s.get('textures') == '0' and
                 s.get('visible') == '0' and 'viewport' not in s, 'Stop retained native owner')
        screenshot(display, window, evidence / 'stopped.png')
        close(log, window)
        require(hashes(project) == originals, '32 sessions changed source files')
        proof['source_mask_sha256'] = hashlib.sha256(wanted).hexdigest()
        proof['source_color_pixels'] = [wanted.count(i) for i in range(1, 5)]
        proof['underlying_3d_exact_pixels'] = len(center)
        log, window, observation, wait, key = launch(project, 'sdk', automatic=True, gameplay=True)
        for commits, positions in ((1, [-4, 4]), (2, [-6, -4, 4, 6]),
                                    (3, [-6, 6]), (4, [-4, 4])):
            state = wait(lambda s: s.get('commits') == str(commits) and
                         s.get('module') == '1' and 'viewport' in s,
                         'SDK scene phase missing', timeout=15)
            require(state['visible'] == str(len(positions)), 'remapped source count mismatch')
            wanted = expected_mask(viewport[2], viewport[3], positions)
            wait(lambda s: s.get('commits') == str(commits) and source_mask(
                 native_pixels(display, window, viewport)) == wanted,
                 'SDK additive/replace pixels mismatch')
            screenshot(display, window, evidence / f'sdk-{commits}.png')
            proof['phases'].append({'commits': commits, 'visible': len(positions),
                                    'mask_sha256': hashlib.sha256(wanted).hexdigest()})
        key('F5')
        wait(lambda s: s.get('state') == '0' and s.get('textures') == '0', 'SDK Stop')
        close(log, window)
        text = '\n'.join(p.read_text() for p in (evidence / 'sdk-core').rglob('game*.log'))
        require(text.count('packaged sprite fixture applied') == 3 and
                'packaged sprite fixture ready' in text and 'packaged sprite fixture stopped' in text,
                'real Zig lifecycle did not complete')
        require(hashes(project) == originals, 'SDK source files changed')
        # A normal floating Inspector covers the Game image and sources in ImGui window order.
        covered = root / 'occlusion'
        shutil.copytree(project, covered)
        layout_path = covered / '.nexora/editor-layout.ini'
        layout_path.write_text(layout.replace(
            '[Window][Inspector###nexora.inspector]\nPos=0,700\nSize=100,20\nCollapsed=1',
            '[Window][Inspector###nexora.inspector]\nPos=890,340\nSize=250,200\nCollapsed=0'))
        covered_hashes = hashes(covered)
        log, window, observation, wait, key = launch(covered, 'occlusion', automatic=True)
        wait(lambda s: s.get('state') == '1' and s.get('visible') == '2' and 'viewport' in s,
             'occlusion Play startup')
        # Play focuses Game. Use the real Ctrl+3 panel command to bring Inspector above it.
        for command in (('keydown', 'ctrl'), ('key', '3'), ('keyup', 'ctrl')):
            subprocess.run([args.xdotool, *command], env=env, check=True, timeout=3)
            time.sleep(.1)
        wait(lambda _: not any(source_mask(native_pixels(display, window, viewport))),
             'floating Inspector did not occlude source pixels')
        screenshot(display, window, evidence / 'occluded.png')
        subprocess.run([args.xdotool, 'mousemove', '--window', str(window), '920', '350'],
                       env=env, check=True, timeout=3)
        subprocess.run([args.xdotool, 'click', '--repeat', '2', '--delay', '100', '1'],
                       env=env, check=True, timeout=3)
        wanted = expected_mask(viewport[2], viewport[3], [-4, 4])
        wait(lambda _: source_mask(native_pixels(display, window, viewport)) == wanted,
             'collapsed Inspector did not reveal immutable sources')
        screenshot(display, window, evidence / 'revealed.png')
        # Every resize needs a fresh surface/frame reference; no old Scene image may survive it.
        try:
            subprocess.run([args.xdotool, 'windowsize', str(window), '1200', '680'],
                           env=env, check=True, timeout=3)
            state = wait(lambda s: 'viewport' in s and s['viewport'] != ','.join(map(str, viewport)),
                         'resize did not reach native Game canvas')
            resized = tuple(map(int, state['viewport'].split(',')))
            resized_mask = expected_mask(resized[2], resized[3], [-4, 4])
            wait(lambda _: source_mask(native_pixels(display, window, resized)) == resized_mask,
                 'fresh resized Scene/Sprite image did not render')
            proof['resize'] = {'viewport': resized, 'mask_sha256': hashlib.sha256(resized_mask).hexdigest()}
        finally:
            subprocess.run([args.xdotool, 'windowsize', str(window), '1280', '720'],
                           env=env, check=True, timeout=3)
        wait(lambda s: s.get('viewport') == ','.join(map(str, viewport)) and
             source_mask(native_pixels(display, window, viewport)) == wanted,
             'restored viewport retained a stale Scene image')
        screenshot(display, window, evidence / 'restored.png')
        close(log, window)
        require(hashes(covered) == covered_hashes, 'occlusion/resize changed readonly source files')
        proof['native_window_occlusion'] = 'passed'
        # External source drift after project admission must reject Play before SDK callbacks.
        log, window, observation, wait, key = launch(project, 'stale', gameplay=True)
        source = project / 'Content/Source.nx2sprite'
        saved = source.read_bytes()
        try:
            source.write_bytes(saved + b'\x00')
            key('F5')

            def rejected(state):
                core = '\n'.join(p.read_text() for p in (evidence / 'stale-core').rglob('game*.log'))
                return (state.get('state') == '0' and state.get('commits') == '0' and
                        state.get('module') == '0' and state.get('textures') == '0' and
                        re.search(r'Error .*\[PIE\].*(?:[Ss]prite|[Ss]ource)', core))

            wait(rejected, 'drifted source was not rejected before publication/callbacks')
        finally:
            source.write_bytes(saved)
        close(log, window)
        core = '\n'.join(p.read_text() for p in (evidence / 'stale-core').rglob('game*.log'))
        require('packaged sprite fixture ready' not in core, 'failed capture invoked module Start')
        require(hashes(project) == originals, 'external source injection did not restore originals')
        proof['stale_capture_rejected_before_callbacks'] = True
        proof.update(passed=True, source_hashes=originals, validation_errors=0)
        proof['captures'] = [{'file': p.name, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                              for p in sorted(evidence.glob('*.png'))]
        print('Configured Sprite Play evidence: ' + json.dumps(proof, sort_keys=True))
    except Exception:
        if editor and editor.poll() is None and window is not None:
            try:
                screenshot(display, window, evidence / 'failure.png')
            except Exception as capture_error:
                (evidence / 'screenshot-error.log').write_text(str(capture_error))
        raise
    finally:
        if editor and editor.poll() is None:
            editor.kill()
            editor.wait(timeout=5)
        if server:
            server.terminate()
            server.wait(timeout=5)
        proof['captures'] = [{'file': p.name, 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}
                              for p in sorted(evidence.glob('*.png'))]
        (evidence / 'receipt.json').write_text(json.dumps(proof, indent=2) + '\n')
        shutil.rmtree(root, ignore_errors=True)


if __name__ == '__main__':
    main()
