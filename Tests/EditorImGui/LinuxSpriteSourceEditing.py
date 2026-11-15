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

from LinuxDisplayAcceptance import request_window_close, start_xvfb, wait_for_window
from LinuxReflectedInspector import rendered_pixels
from NativeSceneInput import NativeSceneInput
from LinuxPackagedSprites import native_pixels
from LinuxSceneSprites import project as project_point


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    evidence = Path.cwd() / 'artifacts/editor-sprite-sources' / ('editing-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    root = evidence / 'project-來源'
    subprocess.run([args.fixture, '--write-native-fixture', str(root)], check=True, timeout=15)
    scene = root / '.nexora/scenes/Main.scene'
    initial_scene = scene.read_bytes()
    xvfb, display = start_xvfb(args.xvfb, '1280x720x24')
    process = driver = None
    proof = {'scope': 'main Editor existing Sprite source editing; ED-WF07/08 remain open',
             'native_input': True, 'passed': False, 'captures': []}
    try:
        require(display, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display, XDG_STATE_HOME=str(evidence / 'state'),
                   MESA_SHADER_CACHE_DIR=str(evidence / 'shader-cache'),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1',
                   NEXORA_SCENE_SPRITE_UI_TEST_EVIDENCE='1',
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
            command = [args.editor, '--graphical', '--native-scene-preview',
                       '--project=' + str(root), '--recent-projects=' + str(evidence / 'recent')]
            if readonly:
                command.append('--read-only')
            process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
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

        def open_editor():
            x, y = point('sprite_create_source')
            driver.send('mousemove', '--window', driver.window, x, y, 'click', '3')
            driver.click(point('sprite_edit_menu'))

        open_host()
        open_creator()
        for index, text in ((0, 'Existing'), (2, '19'), (3, '23'), (4, '3'), (5, '5'),
                            (6, '4.5'), (7, '8.25'), (8, '2')):
            field(index, text)
        driver.click(point('sprite_create_9'))
        driver.wait(lambda v: 'sprite_create_2' not in v, 'source creation did not close')
        source_path = root / 'Content/Existing.nx2sprite'
        before_source = source_path.read_bytes()
        high, low = struct.unpack_from('<QQ', before_source, 8)
        identity_path = root / 'Content/Existing.nx2sprite.meta'
        identity = identity_path.read_bytes()
        driver.click(point('scene_select_all'))
        driver.wait(lambda v: v.get('source_selection_count') == '2', 'selection failed')
        driver.click(point('sprite_source_combo'))
        driver.click(point('sprite_source_first'))
        driver.chord('ctrl+s')
        expected_reference = (b'\x01' + struct.pack('<QQ', high, low)).hex().encode()
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            refs = re.findall(rb'^opaque \d+ 4991205891477078017 "editor.sprite.source" ([0-9a-f]+)$',
                              scene.read_bytes(), re.M)
            if refs == [expected_reference, expected_reference]:
                break
            driver.drain(.05)
        require(refs == [expected_reference, expected_reference], 'source assignment did not Save')
        saved_scene = scene.read_bytes()
        def scene_state():
            records = re.findall(rb'^scene sprite ui (.*)$', driver.stderr, re.M)
            return dict(re.findall(r'(\w+)=([^ ]*)', records[-1].decode())) if records else {}

        driver.wait(lambda v: scene_state().get('sources') == '2' and
                    scene_state().get('composed') == '1', 'native Scene sources were not admitted')
        driver.raw('key', 'ctrl+1')
        point('sprite_create_source')
        observed = scene_state()
        viewport = tuple(map(int, observed['viewport'].split(',')))
        camera = tuple(map(float, observed['camera'].split(',')))
        old_probe = project_point((2, 3, 0), camera, viewport)
        shared_probe = project_point((0, 3, 0), camera, viewport)
        def sample(point):
            rgb = native_pixels(display, int(driver.window), viewport)
            x, y = point[0] - viewport[0], point[1] - viewport[1]
            require(0 <= x < viewport[2] and 0 <= y < viewport[3],
                    'independent source probe is outside the presented Scene')
            offset = (y * viewport[2] + x) * 3
            return tuple(rgb[offset:offset + 3])

        before_pixel = sample(old_probe)
        source_pixel = sample(shared_probe)
        background_probe = project_point((4, 3, 0), camera, viewport)
        background_pixel = sample(background_probe)
        capture('original-assigned-scene')
        proof.update(viewport=viewport, camera=camera, old_probe=old_probe, shared_probe=shared_probe,
                     original_probe_pixel=before_pixel, shared_probe_pixel=source_pixel,
                     background_probe_pixel=background_pixel)
        require(before_pixel == source_pixel and source_pixel != background_pixel,
                'independent original source quad pixels missing')
        original_files = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        open_editor()
        require('sprite_create_0' not in driver.observation(), 'editing exposed a filename mutation')
        for index, text in ((2, '31'), (3, '37'), (4, '6'), (5, '7'),
                            (6, '9.75'), (7, '12.5'), (8, '4')):
            field(index, text)
        field(11, '1')
        driver.click(point('sprite_create_9'))
        require(source_path.read_bytes() == before_source, 'invalid bordered Trim was saved')
        field(11, '0')
        capture('existing-source-edit-draft')
        require(source_path.read_bytes() == before_source and scene.read_bytes() == saved_scene,
                'editing draft mutated authored bytes')
        driver.click(point('sprite_create_9'))
        driver.wait(lambda v: 'sprite_create_2' not in v, 'source editing did not close')
        source = source_path.read_bytes()
        require(source[:24] == before_source[:24] and len(source) == 96,
                'source identity or wire changed')
        require(struct.unpack_from('<QQ7I3f4I', source, 24) ==
                (101, 1, 7, 31, 37, 6, 7, 7, 9, 9.75, 12.5, 4, 0, 0, 0, 0),
                'saved source differs from actual edit controls')
        require(identity_path.read_bytes() == identity and scene.read_bytes() == saved_scene,
                'source editing changed identity sidecar or Scene references/history')
        for name, value in original_files.items():
            if name != 'Content/Existing.nx2sprite':
                require((root / name).read_bytes() == value, 'edit changed unrelated file: ' + name)
        driver.wait(lambda v: sample(old_probe) != source_pixel and
                    sample(shared_probe) == source_pixel,
                    'edited Trim/Pivot/PPU did not update native Scene geometry')
        require(tuple(map(float, scene_state()['camera'].split(','))) == camera,
                'source editing changed authoring camera')
        after_pixel = sample(old_probe)
        driver.click(point('sprite_source_details'))
        expected_details = (b'Original: 31 x 37 px', b'Pivot (bottom-left): 9.75, 12.5 px',
                            b'Trim (top-left): 6, 7; 7 x 9 px', b'Pixels per unit: 4')
        driver.wait(lambda v: all(t in driver.stderr for t in expected_details),
                    'current immutable Inspector metadata was not published')
        capture('edited-source-assigned-details')
        driver.click(point('sprite_source_details'))
        open_editor()
        field(6, '999')
        driver.click(point('sprite_create_10'))
        require(source_path.read_bytes() == source, 'cancelled edit replaced source')
        close_host()
        saved = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        open_host(True)
        driver.click(point('scene_select_all'))
        driver.wait(lambda v: v.get('source_selection_count') == '2', 'read-only selection failed')
        driver.click(point('sprite_source_details'))
        driver.wait(lambda v: all(t in driver.stderr for t in expected_details),
                    'read-only reopen lost edited metadata')
        capture('edited-source-readonly-reopened')
        driver.click(point('sprite_source_details'))
        open_editor()
        require('sprite_create_2' not in driver.observation(), 'read-only editing admitted')
        driver.raw('key', 'Escape')
        close_host()
        after = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        require(after == saved, 'read-only edited project bytes changed')
        proof.update(passed=True, asset_uuid=f'{high:016x}{low:016x}', source_bytes=96,
                     before_source_sha256=hashlib.sha256(before_source).hexdigest(),
                     edited_source_sha256=hashlib.sha256(source).hexdigest(),
                     same_uuid_and_sidecar_after_edit=True, edited_metadata_published=True,
                     scene_bytes_unchanged_by_edit=True, invalid_trim_border_rejected=True,
                     actual_scene_quad_pixel_transition=True,
                     old_only_probe=old_probe, shared_probe=shared_probe,
                     before_pixel=before_pixel, after_pixel=after_pixel,
                     cancelled_edit_preserved_source=True,
                     read_only_all_file_hashes={p: hashlib.sha256(b).hexdigest() for p, b in saved.items()})
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
        print('Sprite source editing evidence:', evidence, flush=True)


if __name__ == '__main__':
    main()
