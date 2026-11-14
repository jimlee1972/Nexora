#!/usr/bin/env python3
"""Actual authoring Scene: independent 3D projection, cutout picks and owning dual images."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import (physical_key_arguments, request_window_close,
                                    start_xvfb, wait_for_window)
from LinuxPackagedSprites import hashes, native_pixels, require
from LinuxAuthoredPreview import screenshot


def project(point, camera, viewport):
    """Independent right-handed perspective oracle, using authored world positions."""
    x, z, yaw, pitch, distance, target_y = camera
    forward = (-math.sin(yaw) * math.cos(pitch), -math.sin(pitch),
               -math.cos(yaw) * math.cos(pitch))
    side = (math.cos(yaw), 0, -math.sin(yaw))
    up = (-math.sin(yaw) * math.sin(pitch), math.cos(pitch),
          -math.cos(yaw) * math.sin(pitch))
    eye = (x - distance * forward[0], target_y - distance * forward[1],
           z - distance * forward[2])
    offset = [p - e for p, e in zip(point, eye)]
    dot = lambda a, b: sum(v * w for v, w in zip(a, b))
    depth = dot(offset, forward)
    require(depth > 0, 'fixture point lies behind camera')
    scale = viewport[3] / (2 * math.tan(.85 / 2) * depth)
    return (round(viewport[0] + viewport[2] / 2 + dot(offset, side) * scale),
            round(viewport[1] + viewport[3] / 2 - dot(offset, up) * scale))


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool', 'evidence-dir'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    evidence = Path(args.evidence_dir) / ('attempt-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    root = Path(tempfile.mkdtemp(prefix='nexora-native-scene-sprites-'))
    server = editor = window = log = None
    proof = {'passed': False, 'cycles': 32, 'samples': [], 'evidence_dir': str(evidence)}
    try:
        fixture = root / 'project'
        subprocess.run([args.fixture, '--write-native-fixture', str(fixture)], check=True,
                       capture_output=True, timeout=20)
        layout = 'schema=1\n'
        for name in ('Scene', 'Game', 'Project', 'Hierarchy', 'Inspector', 'Content',
                     'Console', 'Profiler', 'Build'):
            layout += f'[Window][{name}###nexora.{name.lower()}]\n'
            layout += ('Pos=0,40\nSize=620,640\nCollapsed=0\n\n' if name == 'Scene' else
                       'Pos=640,40\nSize=620,640\nCollapsed=0\n\n' if name == 'Game' else
                       'Pos=0,700\nSize=100,20\nCollapsed=1\n\n')
        (fixture / '.nexora/editor-layout.ini').write_text(layout)
        originals = hashes(fixture)
        server, display = start_xvfb(args.xvfb, '1280x720x24')
        require(display, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display, NEXORA_SCENE_SPRITE_UI_TEST_EVIDENCE='1',
                   NEXORA_PROJECT_ASSET_WORKFLOW_EVIDENCE='1', XDG_STATE_HOME=str(root / 'state'))

        def launch(path, label, readonly=True):
            nonlocal editor, window, log
            log = evidence / (label + '.log')
            command = [args.editor, '--graphical', '--native-scene-preview', '--frames=100000',
                       '--project=' + str(path), '--recent-projects=' + str(root / label)]
            if readonly:
                command.append('--read-only')
            with log.open('wb') as err, (evidence / (label + '.stdout')).open('wb') as out:
                editor = subprocess.Popen(command, env=dict(env, NEXORA_LOG_DIR=str(
                    evidence / (label + '-core'))), stdout=out, stderr=err)
            window = int(wait_for_window(args.xdotool, env))
            send('windowfocus', str(window))
            return wait(lambda s: s.get('composed') == '1' and s.get('sources') == '2' and
                        s.get('focused') == '1' and s.get('admitted') == '1', 'Scene readiness')

        def observe():
            data = log.read_bytes()
            require(len(data) < 4 * 1024 * 1024, 'native evidence output exceeded4MiB')
            lines = re.findall(rb'^scene sprite ui (.*)\n', data, re.M)
            return dict(re.findall(r'(\w+)=([^ ]*)', lines[-1].decode())) if lines else {}

        def wait(predicate, reason, timeout=10):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                require(editor.poll() is None, 'Editor exited: ' + log.read_text()[-4096:])
                current = observe()
                if predicate(current):
                    return current
                time.sleep(.03)
            raise RuntimeError(reason + ': ' + log.read_text()[-4096:])

        def send(*arguments):
            subprocess.run([args.xdotool, *map(str, arguments)], env=env, check=True, timeout=3)

        def key(name):
            slots = {'ctrl': 0, 's': 1, 'z': 2, 'y': 3, '1': 4, '3': 5, 'w': 6,
                     'Home': 7, 'F5': 8, 'F6': 9, 'F10': 10, 'a': 11}
            keys = name.split('+')
            try:
                for token in keys:
                    send(*physical_key_arguments(('keydown', token), env))
                    wait(lambda s: s.get('keys', '0' * 12)[slots[token]] == '1',
                         'processed ' + token + ' press')
                for token in reversed(keys):
                    send(*physical_key_arguments(('keyup', token), env))
                    wait(lambda s: s.get('keys', '1' * 12)[slots[token]] == '0',
                         'processed ' + token + ' release')
            finally:
                for token in reversed(keys):
                    send(*physical_key_arguments(('keyup', token), env))

        def move(point):
            send('mousemove', '--window', window, *point)
            wait(lambda s: s.get('pointer') == ','.join(map(str, point)),
                 'processed native pointer position')

        def button(value, down):
            send('mousedown' if down else 'mouseup', value)
            field = 'down' if value == 1 else 'right'
            wait(lambda s: s.get(field) == str(int(down)), 'processed native mouse button')

        def click(point, button_id=1):
            move(point)
            button(button_id, True)
            button(button_id, False)

        def control(name):
            state = wait(lambda s: name in s, 'rendered ' + name)
            point, stable = state[name], time.monotonic()
            deadline = stable + 10
            while time.monotonic() < deadline:
                require(editor.poll() is None, 'Editor exited positioning ' + name)
                value = observe().get(name)
                if value != point:
                    point, stable = value, time.monotonic()
                if point and time.monotonic() - stable >= .4:
                    return tuple(round(float(x)) for x in point.split(','))
                time.sleep(.02)
            raise RuntimeError('Unstable rendered ' + name)

        def geometry(state):
            return (tuple(map(int, state['viewport'].split(','))),
                    tuple(map(float, state['camera'].split(','))))

        def pixels(viewport):
            return native_pixels(display, window, viewport)

        def sample(rgb, viewport, point):
            x, y = point[0] - viewport[0], point[1] - viewport[1]
            require(0 <= x < viewport[2] and 0 <= y < viewport[3], 'sample outside Scene')
            offset = (y * viewport[2] + x) * 3
            return tuple(rgb[offset:offset + 3])

        def source_samples(state, alternate=False, strict=True):
            viewport, camera = geometry(state)
            rgb = pixels(viewport)
            # Centers of opaque texels, far from cutout/linear filtering boundaries.
            colors = ((255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0))
            for mirrored in (False, True):
                for column, row, color in ((1, 0, colors[0]), (3, 0, colors[1]),
                                           (1, 1, colors[2]), (3, 1, colors[3])):
                    local_x = -.5 + (column + .5) * .5
                    world = ((2 - local_x) if mirrored else (-2 + local_x),
                             3.5 - (row + .5) * .5, 0)
                    point = project(world, camera, viewport)
                    actual = sample(rgb, viewport, point)
                    if alternate:
                        color = tuple(reversed(color))
                    matches = all(a > 150 if c else a < 100 for a, c in zip(actual, color))
                    if not strict and not matches:
                        return False
                    require(matches, f'projected trim/Pivot/mirror color differs: {world} {point} {actual}')
                    proof['samples'].append({'world': world, 'point': point, 'rgb': actual})
            return viewport, camera, rgb

        def close():
            nonlocal editor
            request_window_close(str(window), env)
            editor.wait(timeout=15)
            require(editor.returncode == 0, 'native shutdown failed: ' + log.read_text()[-4096:])
            require(not re.search(r'Validation Error|VUID-|SYNC-HAZARD-', log.read_text()),
                    'Vulkan validation failed')
            require(re.search(r'graphical evidence: .*ui_rejected=0 ', log.read_text()),
                    'stale native UI texture references')
            editor = None

        state = launch(fixture, 'readonly')
        # The source status adds a row after first admission; consume settled presented geometry.
        time.sleep(.5)
        state = observe()
        viewport, camera, baseline = source_samples(state)
        # The alpha-zero texel is visibly empty and cannot select the source plane.
        transparent = project((-2.25, 3.25, 0), camera, viewport)
        background = project((-2.8, 3.25, 0), camera, viewport)
        require(sample(baseline, viewport, transparent) == sample(baseline, viewport, background),
                'transparent cutout did not expose the Scene background')
        key('ctrl+1')
        click(transparent)
        wait(lambda s: s.get('selected') == '', 'transparent source intercepted selection')
        for mirrored, entity in ((False, 2), (True, 3)):
            point = project(((1.75 if mirrored else -1.75), 3.25, 0), camera, viewport)
            click(point)
            wait(lambda s: s.get('selected') == str(entity) + ',', 'opaque source pick failed')
        # Clear selection before whole-canvas identity comparisons (no gizmo overlay).
        click(background)
        wait(lambda s: s.get('selected') == '', 'empty Scene did not clear selection')
        time.sleep(.15)
        baseline = pixels(viewport)
        for cycle in range(32):
            key('F5')
            wait(lambda s: s.get('state') == '1' and s.get('composed') == '1', 'dual Scene/Game Play')
            key('F6')
            wait(lambda s: s.get('state') == '2', 'Pause')
            require(pixels(viewport) == baseline, 'Play changed the complete authoring canvas')
            if cycle == 0:
                screenshot(display, window, evidence / 'dual-paused.png')
                key('F10')
                time.sleep(.15)
                require(pixels(viewport) == baseline, 'Step changed authoring pixels')
                key('F6')
                wait(lambda s: s.get('state') == '1', 'Resume')
            key('F5')
            wait(lambda s: s.get('state') == '0' and s.get('composed') == '1', 'Stop')
        require(hashes(fixture) == originals, 'readonly preview/Play changed project files')
        key('ctrl+1')
        key('Home')
        state = wait(lambda s: abs(float(s['camera'].split(',')[5]) - 1.55) < .0001,
                     'Frame All ignored exact source/mesh union bounds')
        source_samples(state)
        screenshot(display, window, evidence / 'frame-all.png')
        close()
        require(hashes(fixture) == originals, 'readonly navigation changed persisted state')
        proof['readonly_sha256'] = hashlib.sha256(baseline).hexdigest()
        proof['frame_all_camera'] = state['camera']
        # Floating auxiliary windows use the same ImGui order as both sampled images.
        covered = root / 'covered'
        shutil.copytree(fixture, covered)
        (covered / '.nexora/editor-layout.ini').write_text(layout.replace(
            '[Window][Inspector###nexora.inspector]\nPos=0,700\nSize=100,20\nCollapsed=1',
            '[Window][Inspector###nexora.inspector]\nPos=240,400\nSize=250,200\nCollapsed=0'))
        covered_before = hashes(covered)
        state = launch(covered, 'occlusion')
        time.sleep(.5)
        state = observe()
        viewport, camera = geometry(state)
        key('ctrl+3')
        probe = project((-1.75, 3.25, 0), camera, viewport)
        wait(lambda _: sample(pixels(viewport), viewport, probe)[0] < 150,
             'floating Inspector did not occlude Scene source')
        screenshot(display, window, evidence / 'occluded.png')
        click((248, 410))
        wait(lambda _: sample(pixels(viewport), viewport, probe)[0] > 150,
             'collapsed Inspector did not reveal source')
        source_samples(observe())
        send('windowsize', window, '1200', '680')
        time.sleep(.5)
        source_samples(observe())
        send('windowsize', window, '1280', '720')
        time.sleep(.5)
        source_samples(observe())
        screenshot(display, window, evidence / 'revealed-restored.png')
        close()
        require(hashes(covered) == covered_before, 'occlusion/resize changed readonly files')
        # Actual native Move preview, commit, Undo and Redo on an editable document.
        writable = root / 'writable'
        shutil.copytree(fixture, writable)
        state = launch(writable, 'authoring', readonly=False)
        time.sleep(.5)
        state = observe()
        viewport, camera, _ = source_samples(state)
        key('ctrl+1')
        key('w')
        point = project((-1.75, 3.25, 0), camera, viewport)
        click(point)
        wait(lambda s: s.get('selected') == '2,', 'editable source selection')
        # Establish the normal Save-status row before comparing exact canvas pixels.
        key('ctrl+s')
        time.sleep(.5)
        state = observe()
        viewport, camera = geometry(state)
        point = project((-1.75, 3.25, 0), camera, viewport)
        before = pixels(viewport)
        scene_path = writable / '.nexora/scenes/Main.scene'
        initial_scene = scene_path.read_bytes()
        move(point)
        button(1, True)
        move((point[0] + 40, point[1] + 12))
        wait(lambda _: pixels(viewport) != before, 'source did not move during prospective drag')
        require(scene_path.read_bytes() == initial_scene, 'prospective drag persisted early')
        button(1, False)
        key('ctrl+s')
        wait(lambda _: scene_path.read_bytes() != initial_scene, 'source transform did not commit')
        committed = scene_path.read_bytes()
        moved = pixels(viewport)
        key('ctrl+z')
        key('ctrl+s')
        wait(lambda _: scene_path.read_bytes() == initial_scene and pixels(viewport) == before,
             'source Move Undo did not restore exact bytes/pixels')
        key('ctrl+y')
        key('ctrl+s')
        wait(lambda _: scene_path.read_bytes() == committed and pixels(viewport) == moved,
             'source Move Redo did not restore exact bytes/pixels')
        key('ctrl+z')
        key('ctrl+s')
        wait(lambda _: scene_path.read_bytes() == initial_scene, 'final transform Undo')
        close()
        # Real Content context Reimport replaces immutable Atlas owners; no injected model action.
        (writable / '.nexora/editor-layout.ini').write_text(layout.replace(
            '[Window][Game###nexora.game]\nPos=640,40\nSize=620,640\nCollapsed=0',
            '[Window][Game###nexora.game]\nPos=0,700\nSize=100,20\nCollapsed=1').replace(
            '[Window][Content###nexora.content]\nPos=0,700\nSize=100,20\nCollapsed=1',
            '[Window][Content###nexora.content]\nPos=640,40\nSize=620,640\nCollapsed=0'))
        state = launch(writable, 'reimport', readonly=False)
        time.sleep(.5)
        state = observe()
        click(control('search'))
        key('ctrl+a')
        send('type', '--clearmodifiers', '--delay', '20', 'Atlas')
        time.sleep(.3)
        last_texture = int(observe()['texture'])
        for generation in range(1, 33):
            subprocess.run([args.fixture, '--replace-native-atlas', str(writable),
                            str(generation % 2)], check=True, capture_output=True, timeout=10)
            state = wait(lambda s: 'atlaspoint' in s, 'actual Atlas row')
            click(control('atlaspoint'), button_id=3)
            state = wait(lambda s: 'reimport' in s, 'actual Reimport context command')
            click(control('reimport'))
            state = wait(lambda s: int(s.get('texture', '0')) > last_texture and
                         s.get('sources') == '2' and s.get('composed') == '1',
                         'fresh immutable Atlas did not reach native Scene')
            last_texture = int(state['texture'])
            # EndFrame queues presentation; X11 readback must independently observe scanout.
            wait(lambda s: source_samples(s, alternate=bool(generation % 2), strict=False),
                 'presented Atlas colors did not reach the native window')
            require(scene_path.read_bytes() == initial_scene, 'Atlas reimport changed source refs')
        saved_atlas = (writable / 'Content/Atlas.nx2atlas').read_bytes()
        (writable / 'Content/Atlas.nx2atlas').write_bytes(b'corrupt Atlas')
        state = observe()
        click(control('atlaspoint'), button_id=3)
        state = wait(lambda s: 'reimport' in s, 'corrupt Atlas Reimport command')
        click(control('reimport'))

        def rejected_reimport(_):
            lines = re.findall(rb'^project asset workflow (.*)\n', log.read_bytes(), re.M)
            status = dict(re.findall(r'(\w+)=([^ ]*)', lines[-1].decode())) if lines else {}
            return status.get('busy') == '0' and status.get('reimport_state') == '6'

        wait(rejected_reimport, 'corrupt Atlas reimport did not report failure')
        require(int(observe()['texture']) == last_texture, 'failed reimport replaced last-good owner')
        source_samples(observe())
        (writable / 'Content/Atlas.nx2atlas').write_bytes(saved_atlas)
        require(scene_path.read_bytes() == initial_scene, 'failed reimport changed source references')
        proof['atlas_generations'] = 32
        proof['last_scene_texture'] = last_texture
        screenshot(display, window, evidence / 'reimported.png')
        close()
        proof['passed'] = True
        print(json.dumps(proof, sort_keys=True))
    except Exception:
        if editor and editor.poll() is None and window:
            screenshot(display, window, evidence / 'failure.png')
        raise
    finally:
        if editor and editor.poll() is None:
            editor.terminate()
            editor.wait(timeout=10)
        if server:
            server.terminate()
            server.wait(timeout=10)
        (evidence / 'result.json').write_text(json.dumps(proof, indent=2, sort_keys=True))
        if proof['passed']:
            shutil.rmtree(root)
        else:
            print('Retained fixture: ' + str(root), flush=True)


if __name__ == '__main__':
    main()
