#!/usr/bin/env python3
"""Real main GUI Clip selection, saved/reopened SceneOnly/SDK Build and relocated native pixels."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'Tests/EditorImGui'), str(ROOT / 'Tests/TwoD')]
from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from NativeSceneInput import NativeSceneInput
from HD2DAcceptance import NativeWindow
from LinuxGameSpriteBuild import hashes, oracle
from SpriteSourcePlayerAcceptance import pixels


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'player', 'fixture', 'xvfb', 'xdotool', 'cmake', 'ninja', 'zig'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    evidence = Path.cwd() / 'artifacts/editor-sprite-sources' / ('clip-picker-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    project = evidence / 'project 來源'
    proof = {'passed': False, 'scope': 'graphical main animation root selection and asset Build; no playback services',
             'native_input': True, 'commands': [], 'captures': [], 'reports': {}}
    server = process = driver = window = None
    try:
        subprocess.run([args.fixture, '--write-animation-fixture', str(project)], check=True, timeout=20)
        settings_path = project / 'project.build.json'
        settings = json.loads(settings_path.read_text())
        roots = settings.pop('animation_clips')
        settings['schema'] = 2
        settings['build'] = {'mode': 'scene-only', 'output': 'Build/SelectedGame',
                             'script_manifest': 'Scripts/project.json'}
        settings_path.write_text(json.dumps(settings) + '\n')
        (project / 'Scripts').mkdir()
        shutil.copyfile(ROOT / 'Tests/Editor/GameSpriteBuild.zig', project / 'Scripts/game.zig')
        (project / 'Scripts/project.json').write_text(
            '{"schema":1,"module_name":"SpriteBuildFixture","script":"game.zig"}\n')
        (project / '.nexora/scenes').mkdir(parents=True, exist_ok=True)
        shutil.copyfile(project / 'Title.scene', project / '.nexora/scenes/Main.scene')
        authored = {name: digest for name, digest in hashes(project).items()
                    if name.startswith(('Content/', 'Scripts/')) or name in ('Title.scene', 'Level.scene')}
        initial_settings = settings_path.read_bytes()
        legacy_project = evidence / 'legacy project'
        shutil.copytree(project, legacy_project)
        for name in ('Title.scene', 'Level.scene'):
            path = legacy_project / name
            path.write_bytes(b''.join(line for line in path.read_bytes().splitlines(keepends=True)
                                     if b'4991205891477078017 "editor.sprite.source"' not in line))
        legacy_output = evidence / 'legacy game'
        command = [args.editor, '--build-game=' + str(legacy_project), '--output=' + str(legacy_output),
                   '--runtime-profile=' + str(Path(args.player).parent / 'GameRuntime.profile'),
                   '--build-records=' + str(evidence / 'legacy records'), '--scene-only']
        proof['commands'].append(command)
        built = subprocess.run(command, capture_output=True, text=True, timeout=90)
        (evidence / 'legacy-build.stdout.log').write_text(built.stdout)
        (evidence / 'legacy-build.stderr.log').write_text(built.stderr)
        require(built.returncode == 0, 'independent legacy Build failed: ' + built.stderr)
        shutil.rmtree(legacy_project)
        server, display = start_xvfb(args.xvfb, '1600x1000x24')
        require(display is not None, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display, XDG_STATE_HOME=str(evidence / 'state'),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1', NEXORA_PROJECT_GAME_BUILD_EVIDENCE='1')
        window = NativeWindow(display)

        def observation():
            matches = re.findall(rb'^game build ui ([^\n]*)\n', driver.stderr, re.M)
            return dict(part.split('=', 1) for part in matches[-1].decode().split()) if matches else {}

        def wait(predicate, message, seconds=8):
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline:
                driver.drain(.05)
                if predicate():
                    return
                require(process.poll() is None, 'Editor exited: ' + message)
            raise RuntimeError(message + '; actual Build GUI: ' + str(observation()))

        def point(key):
            previous, since = None, time.monotonic()
            def stable():
                nonlocal previous, since
                current = observation().get(key)
                if current != previous:
                    previous, since = current, time.monotonic()
                return current is not None and time.monotonic() - since >= .25
            wait(stable, 'visible control did not settle: ' + key)
            return tuple(map(float, previous.split(',')))

        def capture(tag):
            window.window = int(driver.window)
            proof['captures'].append(dict(window.capture(evidence / (tag + '.png')), phase=tag))

        def open_host(readonly=False):
            nonlocal process, driver
            process = launch(args.editor, project, evidence / 'recent', env, read_only=readonly, binary=True)
            native = wait_for_window(args.xdotool, env)
            driver = NativeSceneInput(process, args.xdotool, env, native, evidence, 2 if readonly else 1,
                                      ack_pointer=True)
            driver.raw('windowfocus', '--sync', native)
            driver.click(point('p0'))
            driver.click(point('p1'))
            point('clip_open')

        def close_host():
            nonlocal process, driver
            request_window_close(driver.window, env)
            driver.finish(15)
            require(process.returncode == 0, 'native Editor did not drain')
            process = driver = None

        def save(mode):
            driver.click(point('p5'))
            wait(lambda: json.loads(settings_path.read_text()).get('animation_clips') == roots and
                 json.loads(settings_path.read_text())['build']['mode'] == mode,
                 'Save did not publish the exact selected roots/mode')

        output = project / 'Build/SelectedGame'
        def build(tag):
            publications = driver.stderr.count(b'game build result: published')
            driver.click(point('p13'))
            wait(lambda: (output / 'Game.nxgame').is_file() and observation().get('busy') == '0' and
                 driver.stderr.count(b'game build result: published') > publications,
                 'graphical Build did not publish', 90)
            destination = evidence / ('moved 遊戲 ' + tag)
            shutil.copytree(output, destination)
            verified = subprocess.run([str(destination / 'Game'), '--verify-game-package',
                                       str(destination / 'Game.nxgame')], capture_output=True,
                                      text=True, timeout=15)
            (evidence / (tag + '-verify.stdout.log')).write_text(verified.stdout)
            require(verified.returncode == 0, verified.stderr)
            report = json.loads(verified.stdout)
            require(report['capability'] == 4 and report['scene_count'] == 2 and report['asset_count'] == 9,
                    'graphical Build did not preserve the exact Clip/source/Atlas closure')
            proof['reports'][tag + '_verify'] = report
            return destination

        open_host()
        driver.click(point('clip_open'))
        for count, asset in enumerate(roots, 1):
            driver.click(point('clip_add_' + asset))
            wait(lambda: observation().get('animation_count') == str(count), 'Clip selection was not applied')
        require(settings_path.read_bytes() == initial_settings, 'selection wrote settings before Save')
        capture('selected-unsaved')
        driver.click(point('clip_close'))
        save('scene-only')
        selected_settings = settings_path.read_bytes()
        # Actual Reload must conserve the saved roots before either Build.
        driver.click(point('p6'))
        wait(lambda: observation().get('animation_count') == '2', 'Reload lost selected roots')
        scene_only = build('scene-only')
        driver.click(point('p11'))
        save('script-project')
        # Require a new completed publication, not the prior SceneOnly ready observation.
        scripted = build('script')
        capture('script-built')
        close_host()
        before_readonly = hashes(project)
        open_host(True)
        require(json.loads(settings_path.read_text())['animation_clips'] == roots,
                'reopened project lost saved roots')
        driver.click(point('clip_open'))
        wait(lambda: len([key for key in observation() if key.startswith('clip_add_')]) == 3,
             'reopened typed Clip inventory incomplete')
        available = {key.removeprefix('clip_add_') for key in observation() if key.startswith('clip_add_')}
        unused = available - set(roots)
        require(len(unused) == 1, 'unselected Clip fixture missing')
        driver.click(point('clip_add_' + unused.pop()))
        driver.click(point('clip_remove_' + roots[0]))
        require(observation().get('animation_count') == '2', 'read-only controls changed the draft')
        capture('readonly-reopened')
        driver.click(point('clip_close'))
        driver.click(point('p5'))
        close_host()
        require(hashes(project) == before_readonly, 'read-only native reopen changed project bytes')
        after = hashes(project)
        require(all(after.get(name) == digest for name, digest in authored.items()),
                'graphical selection/Build changed authored sources')
        proof.update(selected_roots=roots, scene_only_settings=selected_settings.decode(),
                     final_settings=settings_path.read_text(), readonly_project_unchanged=True,
                     authored_source_hashes=authored)
        shutil.rmtree(project)

        def native(executable, tag, source, gameplay):
            nonlocal process
            initial = hashes(executable.parent)
            with (evidence / (tag + '.stdout.log')).open('w') as out, (evidence / (tag + '.stderr.log')).open('w') as err:
                process = subprocess.Popen([str(executable)], cwd=evidence, env=env, stdout=out, stderr=err)
                deadline = time.monotonic() + 35
                ready = False
                last_error = ''
                while time.monotonic() < deadline and process.poll() is None:
                    found = subprocess.run([args.xdotool, 'search', '--onlyvisible', '--name', '^Nexora Game$'],
                                           env=env, capture_output=True, text=True, timeout=5).stdout.splitlines()
                    if len(found) == 1:
                        window.window = int(found[0])
                        path = evidence / (tag + '.png')
                        captured = window.capture(path)
                        try:
                            if source:
                                oracle(path, evidence / 'legacy.png')
                            else:
                                _, _, sample = pixels(path)
                                require(sum(abs(a - b) for a, b in zip(sample(480, 320), sample(480, 220))) > 20,
                                        'independent legacy 3D not ready')
                            ready = True
                            proof['captures'].append(dict(captured, phase=tag))
                            break
                        except RuntimeError as error:
                            last_error = str(error)
                    time.sleep(.05)
                require(ready, 'relocated native pixels unavailable: ' + last_error)
                stable = window.capture(evidence / (tag + '-stable.png'))
                if source:
                    oracle(evidence / (tag + '-stable.png'), evidence / 'legacy.png')
                require(stable['sha256'] == captured['sha256'], 'native static Sprite/3D pixels changed')
                proof['captures'].append(dict(stable, phase=tag))
                request_window_close(str(window.window), env)
                process.wait(timeout=15)
            require(process.returncode == 0, 'native Player failed')
            report = json.loads((evidence / (tag + '.stdout.log')).read_text())
            require(report['status'] == 'RENDERED_GAME_PACKAGE' and report['drained'] and
                    report['presented_frames'] > 1 and report['scene_draw_calls'] > 1,
                    'native presentation/drain incomplete')
            require(report['visible_sprites'] == (2 if source else 0) and
                    report['gameplay_loaded'] == gameplay and
                    (report['gameplay_updates'] > 1 if gameplay else report['gameplay_updates'] == 0),
                    'native Sprite/gameplay mode mismatch')
            error = (evidence / (tag + '.stderr.log')).read_text()
            require(not re.search(r'Validation Error|VUID-|SYNC-HAZARD-', error), 'native Vulkan error')
            if gameplay:
                require(all(marker in error for marker in ('SPRITE BUILD START', 'SPRITE BUILD UPDATE',
                        'SPRITE BUILD STOP ticks=', 'SPRITE BUILD DESTROY ticks=')), 'SDK lifecycle missing')
            require(hashes(executable.parent) == initial, 'native game changed its package')
            proof['reports'][tag] = report
            process = None

        native(legacy_output / 'Game', 'legacy', False, False)
        native(scene_only / 'Game', 'scene-only', True, False)
        native(scripted / 'Game', 'script', True, True)
        proof.update(passed=True, source_deleted=not project.exists())
        print('Actual graphical Clip selection/Save/Reload/reopen, SceneOnly/SDK Build and relocated native masks passed')
    finally:
        if driver:
            driver.abort()
        elif process and process.poll() is None:
            process.kill()
            process.wait(timeout=5)
        if window:
            window.close()
        if server:
            server.terminate()
            server.wait(timeout=5)
        (evidence / 'receipt.json').write_text(json.dumps(proof, indent=2) + '\n')


if __name__ == '__main__':
    main()
