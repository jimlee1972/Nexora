#!/usr/bin/env python3
"""Open actual Game memory controls and observe copied payload/recovery plus native pixels."""
import argparse
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time
from LinuxDisplayAcceptance import request_window_close, start_xvfb, wait_for_window
from LinuxNativeScenePreview import scene_region_pixels


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'module', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix='nexora-play-memory-'))
    state = Path(tempfile.mkdtemp(prefix='nexora-play-memory-state-'))
    (root / 'Content').mkdir()
    (root / '.nexora/scenes').mkdir(parents=True)
    (root / 'project.nexora').write_text('schema=1\nname=Play host memory\n')
    (root / '.nexora/workspace').write_text('schema=1\n')
    scene = root / '.nexora/scenes/Main.scene'
    scene.write_text('NEXORA_EDITOR_SCENE 2\nnode 10 0 Camera\nnode 20 0 Source\nworld\n'
        'NEXORA_SCENE 3 "Memory scene" 0 2\n'
        '10 0 0 0 5 0 0 0 1 1 1 1 1 0 0 60 0.1 1000 1 0 0\n'
        '20 0 0 0 0 0 0 0 1 1 1 1 0 0 0 60 0.1 1000 1 0 0\n')
    baseline = scene.read_bytes()
    layout = 'schema=1\n'
    for title, panel in [('Project', 'project'), ('Hierarchy', 'hierarchy'),
                         ('Inspector', 'inspector'), ('Console', 'console'),
                         ('Content', 'content'), ('Scene', 'scene'), ('Game', 'game'),
                         ('Profiler', 'profiler'), ('Build', 'build')]:
        layout += f'[Window][{title}###nexora.{panel}]\n'
        layout += ('Pos=0,40\nSize=620,640\nCollapsed=0\n\n' if panel == 'game' else
                   'Pos=640,40\nSize=620,640\nCollapsed=0\n\n' if panel == 'scene' else
                   'Pos=0,700\nSize=100,20\nCollapsed=1\n\n')
    (root / '.nexora/editor-layout.ini').write_text(layout)
    module = Path(args.module).name
    shutil.copy2(args.module, root / 'Content' / module)
    server, display = start_xvfb(args.xvfb, '1280x720x24')
    editor = None
    captured = bytearray()
    stdout = tempfile.TemporaryFile()
    started = time.monotonic()
    succeeded = False
    try:
        if display is None:
            raise RuntimeError('Xvfb failed')
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(state), XDG_CACHE_HOME=str(state / 'cache'),
                   NEXORA_PLAY_MEMORY_UI_TEST_EVIDENCE='1',
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1')
        editor = subprocess.Popen([args.editor, f'--project={root}', '--graphical',
            '--native-scene-preview', '--frames=100000', f'--recent-projects={state / "recent"}',
            f'--gameplay-library=Content/{module}'], env=env, stdout=stdout, stderr=subprocess.PIPE)
        window = int(wait_for_window(args.xdotool, env))

        def pump():
            if select.select([editor.stderr], [], [], .03)[0]:
                data = os.read(editor.stderr.fileno(), 65536)
                captured.extend(data)
            if len(captured) > 4 * 1024 * 1024:
                raise RuntimeError('Evidence stream exceeded 4 MiB')
            if editor.poll() is not None:
                raise RuntimeError(f'Editor exited early: {editor.returncode}')

        def wait(predicate, message, timeout=5):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                pump()
                if predicate():
                    return
            raise RuntimeError(message)

        def command(*argv):
            subprocess.run([args.xdotool, *argv], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=5)

        def text():
            records = re.findall(rb'play memory text begin\n(.*?)\nplay memory text end',
                                 captured, re.DOTALL)
            return records[-1].decode() if records else ''

        def gui():
            records = re.findall(rb'^scene merge ui ([^\n]*)\n', captured, re.M)
            return dict(part.split('=', 1) for part in records[-1].decode().split()) \
                if records else {}

        def play_key(name):
            # One physical press, acknowledged by real processed GUI input before release.
            # Do not retry commands or extend the original five-second result deadline.
            deadline = time.monotonic() + 5
            index = {'F5': 0, 'F6': 1}[name]
            before = int(gui().get('play_requests', '0'))
            command('keydown', name)
            wait(lambda: gui().get('play_keys', '000')[index] == '1',
                 f'{name} down was not observed', max(0, deadline - time.monotonic()))
            command('keyup', name)
            wait(lambda: gui().get('play_keys', '111')[index] == '0',
                 f'{name} up was not observed', max(0, deadline - time.monotonic()))
            if int(gui().get('play_requests', '0')) != before + 1:
                raise RuntimeError(f'{name} was not admitted exactly once: {gui()}')

        wait(lambda: re.search(rb'play memory ui dpi=1 button=([\d.]+),([\d.]+)', captured),
             'Game memory button never rendered', 12)
        command('windowfocus', str(window))
        wait(lambda: gui().get('focused') == '1' and gui().get('play_admitted') == '1',
             'Focused Editor did not admit Play')
        play_key('F5')
        wait(lambda: b'play memory fixture updated peak=1048768 rejected=1,1' in captured,
             'Real fixture did not update')
        wait(lambda: re.search(rb'play memory ui dpi=1 button=[\d.]+,[\d.]+ '
                               rb'scope_session=1 current=1', captured),
             'Current Play geometry was not published')
        before = scene_region_pixels(display, window, (345, 195, 590, 330))
        point = re.findall(rb'play memory ui dpi=1 button=([\d.]+),([\d.]+)', captured)[-1]
        command('mousemove', '--window', str(window), str(round(float(point[0]))),
                str(round(float(point[1]))))
        command('click', '1')
        live = 'Live: 2 allocations | 1048640 payload bytes'
        peak = 'Peak: 3 allocations | 1048768 payload bytes'
        rejected = 'Rejected: 1 allocations | 1 deallocations'
        wait(lambda: live in text() and peak in text() and rejected in text(),
             'Native UI did not display exact current payload/peak/rejections')
        changed = 0

        def pixels_changed():
            nonlocal changed
            after = scene_region_pixels(display, window, (345, 195, 590, 330))
            changed = sum(a != b for a, b in zip(before, after))
            return changed >= 500

        wait(pixels_changed, 'Memory window did not change native UI pixels')
        play_key('F6')
        wait(lambda: '\nPaused\n' in text() and live in text(), 'Pause lost live observation')
        play_key('F5')
        recovered = 'Reclaimed: 1 allocations | 1048576 payload bytes'
        wait(lambda: '\nStopped\n' in text() and recovered in text() and
             'Current: no loaded gameplay module.' in text() and peak in text(),
             'Stop did not display last teardown recovery')
        play_key('F5')
        wait(lambda: 'Play session: 2\n' in text() and live in text() and peak in text()
             and recovered in text(), 'Restart did not separate current and previous teardown')
        if scene.read_bytes() != baseline:
            raise RuntimeError('Read-only inspection changed source bytes')
        request_window_close(str(window), env)
        _, tail = editor.communicate(timeout=15)
        captured.extend(tail)
        stdout.seek(0)
        output = stdout.read(4 * 1024 * 1024 + 1)
        if len(output) > 4 * 1024 * 1024 or editor.returncode:
            raise RuntimeError('Output budget or shutdown failed')
        if captured.count(b'play memory fixture destroyed retained=1048576 pages=1') != 2:
            raise RuntimeError('Teardown lost live host pages')
        if not re.search(rb'console producer evidence:.*gameplay_retained=2\b', captured):
            raise RuntimeError('Both fallback warnings did not reach actual Console')
        if re.search(rb'Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+', output + captured):
            raise RuntimeError('Vulkan validation failed')
        if scene.read_bytes() != baseline:
            raise RuntimeError('Shutdown changed source bytes')
        print('Native Play memory UI evidence: ' + json.dumps(dict(
            sessions=2, live_bytes=1048640, peak_bytes=1048768, reclaimed_bytes=1048576,
            rejected_allocations=1, rejected_deallocations=1, console_warnings=2,
            changed_rgb_components=changed, source_bytes='unchanged', pause_stop_restart='passed',
            elapsed_seconds=round(time.monotonic() - started, 3)), sort_keys=True))
        succeeded = True
        return 0
    except Exception as error:
        (state / 'editor.stderr.log').write_bytes(captured)
        stdout.seek(0)
        (state / 'editor.stdout.log').write_bytes(stdout.read(4 * 1024 * 1024 + 1))
        print(f'Play memory failure fixtures retained: {root} {state}', flush=True)
        raise RuntimeError(f'{error}; tail={bytes(captured[-6000:])!r}') from error
    finally:
        if editor is not None and editor.poll() is None:
            editor.kill()
            editor.communicate(timeout=5)
        server.terminate()
        server.communicate(timeout=5)
        stdout.close()
        if succeeded:
            shutil.rmtree(root, ignore_errors=True)
            shutil.rmtree(state, ignore_errors=True)


if __name__ == '__main__':
    raise SystemExit(main())
