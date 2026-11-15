#!/usr/bin/env python3
"""Configured Editor compiles the saved SDK script and renders live source/pool state."""
import argparse
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
from LinuxAuthoredPreview import screenshot
from LinuxNativeScenePreview import scene_region_pixels
from LinuxPackagedSprites import hashes, native_pixels, require, source_mask

ROOT = Path(__file__).resolve().parents[2]


def expected_mask(width, height, phase):
    # Independent fixture math: vertical20, trim2x2, Pivot(2,3)/(3,3), PPU1.
    # The right entity has negative X scale; the pool has both negative scale and flip.
    require(height == 180 and width % 2 == 0, 'unexpected Game canvas geometry')
    unit = height // 20
    sprites = [(-4, 1, False, phase not in (0, 4), 0),
               (4, -1, False, phase not in (0, 4), 0)]
    if phase == 2:
        sprites += [(-4, 1, False, False, 0), (4, -1, False, False, 0)]
    sprites += [(0, -1, True, phase != 0, -5)]
    result = bytearray(width * height)
    for position, scale, flip, second, y_position in sprites:
        low, high = (-2, 0) if second else (-1, 1)
        if flip:
            low, high = -high, -low
        left = width // 2 + unit * (position + min(scale * low, scale * high))
        top = height // 2 - y_position * unit
        reverse = (scale < 0) != flip
        for y in range(2 * unit):
            for x in range(2 * unit):
                column = x // unit
                if reverse:
                    column = 1 - column
                result[(top + y) * width + left + x] = 1 + (y // unit) * 2 + column
    return bytes(result)


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool', 'evidence-dir'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    evidence = Path(args.evidence_dir).resolve() / ('source-services-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    root = Path(tempfile.mkdtemp(prefix='nexora-editor-source-services-'))
    proof = dict(passed=False, scope='configured Editor SDK source/animation/pool adoption',
                 readonly=True, phases=[], captures=[], commands=[])
    server = editor = window = None
    try:
        project = root / 'project 來源'
        command = [args.fixture, '--write-animation-fixture', str(project)]
        proof['commands'].append(command)
        subprocess.run(command, check=True, capture_output=True, timeout=20)
        (project / '.nexora/scenes').mkdir(parents=True, exist_ok=True)
        shutil.copyfile(project / 'Title.scene', project / '.nexora/scenes/Main.scene')
        (project / 'Scripts').mkdir()
        shutil.copyfile(ROOT / 'Tests/Editor/GameSourceServices.zig', project / 'Scripts/game.zig')
        (project / 'Scripts/project.json').write_text(
            '{"schema":1,"module_name":"EditorMainSourceServices","script":"game.zig"}\n')
        settings = json.loads((project / 'project.build.json').read_text())
        for index, entry in enumerate(settings['scenes'], 1):
            entry['id'] = f'00000000-0000-0001-0000-{index:012x}'
        settings['start_scene'] = settings['scenes'][0]['id']
        settings['build'] = dict(mode='script-project', script_manifest='Scripts/project.json',
                                 output='Build/Game')
        (project / 'project.build.json').write_text(json.dumps(settings))
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
        legacy = root / 'legacy'
        shutil.copytree(project, legacy)
        legacy_settings = json.loads((legacy / 'project.build.json').read_text())
        (legacy / 'project.build.json').write_text(json.dumps(
            {key: value for key, value in legacy_settings.items()
             if key in ('start_scene', 'scenes')} | dict(schema=1)))
        for scene in [legacy / 'Title.scene', legacy / 'Level.scene',
                      legacy / '.nexora/scenes/Main.scene']:
            scene.write_bytes(b''.join(line for line in scene.read_bytes().splitlines(keepends=True)
                                      if b'"editor.sprite.source"' not in line))
        original = hashes(project)
        server, display = start_xvfb(args.xvfb, '1280x720x24')
        require(display is not None, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display, NEXORA_PACKAGED_SPRITE_UI_TEST_EVIDENCE='1',
                   NEXORA_SCRIPT_RELOAD_EVIDENCE='1',
                   XDG_STATE_HOME=str(root / 'state'))

        def launch(path, label):
            nonlocal editor, window
            log = evidence / (label + '.log')
            core = evidence / (label + '-core')
            command = [args.editor, '--graphical', '--native-scene-preview', '--read-only',
                       '--frames=100000', '--play', '--project=' + str(path),
                       '--recent-projects=' + str(root / (label + '-recent'))]
            proof['commands'].append(command)
            with log.open('wb') as err, (evidence / (label + '.stdout.log')).open('wb') as out:
                editor = subprocess.Popen(command, env=dict(env, NEXORA_LOG_DIR=str(core)),
                                          stdout=out, stderr=err)
            window = int(wait_for_window(args.xdotool, env))
            subprocess.run([args.xdotool, 'windowfocus', str(window)], env=env,
                           check=True, timeout=3)

            def observation():
                data = log.read_bytes()
                require(len(data) < 4 * 1024 * 1024, 'native evidence exceeded4MiB')
                lines = re.findall(rb'^packaged sprite ui (.*)$', data, re.M)
                return dict(part.decode().split('=', 1) for part in lines[-1].split()) if lines else {}

            def core_text():
                return '\n'.join(p.read_text() for p in core.rglob('game*.log'))

            def wait(predicate, reason, timeout=15):
                until = time.monotonic() + timeout
                while time.monotonic() < until:
                    require(editor.poll() is None, 'Editor exited: ' + log.read_text()[-4096:])
                    state = observation()
                    if predicate(state):
                        return state
                    time.sleep(.03)
                raise RuntimeError(reason + ': ' + log.read_text()[-2048:] + core_text()[-4096:])

            wait(lambda s: s.get('focused') == '1' and s.get('admitted') == '1' and
                 b'native scene viewport:' in log.read_bytes(), 'processed input readiness')
            return log, core_text, wait

        def key(name):
            subprocess.run([args.xdotool, *physical_key_arguments(('keydown', name), env)],
                           env=env, check=True, timeout=3)
            time.sleep(.15)
            subprocess.run([args.xdotool, *physical_key_arguments(('keyup', name), env)],
                           env=env, check=True, timeout=3)

        def close(log):
            request_window_close(str(window), env)
            editor.wait(timeout=15)
            require(editor.returncode == 0, 'native shutdown failed: ' + log.read_text()[-4096:])
            text = log.read_text()
            require(not re.search(r'Validation Error|VUID-|SYNC-HAZARD-', text), 'Vulkan validation')
            require(re.search(r'graphical evidence: .*ui_rejected=0 ', text), 'stale UI textures')

        log, _, wait = launch(legacy, 'legacy')
        state = wait(lambda s: s.get('state') == '1' and 'viewport' in s, 'legacy Game missing')
        key('F6')
        wait(lambda s: s.get('state') == '2', 'legacy Pause')
        viewport = tuple(map(int, state['viewport'].split(',')))
        time.sleep(.15)
        baseline = native_pixels(display, window, viewport)
        screenshot(display, window, evidence / 'legacy.png')
        close(log)
        log, core_text, wait = launch(project, 'sdk')
        authored = None
        for phase, control in enumerate((None, 'd', 'w', 's', 'a')):
            if control:
                require(state.get('captured') == '1', 'Game input capture was revoked')
                key(control)
            commits = (1, 1, 2, 3, 4)[phase]
            state = wait(lambda s: s.get('state') == '1' and s.get('module') == '1' and
                         s.get('commits') == str(commits) and 'viewport' in s and
                         f'MAIN SOURCE PHASE {phase}' in core_text(),
                         f'SDK phase{phase} missing', timeout=90 if phase == 0 else 15)
            require(state['visible'] == str(5 if phase == 2 else 3), 'live source/pool count')
            require(tuple(map(int, state['viewport'].split(','))) == viewport, 'Game canvas moved')
            wanted = expected_mask(viewport[2], viewport[3], phase)
            wait(lambda _: source_mask(native_pixels(display, window, viewport)) == wanted,
                 f'phase{phase} source/Pivot/pool pixels')
            actual = native_pixels(display, window, viewport)
            require(source_mask(actual) == wanted, 'unstable phase pixels')
            width, height = viewport[2:]
            center = [(y * width + x) * 3 for y in range(height // 2 + 4, height // 2 + 10)
                      for x in range(width // 2 - 3, width // 2 + 3)]
            require(all(actual[i:i + 3] == baseline[i:i + 3] for i in center),
                    'independently rendered central3D pixels changed')
            require(baseline[center[0]:center[0] + 3] != baseline[:3], '3D baseline is background')
            rect = re.findall(rb'native scene viewport: (\d+) (\d+) (\d+) (\d+)', log.read_bytes())[-1]
            current = scene_region_pixels(display, window, tuple(map(int, rect)))
            if authored is None:
                authored = current
            require(current == authored and hashes(project) == original,
                    'gameplay changed readonly authoring pixels/files')
            screenshot(display, window, evidence / f'phase-{phase}.png')
            proof['phases'].append(dict(phase=phase, commits=commits,
                                        visible=int(state['visible']), viewport=viewport,
                                        colors=[wanted.count(i) for i in range(1, 5)],
                                        mask_sha256=hashlib.sha256(wanted).hexdigest(),
                                        independent_3d_pixels=len(center)))
            if phase == 0:
                x, y, width, height = viewport
                subprocess.run([args.xdotool, 'mousemove', '--window', str(window),
                                str(x + width // 2), str(y + height // 2)],
                               env=env, check=True, timeout=3)
                time.sleep(.1)
                subprocess.run([args.xdotool, 'click', '1'], env=env, check=True, timeout=3)
                state = wait(lambda s: s.get('captured') == '1', 'Game input acquisition')
                # The acquisition batch is deliberately discarded by the product input policy.
                time.sleep(.1)
        key('F5')
        wait(lambda s: s.get('state') == '0' and s.get('module') == '0' and
             s.get('textures') == '0' and s.get('visible') == '0', 'Stop retained owner')
        close(log)
        text = core_text()
        require(re.search(r'script reload admission: .*applied=1 generation=1 ', log.read_text()),
                'saved ScriptProject did not admit its compiled initial generation')
        require(re.search(r'MAIN SOURCE STOP ticks=[1-9]\d* phase=4 pool_destroy=0', text) and
                re.search(r'MAIN SOURCE DESTROY ticks=[1-9]\d* phase=4', text), 'SDK lifecycle cleanup')
        identities = re.findall(r'MAIN SOURCE PHASE \d.*pool=(\d+:\d+:\d+)', text)
        require(len(identities) == 5 and len(set(identities)) == 1, 'pool handle changed')
        require(hashes(project) == original, 'Play changed saved source/settings/layout bytes')
        proof.update(passed=True, source_hashes=original, pool_handle=identities[0],
                     sdk_compiled_from_saved_settings=True, authoring_unchanged=True)
        print('Configured Editor SDK source animation/pool and five native scene phases passed')
    finally:
        if editor and editor.poll() is None:
            if window is not None:
                try:
                    screenshot(display, window, evidence / 'failure.png')
                except Exception as error:
                    (evidence / 'screenshot-error.log').write_text(str(error))
            editor.kill()
            editor.wait(timeout=5)
        if server:
            server.terminate()
            server.wait(timeout=5)
        proof['captures'] = [dict(file=p.name, sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                             for p in sorted(evidence.glob('*.png'))]
        (evidence / 'receipt.json').write_text(json.dumps(proof, indent=2) + '\n')
        shutil.rmtree(root)


if __name__ == '__main__':
    main()
