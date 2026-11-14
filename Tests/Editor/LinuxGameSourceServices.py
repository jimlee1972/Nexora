#!/usr/bin/env python3
"""Canonical main Game Build, live source/animation/pool APIs and real scene-transition pixels."""
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
from LinuxDisplayAcceptance import start_xvfb, request_window_close
from HD2DAcceptance import NativeWindow
from SpriteSourcePlayerAcceptance import pixels
from LinuxGameSpriteBuild import hashes


def require(value, message):
    if not value:
        raise RuntimeError(message)


def expectations(phase):
    # Independent fixture math: 32 screen pixels/unit, trim [-1,1] (first source)
    # or [-2,0] (second Pivot), y[-2,0]. No production resolver generates this oracle.
    sprites = [(-4, 1, False, phase not in (0, 4)), (4, -1, False, phase not in (0, 4))]
    if phase == 2:
        sprites += [(-4, 1, False, False), (4, -1, False, False)]
    sprites += [(0, -1, True, phase != 0)]
    expected = {}
    for index, (x, scale, flip, second) in enumerate(sprites):
        low, high = (-2, 0) if second else (-1, 1)
        if flip:
            low, high = -high, -low
        left = 480 + 32 * (x + min(scale * low, scale * high))
        top = 480 if index == len(sprites) - 1 else 320
        reverse = (scale < 0) != flip
        for y in range(top, top + 64):
            for px in range(left, left + 64):
                column = (px - left) // 32
                expected[y * 960 + px] = ((y - top) // 32) * 2 + (1 - column if reverse else column)
    return expected


def oracle(path, phase, legacy):
    width, height, sample = pixels(path)
    require((width, height) == (960, 640), 'native extent changed')
    expected = expectations(phase)
    counts = [0] * 4
    for y in range(height):
        for x in range(width):
            r, g, b = sample(x, y)
            actual = (3 if r >= 247 and g >= 247 and b <= 8 else
                      0 if r >= 247 and g <= 8 and b <= 8 else
                      1 if r <= 8 and g >= 247 and b <= 8 else
                      2 if r <= 8 and g <= 8 and b >= 247 else None)
            wanted = expected.get(y * width + x)
            require(actual == wanted, f'phase{phase} exact source/pool mask mismatch at {x},{y}: {sample(x,y)} vs {wanted}')
            if actual is not None:
                counts[actual] += 1
    require(all(sample(x, y) == legacy(x, y) for y in range(280, 360) for x in range(440, 520)),
            'independent central 80x80 3D pixels changed')
    require(sum(counts) == len(expected), 'incomplete source/pool mask')
    return counts


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'player', 'fixture', 'xvfb', 'xdotool', 'cmake', 'ninja', 'zig'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--shipping', action='store_true')
    args = parser.parse_args()
    evidence = Path.cwd() / 'artifacts/editor-sprite-sources' / ('game-services-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    project = evidence / 'project 來源'
    proof = dict(passed=False, shipping=args.shipping, scope='main Player source/animation/pool services; no Editor/reload adoption',
                 commands=[], captures=[], phases=[])
    server = process = window = None
    try:
        command = [args.fixture, '--write-animation-fixture', str(project)]
        proof['commands'].append(command)
        subprocess.run(command, check=True, capture_output=True, timeout=20)
        (project / 'Scripts').mkdir()
        shutil.copyfile(ROOT / 'Tests/Editor/GameSourceServices.zig', project / 'Scripts/game.zig')
        (project / 'Scripts/project.json').write_text(
            '{"schema":1,"module_name":"MainSourceServices","script":"game.zig"}\n')
        before = hashes(project)
        output = evidence / 'built game'
        command = [args.editor, '--build-game=' + str(project), '--output=' + str(output),
                   '--runtime-profile=' + str(Path(args.player).parent / 'GameRuntime.profile'),
                   '--build-records=' + str(evidence / 'records'), '--script-project=Scripts/project.json',
                   '--cmake=' + args.cmake, '--ninja=' + args.ninja, '--zig=' + args.zig]
        proof['commands'].append(command)
        built = subprocess.run(command, capture_output=True, text=True, timeout=90)
        (evidence / 'build.stdout.log').write_text(built.stdout)
        (evidence / 'build.stderr.log').write_text(built.stderr)
        require(built.returncode == 0, 'canonical main Game Build failed: ' + built.stderr)
        require(json.loads(built.stdout)['status'] == 'PUBLISHED_GAME' and hashes(project) == before,
                'Build report/source conservation failed')
        legacy_project = evidence / 'legacy project'
        shutil.copytree(project, legacy_project)
        settings_path = legacy_project / 'project.build.json'
        settings = json.loads(settings_path.read_text())
        settings_path.write_text(json.dumps({key: value for key, value in settings.items()
                                             if key in ('start_scene', 'scenes')} | {'schema': 1}))
        for name in ('Title.scene', 'Level.scene'):
            path = legacy_project / name
            path.write_bytes(b''.join(line for line in path.read_bytes().splitlines(keepends=True)
                                     if b'4991205891477078017 "editor.sprite.source"' not in line))
        legacy_output = evidence / 'legacy game'
        command = [args.editor, '--build-game=' + str(legacy_project), '--output=' + str(legacy_output),
                   '--runtime-profile=' + str(Path(args.player).parent / 'GameRuntime.profile'),
                   '--build-records=' + str(evidence / 'legacy records'), '--scene-only']
        proof['commands'].append(command)
        legacy_built = subprocess.run(command, capture_output=True, text=True, timeout=90)
        (evidence / 'legacy-build.stderr.log').write_text(legacy_built.stderr)
        require(legacy_built.returncode == 0, legacy_built.stderr)
        shutil.rmtree(legacy_project)
        moved = evidence / 'moved 遊戲'
        output.rename(moved)
        shutil.rmtree(project)
        original = hashes(moved)
        verified = subprocess.run([str(moved / 'Game'), '--verify-game-package', str(moved / 'Game.nxgame')],
                                  capture_output=True, text=True, timeout=15)
        require(verified.returncode == 0, verified.stderr)
        report = json.loads(verified.stdout)
        require((report['capability'], report['scene_count'], report['asset_count']) == (4, 2, 9),
                'main source/Clip catalog changed')
        proof.update(package=report, authored_hashes=before, source_deleted=not project.exists())
        server, display = start_xvfb(args.xvfb, '1024x768x24')
        require(display is not None, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display, NEXORA_LOG_DIR=str(evidence / 'game-logs'))
        window = NativeWindow(display)

        def log():
            return (evidence / 'game.stderr.log').read_text()

        def open_game(executable, tag):
            nonlocal process
            with (evidence / (tag + '.stdout.log')).open('w') as out, (evidence / (tag + '.stderr.log')).open('w') as err:
                process = subprocess.Popen([str(executable)], cwd=evidence, env=env, stdout=out, stderr=err)
            deadline = time.monotonic() + 35
            while time.monotonic() < deadline and process.poll() is None:
                result = subprocess.run([args.xdotool, 'search', '--onlyvisible', '--name', '^Nexora Game$'],
                                        env=env, capture_output=True, text=True, timeout=5)
                ids = result.stdout.splitlines()
                if len(ids) == 1:
                    window.window = int(ids[0])
                    return
                time.sleep(.05)
            raise RuntimeError('Native Game window unavailable: ' + tag)

        def close_game(tag):
            nonlocal process
            request_window_close(str(window.window), env)
            process.wait(timeout=15)
            require(process.returncode == 0, 'native game failed: ' + tag)
            value = json.loads((evidence / (tag + '.stdout.log')).read_text())
            require(value['drained'] and value['presented_frames'] > 1, 'native drain/presentation failed')
            process = None
            return value

        open_game(legacy_output / 'Game', 'legacy')
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline:
            capture = window.capture(evidence / 'legacy.png')
            _, _, sample = pixels(evidence / 'legacy.png')
            if sum(abs(a - b) for a, b in zip(sample(480, 320), sample(480, 220))) > 20:
                break
            time.sleep(.05)
        else:
            raise RuntimeError('independent 3D baseline not ready')
        proof['legacy'] = close_game('legacy')
        _, _, legacy = pixels(evidence / 'legacy.png')
        open_game(moved / 'Game', 'game')
        subprocess.run([args.xdotool, 'windowfocus', '--sync', str(window.window)], env=env,
                       check=True, timeout=5)
        for phase, key in enumerate((None, 'd', 'w', 's', 'a')):
            deadline = time.monotonic() + 12
            if key:
                subprocess.run([args.xdotool, 'keydown', key], env=env, check=True, timeout=5)
                time.sleep(.15)
                subprocess.run([args.xdotool, 'keyup', key], env=env, check=True, timeout=5)
            previous = None
            last_error = ''
            while time.monotonic() < deadline:
                require(process.poll() is None, 'SDK game exited during phase: ' + log())
                if f'MAIN SOURCE PHASE {phase}' in log():
                    path = evidence / f'phase-{phase}.png'
                    captured = window.capture(path)
                    try:
                        counts = oracle(path, phase, legacy)
                        if previous == captured['sha256']:
                            proof['phases'].append(dict(phase=phase, pixels=sum(counts), colors=counts))
                            proof['captures'].append(dict(captured, phase=phase))
                            break
                        previous = captured['sha256']
                    except RuntimeError as error:
                        last_error = str(error)
                time.sleep(.05)
            else:
                raise RuntimeError(f'phase{phase} did not reach stable exact pixels: {last_error}; {log()}')
        proof['native'] = close_game('game')
        error = log()
        require(re.search(r'MAIN SOURCE STOP ticks=[1-9]\d* phase=4 pool_destroy=0', error) and
                re.search(r'MAIN SOURCE DESTROY ticks=[1-9]\d* phase=4', error), 'SDK Stop/Destroy/pool cleanup missing')
        identities = re.findall(r'MAIN SOURCE PHASE \d.*pool=(\d+:\d+:\d+)', error)
        require(len(identities) == 5 and len(set(identities)) == 1, 'live pool handle changed during transitions')
        require(not re.search(r'Validation Error|VUID-|SYNC-HAZARD-', error), 'native Vulkan error')
        require(proof['native']['visible_sprites'] == 3 and proof['native']['scene_commits'] == 4 and
                proof['native']['gameplay_loaded'] and proof['native']['gameplay_updates'] > 1,
                'main gameplay/scene/pool report incomplete')
        require(hashes(moved) == original, 'native game modified its relocated package')
        proof.update(passed=True, package_unchanged=True, pool_handle=identities[0])
        print('Main SDK source animation/pool, Additive/unload/Replace and five exact native phases passed')
    finally:
        if process and process.poll() is None:
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
