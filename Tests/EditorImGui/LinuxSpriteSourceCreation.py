#!/usr/bin/env python3
"""Create an actual source through X11, assign it, save and reopen without writes."""
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


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    evidence = Path.cwd() / 'artifacts/editor-sprite-sources' / ('creation-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    root = evidence / 'project-來源'
    subprocess.run([args.fixture, '--write-native-fixture', str(root)], check=True, timeout=15)
    scene = root / '.nexora/scenes/Main.scene'
    initial_scene = scene.read_bytes()
    xvfb, display = start_xvfb(args.xvfb, '1280x720x24')
    process = driver = None
    proof = {'scope': 'main Editor new Sprite source creation; ED-WF07/08 remain open',
             'native_input': True, 'passed': False, 'captures': []}
    try:
        require(display, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display, XDG_STATE_HOME=str(evidence / 'state'),
                   MESA_SHADER_CACHE_DIR=str(evidence / 'shader-cache'),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1',
                   NEXORA_SPRITE_SOURCE_INSPECTOR_TEST_EVIDENCE='1')

        def point(name):
            deadline = time.monotonic() + 8
            previous, stable, first = None, time.monotonic(), 0
            while time.monotonic() < deadline:
                driver.drain(.05)
                value = driver.observation().get(name)
                frames = re.findall(rb'^sprite inspector frame=(\d+)\n', driver.stderr, re.M)
                frame = int(frames[-1]) if frames else 0
                if value != previous:
                    previous, stable, first = value, time.monotonic(), frame
                if value and time.monotonic() - stable >= .35 and frame >= first + 2:
                    return tuple(round(float(x)) for x in value.split(','))
                require(process.poll() is None, 'Editor exited before ' + name)
            raise RuntimeError('Visible control did not settle: ' + name)

        def capture(name):
            pixels = rendered_pixels(display, driver.window, (0, 0, 1280, 720))
            require(len(set(pixels)) > 16, 'native Editor pixels absent')
            path = evidence / (name + '.ppm')
            path.write_bytes(b'P6\n1280 720\n255\n' + bytes(c for p in pixels for c in p))
            proof['captures'].append({'file': path.name,
                                      'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})

        def open_host(readonly=False):
            nonlocal process, driver
            process = launch(args.editor, root, evidence / 'recent', env, read_only=readonly, binary=True)
            window = wait_for_window(args.xdotool, env)
            driver = NativeSceneInput(process, args.xdotool, env, window, evidence,
                                      2 if readonly else 1, ack_pointer=True)
            driver.raw('windowfocus', '--sync', window)
            point('sprite_create_atlas')

        def close_host():
            nonlocal process, driver
            request_window_close(driver.window, env)
            driver.finish(15)
            require(process.returncode == 0, 'native shutdown failed')
            process = driver = None

        def open_creator():
            x, y = point('sprite_create_atlas')
            driver.send('mousemove', '--window', driver.window, x, y, 'click', '3')
            driver.click(point('sprite_create_menu'))

        def field(index, text):
            driver.click(point('sprite_create_' + str(index)))
            driver.chord('ctrl+a')
            driver.raw('type', '--clearmodifiers', '--delay', '20', text)
            driver.raw('key', 'Return')

        open_host()
        old_content = {p.name: p.read_bytes() for p in (root / 'Content').iterdir() if p.is_file()}
        open_creator()
        for index, text in ((0, 'Created'), (2, '19'), (3, '23'), (4, '3'), (5, '5'),
                            (6, '4.5'), (7, '8.25'), (8, '2')):
            field(index, text)
        require(scene.read_bytes() == initial_scene and not list((root / 'Content').glob('*.nx2sprite')),
                'draft rendering wrote authoring files')
        capture('source-draft')
        driver.click(point('sprite_create_9'))
        source_path = root / 'Content/Created.nx2sprite'
        driver.wait(lambda v: 'sprite_create_0' not in v, 'created source dialog did not close')
        require(source_path.is_file() and source_path.stat().st_size == 96, 'canonical source absent')
        source = source_path.read_bytes()
        # Codec layout is independently inspected, never used to create the successful source.
        require(source[:4] == b'ND2P' and struct.unpack_from('<I', source, 4) == (1,),
                'unexpected Sprite source schema')
        high, low = struct.unpack_from('<QQ', source, 8)
        require((high, low) != (0, 0) and (high, low) != (101, 1), 'new UUID missing')
        require(struct.unpack_from('<QQ7I3f4I', source, 24) ==
                (101, 1, 7, 19, 23, 3, 5, 7, 9, 4.5, 8.25, 2, 0, 0, 0, 0),
                'created asymmetric geometry differs from native inputs')
        require(all((root / 'Content' / name).read_bytes() == data for name, data in old_content.items()),
                'source creation changed an existing asset or identity')
        require(scene.read_bytes() == initial_scene, 'source creation changed Scene')
        driver.click(point('scene_select_all'))
        driver.wait(lambda v: v.get('source_selection_count') == '2', 'selection failed')
        driver.click(point('sprite_source_combo'))
        driver.click(point('sprite_source_first'))
        driver.click(point('sprite_source_details'))
        expected_details = (b'Original: 19 x 23 px', b'Pivot (bottom-left): 4.5, 8.25 px',
                            b'Trim (top-left): 3, 5; 7 x 9 px', b'Pixels per unit: 2')
        driver.wait(lambda v: all(t in driver.stderr for t in expected_details), 'imported geometry absent')
        capture('created-assigned-details')
        driver.click(point('sprite_source_details'))
        driver.chord('ctrl+s')
        expected = (b'\x01' + struct.pack('<QQ', high, low)).hex().encode()
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            data = scene.read_bytes()
            refs = re.findall(rb'^opaque \d+ 4991205891477078017 "editor.sprite.source" ([0-9a-f]+)$', data, re.M)
            if refs == [expected, expected]:
                break
            driver.drain(.05)
        require(refs == [expected, expected], 'native Save did not persist the new UUID')
        close_host()
        saved = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        open_host(True)
        driver.click(point('scene_select_all'))
        driver.wait(lambda v: v.get('source_selection_count') == '2', 'read-only selection failed')
        driver.click(point('sprite_source_details'))
        driver.wait(lambda v: all(t in driver.stderr for t in expected_details), 'read-only geometry absent')
        capture('readonly-reopened-details')
        driver.click(point('sprite_source_details'))
        open_creator()
        require('sprite_create_0' not in driver.observation(), 'read-only creation admitted')
        driver.raw('key', 'Escape')
        driver.chord('ctrl+s')
        close_host()
        after = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        require(after == saved, 'read-only reopen modified project bytes')
        proof.update(passed=True, asset_uuid=f'{high:016x}{low:016x}', source_bytes=96,
                     source_sha256=hashlib.sha256(source).hexdigest(),
                     read_only_all_file_hashes={p: hashlib.sha256(b).hexdigest() for p, b in saved.items()},
                     same_uuid_after_save_reopen=True, existing_sources_unchanged=True)
    finally:
        if driver:
            driver.abort()
        elif process and process.poll() is None:
            process.kill()
            process.communicate(timeout=5)
        if xvfb.poll() is None:
            xvfb.terminate()
        xvfb.communicate(timeout=5)
        (evidence / 'proof.json').write_text(json.dumps(proof, indent=2) + '\n')
        print('Sprite source creation evidence:', evidence, flush=True)


if __name__ == '__main__':
    main()
