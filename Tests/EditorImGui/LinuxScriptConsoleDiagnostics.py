#!/usr/bin/env python3
"""Observe real saved ScriptProject compiler diagnostics through actual native Play controls."""
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

from LinuxAuthoredPreview import screenshot
from LinuxDisplayAcceptance import request_window_close, start_xvfb, wait_for_window
from NativeSceneInput import NativeSceneInput


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool', 'evidence-dir'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--capture', action='store_true')
    args = parser.parse_args()
    evidence = Path(args.evidence_dir).resolve() / ('script-console-' + str(time.time_ns()))
    evidence.mkdir(parents=True)
    root = Path(tempfile.mkdtemp(prefix='nexora-script-console-'))
    project = root / 'project 來源'
    state = evidence / 'state'
    state.mkdir()
    server = process = driver = None
    proof = dict(passed=False, scope='actual ScriptProject compiler-to-Console route')
    try:
        subprocess.run([args.fixture, '--write-animation-fixture', str(project)], check=True,
                       capture_output=True, timeout=20)
        (project / '.nexora/scenes').mkdir(parents=True, exist_ok=True)
        shutil.copyfile(project / 'Title.scene', project / '.nexora/scenes/Main.scene')
        (project / 'Scripts').mkdir()
        (project / 'Scripts/game.zig').write_text('const unexpected = ;\n')
        (project / 'Scripts/project.json').write_text(
            '{"schema":1,"module_name":"ScriptConsoleDiagnostics","script":"game.zig"}\n')
        settings = json.loads((project / 'project.build.json').read_text())
        for index, entry in enumerate(settings['scenes'], 1):
            entry['id'] = f'00000000-0000-0001-0000-{index:012x}'
        settings['start_scene'] = settings['scenes'][0]['id']
        settings['build'] = dict(mode='script-project', script_manifest='Scripts/project.json',
                                 output='Build/Game')
        (project / 'project.build.json').write_text(json.dumps(settings))

        def authored():
            return {str(p.relative_to(project)): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in project.rglob('*') if p.is_file() and
                    (p.name in ('project.nexora', 'project.build.json', 'Title.scene', 'Level.scene')
                     or p.relative_to(project).as_posix() == '.nexora/scenes/Main.scene'
                     or p.relative_to(project).parts[0] in ('Content', 'Scripts'))}

        original = authored()
        server, display = start_xvfb(args.xvfb, '1280x720x24')
        require(display is not None, 'Xvfb did not become ready')
        env = dict(os.environ, DISPLAY=display, XDG_STATE_HOME=str(state),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1',
                   NEXORA_PACKAGED_SPRITE_UI_TEST_EVIDENCE='1',
                   NEXORA_SCRIPT_RELOAD_EVIDENCE='1',
                   NEXORA_SCRIPT_DIAGNOSTIC_TEST_EVIDENCE='1')
        command = [args.editor, '--graphical', '--native-scene-preview', '--frames=100000',
                   '--project=' + str(project), '--read-only',
                   '--recent-projects=' + str(root / 'recent')]
        proof['command'] = command
        process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        driver = NativeSceneInput(process, args.xdotool, env, '0', state, 1,
                                  minimum_wait=.15, phase_wait=.06, ack_pointer=True)
        window = wait_for_window(args.xdotool, env)
        driver.window = str(window)
        driver.raw('windowfocus', '--sync', window)
        driver.wait(lambda s: s.get('focused') == '1' and s.get('play_admitted') == '1' and
                    s.get('play_state') == '0', 'actual native Play is not ready',
                    time.monotonic() + 15)
        driver.play_key('F5')

        def failed():
            return re.search(rb'script reload admission: project=\d+ play=\d+ source=\d+ '
                             rb'applied=0 generation=0 message=[^\n]+\n', driver.stderr) is not None

        until = time.monotonic() + 90
        while time.monotonic() < until and not failed():
            require(process.poll() is None, 'Editor exited while compiling the real SDK script')
            driver.drain(.05)
        require(failed(), 'real SDK compiler failure did not reach native owner; streams retained')
        require(authored() == original, 'Play/compiler failure changed authored inputs')
        if args.capture:
            # Actual default dock Console tab and filter; no helper changes Console records.
            driver.click((330, 550), hold=0)
            driver.click((380, 577), hold=0)
            driver.wait(lambda s: s.get('text_input') == '1', 'Console filter did not receive focus')
            # Phase native text delivery; queued shifted characters can otherwise still be
            # waiting when a software-rendered frame is captured.
            for character in 'ScriptDiagnostic':
                driver.raw('type', '--clearmodifiers', '--delay', '80', '--', character)
            until = time.monotonic() + 8
            while time.monotonic() < until:
                rows = re.findall(rb'^script console ui visible=(\d+)\r?$', driver.stderr, re.M)
                if rows and rows[-1] == b'2':
                    break
                require(process.poll() is None, 'Editor exited awaiting the actual Console filter')
                driver.drain(.05)
            else:
                raise RuntimeError('Actual Console did not expose the two structured Error rows')
            driver.pointer_move(400, 648)
            driver.drain(.4)
            screenshot(display, int(window), evidence / 'script-error.png')
            driver.raw('key', 'Escape')
            driver.wait(lambda s: s.get('text_input') == '0', 'Console text ownership did not release')
        driver.play_key('F5')
        driver.wait(lambda s: s.get('play_state') == '0', 'actual Stop did not discard Play')
        request_window_close(window, env)
        driver.finish(15)
        output = (driver.stdout + driver.stderr).decode('utf-8', errors='replace')
        require(process.returncode == 0, 'Editor did not close normally')
        require(not re.search(r'Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+', output),
                'native SDK diagnostic case reported Vulkan validation errors')
        records = []
        for category, severity, source, message in re.findall(
                r'script console record category=(.*?) severity=(\d+) source_hex=([0-9a-f]*) '
                r'message_hex=([0-9a-f]*)', output):
            records.append(dict(category=category, severity=int(severity),
                                source=bytes.fromhex(source).decode(),
                                message=bytes.fromhex(message).decode()))
        errors = [r for r in records if r['category'] == 'ScriptDiagnostic' and r['severity'] == 3 and
                  'expected expression' in r['message'] and r['source'].endswith('game.zig:1:20')]
        require(errors and all(re.match(r'ScriptBuild project=\d+ play=\d+ source=\d+ \| ',
                                       r['source']) for r in errors),
                'real structured Error/location/current scope did not reach Runtime Console')
        require(any(r['category'] == 'Script Build' and r['severity'] == 1 and
                    'expected expression' in r['message'] for r in records),
                'existing sanitized Script Build Info output was lost')
        require(authored() == original, 'Stop/close changed authored inputs')
        proof.update(passed=True, editor_exit=process.returncode, records=records,
                     authored_sha256=original, located_errors=len(errors), project=str(project))
        print('Actual SDK compiler Error/location/scope, original Info, Play Stop and authored hashes verified')
    finally:
        if driver:
            driver.abort()
        elif process:
            if process.poll() is None:
                process.kill()
            process.communicate(timeout=5)
        if server:
            server.terminate()
            server.communicate(timeout=10)
        (evidence / 'receipt.json').write_text(json.dumps(proof, indent=2) + '\n')
        print('Native script Console evidence: ' + str(evidence), flush=True)
        if proof['passed']:
            shutil.rmtree(root)
        else:
            print('Failed native project retained: ' + str(root), flush=True)


if __name__ == '__main__':
    main()
