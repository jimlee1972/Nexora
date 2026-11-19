#!/usr/bin/env python3
"""Actual main Editor Atlas/Clip authoring, native pixel publication and read-only reopen."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import shutil
import time
import subprocess

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
    for name in ('editor', 'fixture', 'player', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--workflow', action='store_true')
    args = parser.parse_args()
    evidence = Path.cwd() / ('artifacts/editor-sprite-workflow' if args.workflow else
                             'artifacts/editor-atlas-clip') / str(time.time_ns())
    evidence.mkdir(parents=True)
    root = evidence / 'project-切片'
    subprocess.run([args.fixture, '--write-native-fixture', str(root)], check=True, timeout=15)
    scene = root / ('Authored.scene' if args.workflow else '.nexora/scenes/Main.scene')
    original_scene = scene.read_bytes()
    atlas = root / 'Content/Atlas.nx2atlas'
    clip = root / 'Content/Animation.nx2clip'
    old_atlas, old_clip = atlas.read_bytes(), clip.read_bytes()
    xvfb, display = start_xvfb(args.xvfb, '1600x1000x24')
    process = driver = None
    launch_number = 0
    proof = {'passed': False, 'native_input': True, 'captures': []}
    try:
        require(display, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display, XDG_STATE_HOME=str(evidence / 'state'),
                   MESA_SHADER_CACHE_DIR=str(evidence / 'shader-cache'),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1',
                   NEXORA_SCENE_SPRITE_UI_TEST_EVIDENCE='1',
                   NEXORA_SPRITE_SOURCE_INSPECTOR_TEST_EVIDENCE='1')
        if args.workflow:
            env.update(NEXORA_GAME_TWOD_UI_TEST_EVIDENCE='1',
                       NEXORA_PACKAGED_SPRITE_UI_TEST_EVIDENCE='1',
                       NEXORA_PROJECT_ASSET_WORKFLOW_EVIDENCE='1')

        def point(name, observe=None):
            deadline = time.monotonic() + 8
            previous, stable, first = None, time.monotonic(), 0
            while time.monotonic() < deadline:
                driver.drain(.05)
                value = (observe() if observe else driver.observation()).get(name)
                frames = re.findall(rb'^sprite inspector frame=(\d+)\n', driver.stderr, re.M)
                frame = int(frames[-1]) if frames else 0
                if value != previous:
                    previous, stable, first = value, time.monotonic(), frame
                if value and time.monotonic() - stable >= .35 and frame >= first + 2:
                    return tuple(round(float(x)) for x in value.split(','))
                require(process.poll() is None, 'Editor exited before ' + name)
            raise RuntimeError('Visible control did not settle: ' + name)

        def open_host(readonly=False):
            nonlocal process, driver, launch_number
            launch_number += 1
            command = [args.editor, '--graphical', '--native-scene-preview',
                       '--project=' + str(root), '--recent-projects=' + str(evidence / 'recent')]
            if readonly:
                command.append('--read-only')
            process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            window = wait_for_window(args.xdotool, env)
            driver = NativeSceneInput(process, args.xdotool, env, window, evidence,
                                      launch_number, ack_pointer=True)
            driver.raw('windowfocus', '--sync', window)
            point('sprite_create_atlas')
            if workflow:
                workflow.open_scene()

        def close_host():
            nonlocal process, driver
            request_window_close(driver.window, env)
            driver.finish(15)
            require(process.returncode == 0, 'native shutdown failed')
            process = driver = None

        def open_editor(atlas_asset):
            x, y = point('sprite_create_atlas' if atlas_asset else 'asset_edit_clip')
            driver.send('mousemove', '--window', driver.window, x, y, 'click', '3')
            driver.click(point('atlas_edit_menu' if atlas_asset else 'clip_edit_menu'))

        def field(index, text):
            driver.click(point('asset_edit_' + str(index)))
            driver.chord('ctrl+a')
            driver.raw('type', '--clearmodifiers', '--delay', '20', text)
            driver.raw('key', 'Return')

        def scene_state():
            records = re.findall(rb'^scene sprite ui (.*)$', driver.stderr, re.M)
            return dict(re.findall(r'(\w+)=([^ ]*)', records[-1].decode())) if records else {}

        def probe():
            if args.workflow:
                driver.focus_scene()
            else:
                driver.raw('key', 'ctrl+1')
            driver.wait(lambda v: scene_state().get('sources') == '1' and
                        scene_state().get('composed') == '1', 'native Scene source not rendered')
            observed = scene_state()
            viewport = tuple(map(int, observed['viewport'].split(',')))
            camera = tuple(map(float, observed['camera'].split(',')))
            pixel = project_point((0, 3, 0), camera, viewport)
            rgb = native_pixels(display, int(driver.window), viewport)
            x, y = pixel[0] - viewport[0], pixel[1] - viewport[1]
            require(0 <= x < viewport[2] and 0 <= y < viewport[3], 'probe outside Scene')
            offset = (y * viewport[2] + x) * 3
            return tuple(rgb[offset:offset + 3])

        def capture(name):
            rgb = rendered_pixels(display, driver.window, (0, 0, 1280, 720))
            require(len(set(rgb)) > 16, 'native pixels absent')
            if args.workflow:
                from LinuxAuthoredPreview import screenshot
                path = evidence / (name + '.png')
                screenshot(display, int(driver.window), path)
            else:
                path = evidence / (name + '.ppm')
                path.write_bytes(b'P6\n1280 720\n255\n' + bytes(c for p in rgb for c in p))
            proof['captures'].append({'file': path.name,
                                      'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})

        workflow = None
        if args.workflow:
            from SpriteSourceWorkflow import SpriteSourceWorkflow
            workflow = SpriteSourceWorkflow(root, scene, proof, lambda: driver, point, scene_state,
                                            probe, capture, lambda rect: native_pixels(
                                                display, int(driver.window), rect))
        open_host()
        if workflow:
            workflow.assign()
            close_host()
            open_host()
            workflow.reopened()
            workflow.play((240, 20, 20))
        before_pixel = probe()
        require(before_pixel[0] > before_pixel[1] * 2, 'initial red source pixel missing')
        identities = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*.meta')}
        open_editor(True)
        field(9, '10')
        capture('atlas-slice-draft')
        driver.click(point('asset_edit_13'))
        driver.wait(lambda v: atlas.read_bytes() != old_atlas, 'Atlas was not saved')
        edited_atlas = atlas.read_bytes()
        require(edited_atlas[:44] == old_atlas[:44], 'Atlas UUID or dimensions changed')
        # Canonical Atlas header is 44 bytes; first region is id/x/y/width/height.
        require(struct.unpack_from('<IIIII', edited_atlas, 44) == (7, 10, 2, 8, 8),
                'wrong saved Atlas slice')
        point('sprite_create_atlas')
        after_pixel = probe()
        require(after_pixel[1] > after_pixel[0] * 2, 'edited Atlas did not publish green Scene pixel')
        capture('edited-atlas-scene')
        if workflow:
            workflow.play((20, 240, 20))
        open_editor(False)
        field(8, '.375')
        field(9, 'impact')
        driver.click(point('asset_edit_12'))
        driver.click(point('asset_edit_13'))
        driver.wait(lambda v: clip.read_bytes() != old_clip, 'Clip was not saved')
        edited_clip = clip.read_bytes()
        require(edited_clip[:48] == old_clip[:48], 'Clip identity/dependency/loop/count changed')
        offset = 48
        frames = []
        for _ in range(2):
            region, seconds, length = struct.unpack_from('<IdI', edited_clip, offset)
            offset += 16
            event = edited_clip[offset:offset + length].decode('ascii')
            offset += length
            frames.append((region, seconds, event))
        require(offset == len(edited_clip) and frames == [(8, .2, 'last'), (7, .375, 'impact')],
                'Clip frame/time/event/reorder not persisted')
        point('asset_edit_clip')
        require(scene.read_bytes() == original_scene, 'asset authoring changed Scene bytes')
        require(identities == {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*.meta')},
                'asset identity sidecar changed')
        open_editor(False)
        field(8, '.8')
        driver.click(point('asset_edit_14'))
        require(clip.read_bytes() == edited_clip, 'Cancel saved a Clip draft')
        if workflow:
            workflow.reimport_roundtrip(old_atlas, edited_atlas)
            require(identities == {str(p.relative_to(root)): p.read_bytes()
                                  for p in root.rglob('*.meta')},
                    'Reimport changed authored identity sidecars')
        close_host()
        saved = {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        open_host(True)
        reopened_pixel = probe()
        require(reopened_pixel[1] > reopened_pixel[0] * 2, 'read-only reopen lost green pixel')
        open_editor(True)
        require('asset_edit_9' not in driver.observation(), 'read-only Atlas edit opened')
        driver.raw('key', 'Escape')
        close_host()
        require(saved == {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'read-only reopen changed project bytes')
        if workflow:
            require(scene.read_bytes() == original_scene, 'Build scene differs from GUI assignment')
            proof['same_project_workflow']['build_scene_sha256'] = hashlib.sha256(
                scene.read_bytes()).hexdigest()
            settings = json.loads((root / 'project.build.json').read_text())
            require(len(settings['scenes']) == 1 and settings['scenes'][0]['path'] == 'Authored.scene',
                    'Build did not enroll the exact GUI-saved scene')
        # Export the exact GUI-saved bytes and explicit Clip root, then remove every source.
        built = evidence / 'built-game'
        command = [args.editor, '--build-game=' + str(root), '--output=' + str(built),
                   '--runtime-profile=' + str(Path(args.player).parent / 'GameRuntime.profile'),
                   '--build-records=' + str(evidence / 'build-records'), '--scene-only']
        exported = subprocess.run(command, env=env, capture_output=True, text=True, timeout=90)
        (evidence / 'build.stdout.log').write_text(exported.stdout)
        (evidence / 'build.stderr.log').write_text(exported.stderr)
        require(exported.returncode == 0 and json.loads(exported.stdout)['status'] == 'PUBLISHED_GAME',
                'GUI-saved assets did not Build: ' + exported.stderr)
        require(saved == {str(p.relative_to(root)): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Build changed authored project bytes')
        require(any(edited_clip in p.read_bytes() for p in built.rglob('*') if p.is_file() and
                    p.stat().st_size <= 64 * 1024 * 1024), 'exact edited Clip absent from runtime closure')
        moved = evidence / 'moved 遊戲'
        built.rename(moved)
        shutil.make_archive(str(evidence / 'saved-project'), 'zip', root)
        shutil.rmtree(root)
        runtime_before = {str(p.relative_to(moved)): hashlib.sha256(p.read_bytes()).hexdigest()
                          for p in moved.rglob('*') if p.is_file()}
        process = subprocess.Popen([str(moved / 'Game')], cwd=evidence, env=env,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        player_window = None
        deadline = time.monotonic() + 35
        player_pixel = None
        while time.monotonic() < deadline and process.poll() is None:
            found = subprocess.run([args.xdotool, 'search', '--onlyvisible', '--name', '^Nexora Game$'],
                                   env=env, capture_output=True, text=True, timeout=5).stdout.splitlines()
            if len(found) == 1:
                player_window = found[0]
                rgb = native_pixels(display, int(player_window), (0, 0, 960, 640))
                offset = (224 * 960 + 480) * 3
                value = tuple(rgb[offset:offset + 3])
                if value[1] > value[0] * 2 and value[1] > value[2] * 2:
                    player_pixel = value
                    break
            time.sleep(.05)
        require(player_pixel is not None, 'relocated source-free Player lost edited green pixels')
        if workflow:
            expected_pixel = proof['same_project_workflow']['play'][-1]['expected_rgb']
            require(all(abs(a-b) <= 1 for a, b in zip(player_pixel, expected_pixel)),
                    'relocated Player differs from the same saved source sRGB Play pixels')
            proof['same_project_workflow']['matching_relocated_player_pixels'] = True
            from LinuxAuthoredPreview import screenshot
            player_capture = evidence / 'workflow-relocated-player.png'
            screenshot(display, int(player_window), player_capture, (960, 640))
            proof['captures'].append({'file': player_capture.name,
                                      'sha256': hashlib.sha256(player_capture.read_bytes()).hexdigest()})
        request_window_close(player_window, env)
        stdout, stderr = process.communicate(timeout=15)
        (evidence / 'player.stdout.log').write_bytes(stdout)
        (evidence / 'player.stderr.log').write_bytes(stderr)
        require(process.returncode == 0, 'relocated Player shutdown failed')
        report = json.loads(stdout)
        require(report['status'] == 'RENDERED_GAME_PACKAGE' and report['presented_frames'] > 1 and
                report['sprite_draw_calls'] > 1 and report['visible_sprites'] == 1 and report['drained'],
                'edited native Player frame/drain incomplete')
        require(not re.search(rb'Validation Error|VUID-|SYNC-HAZARD-', stderr), 'native Player validation failure')
        require(runtime_before == {str(p.relative_to(moved)): hashlib.sha256(p.read_bytes()).hexdigest()
                                   for p in moved.rglob('*') if p.is_file()}, 'Player changed runtime package')
        process = None
        proof.update(edited_assets_built=True, exact_edited_clip_packaged=True,
                     source_deleted=not root.exists(), unicode_relocated=True,
                     native_player_pixel=player_pixel, player_report=report,
                     runtime_hashes=runtime_before)
        proof.update(passed=True, before_pixel=before_pixel, edited_pixel=after_pixel,
                     reopened_pixel=reopened_pixel, clip_frames=frames,
                     atlas_sha256=hashlib.sha256(edited_atlas).hexdigest(),
                     clip_sha256=hashlib.sha256(edited_clip).hexdigest(),
                     scene_unchanged=True, sidecars_unchanged=True, readonly_project_bytes_unchanged=True)
    finally:
        if driver and process and process.poll() is None:
            process.terminate()
        if process:
            try:
                stdout, stderr = process.communicate(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                stdout, stderr = process.communicate()
            (evidence / 'failure.stdout').write_bytes(stdout or b'')
            (evidence / 'failure.stderr').write_bytes((bytes(driver.stderr) if driver else b'') + (stderr or b''))
        if xvfb:
            xvfb.terminate()
            xvfb.communicate(timeout=10)
        (evidence / 'result.json').write_text(json.dumps(proof, indent=2) + '\n')
    print(json.dumps(proof))


if __name__ == '__main__':
    main()
