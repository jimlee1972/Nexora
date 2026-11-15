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


def expected_mask(width, height, phase, pivot=2):
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
        low, high = (-2, 0) if second else (1 - pivot, 3 - pivot)
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
    scenario = parser.add_mutually_exclusive_group()
    scenario.add_argument('--reimport', action='store_true')
    scenario.add_argument('--reload', action='store_true')
    args = parser.parse_args()
    evidence = Path(args.evidence_dir).resolve() / ('source-services-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    root = Path(tempfile.mkdtemp(prefix='nexora-editor-source-services-'))
    proof = dict(passed=False, scope='configured Editor SDK source/animation/pool adoption',
                 readonly=not args.reimport, phases=[], captures=[], commands=[], reimports=[], reloads=[],
                 rejected_reloads=[])
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
        if args.reimport:
            layout = layout.replace(
                '[Window][Game###nexora.game]\nPos=640,40\nSize=620,640\nCollapsed=0',
                '[Window][Game###nexora.game]\nPos=640,40\nSize=620,430\nCollapsed=0').replace(
                '[Window][Content###nexora.content]\nPos=0,700\nSize=100,20\nCollapsed=1',
                '[Window][Content###nexora.content]\nPos=640,480\nSize=620,200\nCollapsed=0')
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
        def authored_hashes():
            # Writer locks, recovery/cache and saved UI layout are not authored game inputs.
            return {path: value for path, value in hashes(project).items()
                    if path in ('project.nexora', 'project.build.json', 'Title.scene', 'Level.scene',
                                '.nexora/scenes/Main.scene') or
                    path.startswith(('Content/', 'Scripts/'))}

        original = authored_hashes() if args.reimport or args.reload else hashes(project)
        server, display = start_xvfb(args.xvfb, '1280x720x24')
        require(display is not None, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display, NEXORA_PACKAGED_SPRITE_UI_TEST_EVIDENCE='1',
                   NEXORA_SCRIPT_RELOAD_EVIDENCE='1',
                   XDG_STATE_HOME=str(root / 'state'))
        if args.reimport:
            env.update(NEXORA_SCENE_SPRITE_UI_TEST_EVIDENCE='1',
                       NEXORA_PROJECT_ASSET_WORKFLOW_EVIDENCE='1')

        def launch(path, label):
            nonlocal editor, window
            log = evidence / (label + '.log')
            core = evidence / (label + '-core')
            command = [args.editor, '--graphical', '--native-scene-preview',
                       '--frames=100000', '--play', '--project=' + str(path),
                       '--recent-projects=' + str(root / (label + '-recent'))]
            if not args.reimport or label != 'sdk':
                command.append('--read-only')
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
        state = wait(lambda s: s.get('state') == '2', 'legacy Pause')
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
            require(current == authored and
                    (authored_hashes() if args.reimport or args.reload else hashes(project)) == original,
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
                if args.reload:
                    script = project / 'Scripts/game.zig'
                    template = script.read_text()
                    original_inputs = dict(original)
                    last_ticks = 0
                    identity = None
                    admission = re.compile(r'script reload admission: .*applied=(\d) generation=(\d+) ')

                    def save(code, fault=0, invalid=False):
                        nonlocal original
                        text = template.replace('fixture_generation: u32 = 0;',
                                                f'fixture_generation: u32 = {code};').replace(
                                                'fixture_fault: u32 = 0;', f'fixture_fault: u32 = {fault};')
                        if invalid:
                            text += '\nthis is an intentionally invalid Zig source\n'
                        temporary = root / 'saved-script.pending'
                        temporary.write_text(text)
                        os.replace(temporary, script)
                        original = authored_hashes()
                        require({k: v for k, v in original.items() if k != 'Scripts/game.zig'} ==
                                {k: v for k, v in original_inputs.items() if k != 'Scripts/game.zig'},
                                'a saved script edit changed another authored input')

                    def rendered():
                        require(source_mask(native_pixels(display, window, viewport)) == wanted,
                                'reload changed independently predicted source/pool pixels')
                        current = native_pixels(display, window, viewport)
                        require(all(current[i:i + 3] == baseline[i:i + 3] for i in center),
                                'reload changed independent3D pixels')
                        require(scene_region_pixels(display, window, tuple(map(int, rect))) == authored
                                and authored_hashes() == original,
                                'reload changed authoring pixels or saved inputs')

                    def live(code, accepted):
                        nonlocal last_ticks, identity
                        pattern = re.compile(
                            rf'MAIN SOURCE LIVE code={code} reloads=(\d+) ticks=(\d+) '
                            r'scene=(\d+) ids=(\d+,\d+) pool=(\d+:\d+:\d+) phase=(\d+)')
                        wait(lambda _: pattern.search(core_text()) is not None and
                             admission.findall(log.read_text())[-1:] == [('1', str(accepted + 1))],
                             f'actual SDK generation{code} not published/dispatched', timeout=90)
                        match = pattern.search(core_text())
                        count, ticks, scene, ids, pool, current_phase = match.groups()
                        require(int(count) == accepted and int(ticks) > last_ticks and current_phase == '0',
                                'counter/phase state was reset or candidate was mistaken for live')
                        current_identity = (scene, ids, pool)
                        if identity is None:
                            identity = current_identity
                        require(current_identity == identity, 'scene/entity/pool identity changed')
                        last_ticks = int(ticks)
                        rendered()
                        proof['reloads'].append(dict(code=code, accepted=accepted, ticks=int(ticks),
                                                     scene=scene, ids=ids, pool=pool,
                                                     runtime_generation=accepted + 1))

                    # A candidate-hook log is provisional. Only published Runtime admission plus
                    # the first dispatched Update and independent native pixels count as acceptance.
                    live(0, 0)
                    for code, fault, invalid in [(101, 0, True), (102, 1, False), (103, 2, False),
                                                  (104, 3, False), (106, 6, False), (107, 7, False)]:
                        count = len(admission.findall(log.read_text()))
                        save(code, fault, invalid)
                        wait(lambda _: len(admission.findall(log.read_text())) > count,
                             f'failure mode{fault} did not finish', timeout=90)
                        require(admission.findall(log.read_text())[-1] == ('0', '1') and
                                f'MAIN SOURCE LIVE code={code} ' not in core_text(),
                                'failed candidate published or dispatched')
                        rendered()
                        proof['rejected_reloads'].append(dict(code=code, fault=fault, compile=invalid))
                    # Save a new authored generation during ordinary background compilation.
                    # The obsolete worker result must never be dispatched.
                    progress_count = log.read_text().count('script reload progress:')
                    save(200)
                    wait(lambda _: log.read_text().count('script reload progress:') > progress_count,
                         'obsolete generation never entered the background compiler', timeout=90)
                    save(1)
                    live(1, 1)
                    require('MAIN SOURCE LIVE code=200 ' not in core_text(), 'obsolete result dispatched')
                    proof['obsolete_result_discarded'] = True
                    for code in range(2, 33):
                        save(code)
                        live(code, code)
                        if code % 8 == 0:
                            screenshot(display, window, evidence / f'reload-{code}.png')
                    # The old module itself attempts cached writes during Capture. This invalidates
                    # the read-only capture guard and must reject without changing its live state.
                    save(33, 4)
                    live(33, 33)
                    count = len(admission.findall(log.read_text()))
                    save(34)
                    wait(lambda _: len(admission.findall(log.read_text())) > count,
                         'Capture write did not finish', timeout=90)
                    require(admission.findall(log.read_text())[-1] == ('0', '34') and
                            'MAIN SOURCE LIVE code=34 ' not in core_text() and
                            'stage=capture' in core_text(), 'Capture write escaped quarantine')
                    rendered()
                    proof['rejected_reloads'].append(dict(code=34, fault='old Capture write'))
                    finalizers = re.findall(r'MAIN SOURCE CALLBACK code=(\d+) stage=finalizer '
                                           r'entity=(-?\d+) pool=(-?\d+) world=(-?\d+)', core_text())
                    require(all(any(int(row[0]) == code for row in finalizers) for code in range(33)),
                            'real retired DLL finalizers were not executed for every generation')
                    proof['retired_library_finalizers'] = finalizers
                    proof['actual_native_replacements'] = 33
                    proof['state_codec_payload_bytes'] = 80
                    state = wait(lambda s: s.get('captured') == '1', 'reload input capture lost')
                if args.reimport:
                    def observe(prefix):
                        lines = re.findall(rb'^' + prefix.encode() + rb' (.*)$', log.read_bytes(), re.M)
                        return dict(re.findall(r'(\w+)=([^ ]*)', lines[-1].decode())) if lines else {}

                    def send(*arguments):
                        subprocess.run([args.xdotool, *map(str, arguments)], env=env,
                                       check=True, timeout=3)

                    def control(name):
                        wait(lambda _: name in observe('scene sprite ui'), 'Content ' + name)
                        point = observe('scene sprite ui')[name]
                        stable = time.monotonic()
                        deadline = stable + 10
                        while time.monotonic() < deadline:
                            require(editor.poll() is None, 'Editor exited positioning ' + name)
                            current = observe('scene sprite ui').get(name)
                            if current != point:
                                point, stable = current, time.monotonic()
                            if point and time.monotonic() - stable >= .4:
                                return tuple(round(float(v)) for v in point.split(','))
                            time.sleep(.02)
                        raise RuntimeError('unstable Content control ' + name)

                    def click(point, button=1):
                        send('mousemove', '--window', window, *point)
                        time.sleep(.05)
                        send('click', button)
                        time.sleep(.1)

                    def search(text):
                        click(control('search'))
                        key('ctrl+a')
                        send('type', '--clearmodifiers', '--delay', '5', text)
                        time.sleep(.15)

                    def reimport(kind, accepted, failed=False):
                        search('Color' if kind == 'atlas' else 'Source.nx2sprite')
                        click(control('atlaspoint' if kind == 'atlas' else 'sourcepoint'), button=3)
                        click(control('reimport'))
                        if failed:
                            expected_error = 'asset source is unavailable or is a symlink'.encode().hex()
                            wait(lambda _: observe('project asset workflow').get('busy') == '0' and
                                 (observe('project asset workflow').get('error_hex') == expected_error
                                  if failed == 'missing' else
                                  observe('project asset workflow').get('reimport_state') == '6'),
                                 'failed source reimport did not retain last-good Content')
                        else:
                            wait(lambda s: s.get('reimports') == str(accepted) and
                                 observe('project asset workflow').get('busy') == '0',
                                 'source reimport did not publish at native frame boundary')

                    key('Escape')
                    wait(lambda s: s.get('captured') == '0', 'release Game input for Content')
                    for cycle in range(1, 33):
                        variant = cycle % 2
                        for kind, count in [('atlas', 2 * cycle - 1), ('source', 2 * cycle)]:
                            command = [args.fixture, '--reimport-' + kind, str(project), str(variant)]
                            proof['commands'].append(command)
                            subprocess.run(command, check=True, capture_output=True, timeout=10)
                            reimport(kind, count)
                        wanted = expected_mask(viewport[2], viewport[3], 0, pivot=2 + variant)
                        if variant:
                            wanted = bytes(0 if v == 0 else ((v - 1) ^ 1) + 1 for v in wanted)
                        wait(lambda _: source_mask(native_pixels(display, window, viewport)) == wanted,
                             'independent live reimport pixels')
                        actual = native_pixels(display, window, viewport)
                        require(source_mask(actual) == wanted and
                                all(actual[i:i + 3] == baseline[i:i + 3] for i in center),
                                'reimport pixels unstable or independent3D changed')
                        inputs = authored_hashes()
                        require(all(inputs[path] == value for path, value in original.items()
                                    if path not in ('Content/Color.nx2atlas', 'Content/Source.nx2sprite')),
                                'reimport changed authored scenes/settings/scripts/sidecars')
                        proof['reimports'].append(dict(cycle=cycle, publications=2 * cycle,
                                                       pivot=2 + variant, alternate=bool(variant),
                                                       mask_sha256=hashlib.sha256(wanted).hexdigest(),
                                                       independent_3d_pixels=len(center)))
                        if cycle in (1, 32):
                            screenshot(display, window, evidence / f'reimport-{cycle}.png')
                    require(authored_hashes() == original, 'restored reimports changed saved inputs')
                    for kind, count in [('atlas', 65), ('source', 66)]:
                        subprocess.run([args.fixture, '--reimport-' + kind, str(project), '1'],
                                       check=True, capture_output=True, timeout=10)
                        reimport(kind, count)
                    output = root / 'reimported Build 來源'
                    command = [args.editor, '--build-game=' + str(project),
                               '--output=' + str(output), '--script-project=Scripts/project.json',
                               '--build-records=' + str(evidence / 'reimport-build-records')]
                    proof['commands'].append(command)
                    built = subprocess.run(command, capture_output=True, text=True, timeout=120)
                    (evidence / 'reimport-build.stdout.log').write_text(built.stdout)
                    (evidence / 'reimport-build.stderr.log').write_text(built.stderr)
                    require(built.returncode == 0 and 'PUBLISHED_GAME' in built.stdout,
                            'Build did not publish reimported saved sources: ' + built.stderr[-4096:])
                    command = [args.fixture, '--verify-reimported-package', str(output / 'Game.nxgame')]
                    proof['commands'].append(command)
                    subprocess.run(command, check=True, capture_output=True, timeout=10)
                    proof['reimported_sdk_build_verified'] = True
                    for kind, count in [('atlas', 67), ('source', 68)]:
                        subprocess.run([args.fixture, '--reimport-' + kind, str(project), '0'],
                                       check=True, capture_output=True, timeout=10)
                        reimport(kind, count)
                    atlas_path = project / 'Content/Color.nx2atlas'
                    valid_atlas = atlas_path.read_bytes()
                    atlas_path.write_bytes(b'corrupt Atlas')
                    reimport('atlas', 68, failed=True)
                    wait(lambda s: s.get('reimports') == '68', 'failed import published new assets')
                    require(source_mask(native_pixels(display, window, viewport)) == wanted,
                            'corrupt reimport replaced the live native frame')
                    atlas_path.write_bytes(valid_atlas)
                    reimport('atlas', 69)
                    require(source_mask(native_pixels(display, window, viewport)) == wanted,
                            'same-byte reimport changed native pixels')
                    missing = project / 'missing-atlas'
                    atlas_path.rename(missing)
                    reimport('atlas', 69, failed='missing')
                    require(source_mask(native_pixels(display, window, viewport)) == wanted and
                            observe('packaged sprite ui').get('reimports') == '69',
                            'missing reimport lost the published generation')
                    missing.rename(atlas_path)
                    reimport('atlas', 70)
                    search('')
                    click((x + width // 2, y + height // 2))
                    state = wait(lambda s: s.get('captured') == '1', 'reacquire Game input')
                    proof.update(reimport_publications=70, corrupt_and_missing_retained=True,
                                 half_frame_clock_preserved=True)
        if args.reload:
            progress_count = log.read_text().count('script reload progress:')
            admissions_before_stop = len(admission.findall(log.read_text()))
            save(300)
            wait(lambda _: log.read_text().count('script reload progress:') > progress_count,
                 'Stop cancellation did not enter background compilation', timeout=90)
        key('F5')
        wait(lambda s: s.get('state') == '0' and s.get('module') == '0' and
             s.get('textures') == '0' and s.get('visible') == '0', 'Stop retained owner')
        if args.reload:
            require(len(admission.findall(log.read_text())) == admissions_before_stop and
                    'MAIN SOURCE LIVE code=300 ' not in core_text(), 'stopped compiler result dispatched')
            proof['stop_cancelled_compilation'] = True
        close(log)
        text = core_text()
        require(re.search(r'script reload admission: .*applied=1 generation=1 ', log.read_text()),
                'saved ScriptProject did not admit its compiled initial generation')
        require(re.search(r'MAIN SOURCE STOP ticks=[1-9]\d* phase=4 pool_destroy=0', text) and
                re.search(r'MAIN SOURCE DESTROY ticks=[1-9]\d* phase=4', text), 'SDK lifecycle cleanup')
        identities = re.findall(r'MAIN SOURCE PHASE \d.*pool=(\d+:\d+:\d+)', text)
        require(len(identities) == 5 and len(set(identities)) == 1, 'pool handle changed')
        require((authored_hashes() if args.reimport or args.reload else hashes(project)) == original,
                'Play changed saved authored inputs')
        if args.reimport:
            reopened_log, reopened_core, reopened_wait = launch(project, 'reopened')
            reopened = reopened_wait(lambda s: s.get('module') == '1' and s.get('visible') == '3' and
                                     s.get('reimports') == '0' and 'viewport' in s and
                                     'MAIN SOURCE PHASE 0' in reopened_core(),
                                     'saved source/Clip/pool reopening failed', timeout=90)
            rect = tuple(map(int, reopened['viewport'].split(',')))
            reopened_wait(lambda _: source_mask(native_pixels(display, window, rect)) ==
                          expected_mask(rect[2], rect[3], 0), 'reopened saved source native pixels')
            key('F5')
            reopened_wait(lambda s: s.get('state') == '0' and s.get('module') == '0',
                          'reopened Stop did not drain')
            close(reopened_log)
            require(authored_hashes() == original, 'reopening changed authored sources')
            proof['saved_reopen_verified'] = True
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
