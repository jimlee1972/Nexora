#!/usr/bin/env python3
"""Actual main Build CLI, source deletion, Unicode relocation and native Player pixels."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / 'Tests/EditorImGui'), str(ROOT / 'Tests/TwoD')]
from LinuxDisplayAcceptance import start_xvfb, request_window_close
from HD2DAcceptance import NativeWindow
from SpriteSourcePlayerAcceptance import pixels


def require(value, text):
    if not value:
        raise RuntimeError(text)


def hashes(root):
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(root.rglob('*')) if p.is_file()}


def oracle(path, baseline):
    width, height, sample = pixels(path)
    require((width, height) == (960, 640), 'native Player extent changed')
    colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0)]
    # Fixed independent screen expectations: trim [-1,1]x[-2,0], default vertical20,
    # entities x=-4/+4, second X scale -1. No production resolver computes this oracle.
    for x, y, color in [(336, 336, 0), (368, 336, 1), (336, 368, 2), (368, 368, 3),
                        (592, 336, 1), (624, 336, 0), (592, 368, 3), (624, 368, 2)]:
        require(all(abs(a - b) <= 8 for a, b in zip(sample(x, y), colors[color])),
                f'crop/Pivot/mirror color mismatch at {x},{y}: {sample(x,y)}')
    counts = [0] * 4
    for y in range(height):
        for x in range(width):
            rgb = sample(x, y)
            for i, color in enumerate(colors):
                if all(abs(a - b) <= 8 for a, b in zip(rgb, color)):
                    require(320 <= y < 384 and (320 <= x < 384 or 576 <= x < 640),
                            f'Sprite pixels outside independent trim bounds at {x},{y}')
                    counts[i] += 1
    require(counts == [2048] * 4, f'incomplete exact color masks: {counts}')
    _, _, legacy = pixels(baseline)
    require(all(sample(x, y) == legacy(x, y) for y in range(280, 360) for x in range(440, 520)),
            '3D mesh pixels changed under the Sprite overlay')
    return counts


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'player', 'fixture', 'xvfb', 'xdotool', 'cmake', 'ninja', 'zig'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    evidence = Path.cwd() / 'artifacts/editor-sprite-sources' / ('build-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    project = evidence / 'project 來源'
    output = evidence / 'built game'
    moved = evidence / 'moved 遊戲'
    proof = {'passed': False, 'scope': 'main SceneOnly Game Build/source-capable native Player',
             'captures': [], 'commands': []}
    server = process = window = None
    try:
        command = [args.fixture, '--write-fixture', str(project)]
        proof['commands'].append(command)
        subprocess.run(command, check=True, capture_output=True, timeout=20)
        scripts = project / 'Scripts'
        scripts.mkdir()
        shutil.copyfile(ROOT / 'Tests/Editor/GameSpriteBuild.zig', scripts / 'game.zig')
        (scripts / 'project.json').write_text(
            '{"schema":1,"module_name":"SpriteBuildFixture","script":"game.zig"}\n')
        before = hashes(project)
        for name in ('Title.scene', 'Level.scene'):
            shutil.copyfile(project / name, evidence / name)
        command = [args.editor, '--build-game=' + str(project), '--output=' + str(output),
                   '--runtime-profile=' + str(Path(args.player).parent / 'GameRuntime.profile'),
                   '--build-records=' + str(evidence / 'records'),
                   '--script-project=Scripts/project.json', '--cmake=' + args.cmake,
                   '--ninja=' + args.ninja, '--zig=' + args.zig]
        proof['commands'].append(command)
        built = subprocess.run(command, capture_output=True, text=True, timeout=90)
        (evidence / 'build.stdout.log').write_text(built.stdout)
        (evidence / 'build.stderr.log').write_text(built.stderr)
        require(built.returncode == 0, 'main Build failed: ' + built.stderr)
        require(json.loads(built.stdout)['status'] == 'PUBLISHED_GAME', 'Build report incomplete')
        require(before == hashes(project), 'Build changed authored files')
        proof['project_before'] = before
        proof['project_after'] = hashes(project)
        legacy_project = evidence / 'legacy project'
        shutil.copytree(project, legacy_project)
        for name in ('Title.scene', 'Level.scene'):
            path = legacy_project / name
            path.write_bytes(b''.join(line for line in path.read_bytes().splitlines(keepends=True)
                                     if b'4991205891477078017 \"editor.sprite.source\"' not in line))
        legacy_output = evidence / 'legacy game'
        legacy_command = [args.editor, '--build-game=' + str(legacy_project),
            '--output=' + str(legacy_output),
            '--runtime-profile=' + str(Path(args.player).parent / 'GameRuntime.profile'),
            '--build-records=' + str(evidence / 'legacy records'), '--scene-only']
        proof['commands'].append(legacy_command)
        legacy_built = subprocess.run(legacy_command, capture_output=True, text=True, timeout=90)
        (evidence / 'legacy-build.stdout.log').write_text(legacy_built.stdout)
        (evidence / 'legacy-build.stderr.log').write_text(legacy_built.stderr)
        require(legacy_built.returncode == 0, legacy_built.stderr)
        shutil.rmtree(legacy_project)
        gui_project = evidence / 'gui project'
        shutil.copytree(project, gui_project)
        (gui_project / '.nexora/scenes').mkdir(parents=True, exist_ok=True)
        shutil.copyfile(gui_project / 'Title.scene', gui_project / '.nexora/scenes/Main.scene')
        output.rename(moved)
        shutil.rmtree(project)
        executable = moved / 'Game'
        require(executable.is_file(), 'canonical runtime-only Game missing')
        initial = hashes(moved)
        require(not any(p.endswith(('.scene', '.nx2sprite', '.nx2atlas', '.meta')) for p in initial),
                'authored source escaped into runtime output')
        verified = subprocess.run([str(executable), '--verify-game-package', str(moved / 'Game.nxgame')],
                                  capture_output=True, text=True, timeout=15)
        require(verified.returncode == 0, verified.stderr)
        report = json.loads(verified.stdout)
        require(report['capability'] == 3 and report['scene_count'] == 2 and report['asset_count'] == 5,
                'source capability/shared closure missing')
        (evidence / 'verify.stdout.log').write_text(verified.stdout)
        server, display = start_xvfb(args.xvfb, '1024x768x24')
        require(display is not None, 'Xvfb unavailable')
        env = dict(os.environ, DISPLAY=display, NEXORA_LOG_DIR=str(evidence / 'game-logs'))
        gui_before = hashes(gui_project)
        gui_command = [args.editor, '--graphical', '--project=' + str(gui_project),
                       '--read-only', '--play', '--frames=12',
                       '--recent-projects=' + str(evidence / 'gui-recent')]
        proof['commands'].append(gui_command)
        gui = subprocess.run(gui_command, env=dict(env, NEXORA_SPRITE_BUILD_UI_TEST_EVIDENCE='1'),
                             capture_output=True, text=True, timeout=30)
        (evidence / 'editor-play.stdout.log').write_text(gui.stdout)
        (evidence / 'editor-play.stderr.log').write_text(gui.stderr)
        require(gui.returncode == 0 and 'sprite package Play rejected: state=0' in gui.stderr,
                'unsupported configured Play was not rejected gracefully: ' + gui.stderr)
        require('graphical evidence: acquired=12 presented=12' in gui.stderr and 'pie_ticks=0' in gui.stderr,
                'unsupported configured Play did not retain the actual stopped native Editor')
        require(gui_before == hashes(gui_project), 'unsupported Play changed authored files')
        proof['configured_play_rejected'] = True
        shutil.rmtree(gui_project)
        window = NativeWindow(display)
        def native(exe, tag, source):
            nonlocal process
            captured = []
            baseline = evidence / 'legacy.png'
            with (evidence / (tag + '.stdout.log')).open('w') as out, (evidence / (tag + '.stderr.log')).open('w') as err:
                # No arguments: executable-relative Game.launch resolves the relocated package.
                process = subprocess.Popen([str(exe)], cwd=evidence, env=env, stdout=out, stderr=err)
                deadline = time.monotonic() + 35
                last_error = ''
                while time.monotonic() < deadline and process.poll() is None:
                    found = subprocess.run([args.xdotool, 'search', '--onlyvisible', '--name', '^Nexora Game$'],
                                           env=env, capture_output=True, text=True, timeout=5).stdout.splitlines()
                    if len(found) == 1:
                        window.window = int(found[0])
                        path = evidence / (tag + '.png')
                        capture = window.capture(path)
                        try:
                            if source:
                                counts = oracle(path, baseline)
                            else:
                                _, _, sample = pixels(path)
                                require(sum(abs(a-b) for a,b in zip(sample(480,320),sample(480,220))) > 20,
                                        'legacy native mesh not ready')
                                counts = []
                            captured.append(dict(capture, colors=counts, phase=tag))
                            break
                        except RuntimeError as error:
                            last_error = str(error)
                    time.sleep(.05)
                require(bool(captured), 'native pixels did not become ready: ' + last_error)
                second = evidence / (tag + '-stable.png')
                capture = window.capture(second)
                if source:
                    oracle(second, baseline)
                require(captured[0]['sha256'] == capture['sha256'], 'static source pixels changed')
                captured.append(dict(capture, phase=tag))
                request_window_close(str(window.window), env)
                process.wait(timeout=15)
            require(process.returncode == 0, 'native Player failed')
            report = json.loads((evidence / (tag + '.stdout.log')).read_text())
            require(report['status'] == 'RENDERED_GAME_PACKAGE' and report['presented_frames'] > 1 and
                    report['scene_draw_calls'] > 1 and report['drained'],
                    'native source presentation/drain incomplete')
            require(report['sprite_draw_calls'] > 1 and report['visible_sprites'] == 2 if source else
                    report['sprite_draw_calls'] == 0 and report['visible_sprites'] == 0,
                    'native Sprite count mismatch')
            if source:
                require(report['gameplay_loaded'] and report['gameplay_updates'] > 1,
                        'actual compiled Zig module did not update')
                stderr = (evidence / (tag + '.stderr.log')).read_text()
                require(all(marker in stderr for marker in ('SPRITE BUILD START',
                        'SPRITE BUILD UPDATE', 'SPRITE BUILD STOP ticks=', 'SPRITE BUILD DESTROY ticks=')),
                        'Zig lifecycle incomplete')
                counts = re.findall(r'SPRITE BUILD (?:STOP|DESTROY) ticks=(\d+)', stderr)
                require(len(counts) == 2 and counts[0] == counts[1] and int(counts[0]) > 0 and
                        stderr.index('SPRITE BUILD STOP') < stderr.index('SPRITE BUILD DESTROY'),
                        'Zig state/lifecycle order changed')
            require('Validation Error' not in (evidence / (tag + '.stderr.log')).read_text(), 'Vulkan validation error')
            proof['captures'].extend(captured)
            return report

        proof['legacy_report'] = native(legacy_output / 'Game', 'legacy', False)
        report = native(executable, 'native', True)
        require(initial == hashes(moved), 'native Player changed runtime package')
        proof.update(passed=True, report=report, runtime_hashes=initial, source_deleted=not project.exists())
        print('Main Game Build, Unicode relocation, deleted sources and exact 8192 native Sprite pixels passed')
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
