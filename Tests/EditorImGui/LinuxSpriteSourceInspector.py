#!/usr/bin/env python3
"""Real X11 source selection/clear, batch history, save/reopen and read-only rejection."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import time

from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from LinuxReflectedInspector import rendered_pixels
from NativeSceneInput import NativeSceneInput


def require(value, message):
    if not value:
        raise RuntimeError(message)


def references(data):
    return re.findall(rb'^opaque \d+ 4991205891477078017 "editor.sprite.source" ([0-9a-f]+)$', data, re.M)


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    evidence = Path.cwd() / 'artifacts/editor-sprite-sources' / ('inspector-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    root = evidence / 'project-來源'
    subprocess.run([args.fixture, '--write-native-fixture', str(root)], check=True, timeout=15)
    scene = root / '.nexora/scenes/Main.scene'
    initial = scene.read_bytes()
    (evidence / 'initial.scene').write_bytes(initial)
    expected = (b'\x01' + struct.pack('<QQ', 102, 1)).hex().encode()
    xvfb, display = start_xvfb(args.xvfb, '1280x720x24')
    process = driver = None
    proof = {'scope': 'main Inspector reference authoring only; full ED-WF07 remains open',
             'native_input': True, 'attempts': [], 'passed': False}
    try:
        require(display is not None, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display, XDG_STATE_HOME=str(evidence / 'state'),
                   MESA_SHADER_CACHE_DIR=str(evidence / 'shader-cache'),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1',
                   NEXORA_SPRITE_SOURCE_INSPECTOR_TEST_EVIDENCE='1')

        def point(name):
            deadline = time.monotonic() + 8
            previous = None
            stable = time.monotonic()
            first_frame = 0
            while time.monotonic() < deadline:
                driver.drain(.05)
                value = driver.observation()
                current = value.get(name)
                frames = re.findall(rb'^sprite inspector frame=(\d+)\n', driver.stderr, re.M)
                frame = int(frames[-1]) if frames else 0
                if current != previous:
                    previous, stable = current, time.monotonic()
                    first_frame = frame
                if (current and time.monotonic() - stable >= .4 and
                        frame >= first_frame + 2):
                    return tuple(round(float(x)) for x in current.split(','))
                require(process.poll() is None, 'Editor exited before visible control: ' + name)
            raise RuntimeError('Visible control did not settle: ' + name)

        def capture(name):
            pixels = rendered_pixels(display, driver.window, (0, 0, 1280, 720))
            require(len(set(pixels)) > 16, 'presented native Editor pixels absent')
            output = evidence / (name + '.ppm')
            output.write_bytes(b'P6\n1280 720\n255\n' + bytes(c for p in pixels for c in p))
            proof['attempts'].append({'capture': output.name,
                                      'sha256': hashlib.sha256(output.read_bytes()).hexdigest()})

        def open_host(readonly=False):
            nonlocal process, driver
            number = len(proof['attempts']) + 1
            process = launch(args.editor, root, evidence / 'recent', env, read_only=readonly, binary=True)
            window = wait_for_window(args.xdotool, env)
            driver = NativeSceneInput(process, args.xdotool, env, window, evidence, number, ack_pointer=True)
            driver.raw('windowfocus', '--sync', window)
            driver.click(point('scene_select_all'))
            driver.wait(lambda v: v.get('source_selection_count') == '2', 'two source targets not selected')
            point('sprite_source_combo')

        def close_host():
            nonlocal process, driver
            request_window_close(driver.window, env)
            driver.finish(15)
            require(process.returncode == 0, 'native Inspector shutdown failed')
            process = driver = None

        def save(expected_bytes=None, count=2):
            driver.chord('ctrl+s')
            deadline = time.monotonic() + 8
            while time.monotonic() < deadline:
                data = scene.read_bytes()
                if (expected_bytes is not None and data == expected_bytes) or (
                        expected_bytes is None and references(data) == [expected] * count):
                    return data
                driver.drain(.05)
            raise RuntimeError('Native save did not publish expected exact source references')

        open_host()
        capture('unassigned')
        driver.click(point('sprite_source_combo'))
        driver.click(point('sprite_source_first'))
        assigned = save()
        (evidence / 'assigned.scene').write_bytes(assigned)
        require(assigned != initial, 'source assignment was not persisted')
        capture('assigned')
        driver.chord('ctrl+z')
        save(initial)
        (evidence / 'undone.scene').write_bytes(scene.read_bytes())
        driver.chord('ctrl+shift+z')
        save(assigned)
        driver.click(point('sprite_source_clear'))
        save(initial)
        driver.chord('ctrl+z')
        save(assigned)
        close_host()
        before = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        open_host(True)
        require(references(scene.read_bytes()) == [expected, expected], 'reopened references changed')
        driver.click(point('sprite_source_clear'))
        driver.click(point('sprite_source_combo'))
        driver.chord('ctrl+s')
        capture('readonly-reopened')
        close_host()
        after = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        require(before == after, 'read-only native restart changed project bytes')
        require(b'"unrelated.plugin" 07090b' in assigned, 'unrelated opaque bytes changed')
        proof.update(passed=True, selected=2, reference_hex=expected.decode(),
                     initial_sha256=hashlib.sha256(initial).hexdigest(),
                     assigned_sha256=hashlib.sha256(assigned).hexdigest(),
                     one_undo_exact=True, redo_exact=True, clear_undo_exact=True,
                     readonly_project_bytes_unchanged=True,
                     before_project_sha256={p: hashlib.sha256(b).hexdigest() for p, b in before.items()},
                     after_project_sha256={p: hashlib.sha256(b).hexdigest() for p, b in after.items()})
        print('Native Sprite Inspector selection/clear/batch Undo/Redo/save/reopen/read-only passed')
    finally:
        if driver:
            driver.abort()
        elif process and process.poll() is None:
            process.kill()
            process.communicate(timeout=5)
        if xvfb:
            xvfb.terminate()
            xvfb.communicate(timeout=5)
        (evidence / 'receipt.json').write_text(json.dumps(proof, indent=2) + '\n')
        print('Sprite Inspector native evidence:', evidence)


if __name__ == '__main__':
    main()
