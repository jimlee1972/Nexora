#!/usr/bin/env python3
"""Review source conflicts and confirm one native live placement rebase."""
import argparse
import difflib
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from NativeSceneInput import NativeSceneInput


def require(value, message):
    if not value:
        raise RuntimeError(message)


def files(root):
    return {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix='nexora-native-instance-overrides-'))
    root, expected_file = scratch / 'project', scratch / 'retained.scene'
    subprocess.run([args.fixture, '--write-native-fixture', str(root), str(expected_file)], check=True)
    scene = root / '.nexora/scenes/Main.scene'
    original, retained = scene.read_bytes(), expected_file.read_bytes()
    require(original != retained and b'prefab-placement ' in original,
            'Fixture did not persist actual property overrides and instance bindings')
    sources = {p: data for p, data in files(root).items() if 'prefabs' in p.parts}
    xvfb, display = start_xvfb(args.xvfb, '1280x720x24')
    process = driver = None
    passed = False
    case_deadline = time.monotonic() + 90
    try:
        require(display is not None, 'Xvfb failed')
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(scratch / 'state'),
                   MESA_SHADER_CACHE_DIR=str(scratch / 'shader-cache'),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1')

        def deadline(seconds=10):
            until = min(case_deadline, time.monotonic() + seconds)
            require(until > time.monotonic(), 'Native rebase exceeded its whole-case deadline')
            return until

        def wait(predicate, message, expected=None, until=None):
            until = deadline() if until is None else until
            while time.monotonic() < until:
                require(process.poll() is None, 'Native host exited before ' + message)
                if predicate() and time.monotonic() < until:
                    return
                driver.drain(min(.05, max(0, until - time.monotonic())))
            if expected is not None:
                print(''.join(difflib.unified_diff(expected.decode().splitlines(keepends=True),
                      scene.read_text().splitlines(keepends=True), fromfile='expected fixture',
                      tofile='saved fixture')), flush=True)
            capture = os.environ.get('NEXORA_NATIVE_ACCEPTANCE_FAILURE_IMAGE')
            if capture and shutil.which('import'):
                subprocess.run(['import', '-display', display, '-window', window, capture],
                               env=env, check=False)
            raise RuntimeError(message + '; actual GUI=' + repr(driver.observation()))

        def click_named(name, until=None):
            until = deadline() if until is None else until
            value = driver.wait(lambda v: v.get('focused') == '1' and
                                          v.get('buttons') == '000' and name in v,
                                'Actual native control unavailable: ' + name, until)
            # Refresh coordinates only; emit one down/up after the copied control settles.
            for _ in range(3):
                point = tuple(map(round, map(float, value[name].split(','))))
                driver.pointer_move(*point, until)
                value = driver.observation()
                if name in value and tuple(map(round, map(float, value[name].split(',')))) == point:
                    break
                value = driver.wait(lambda v: name in v,
                                    'Native control disappeared while positioning: ' + name, until)
            else:
                raise RuntimeError('Native control did not settle before one click: ' + name)
            driver.pointer_button(1, True, until)
            driver.pointer_button(1, False, until)

        def key(chord, until=None):
            until = deadline() if until is None else until
            if chord not in ('Escape', 'ctrl+y'):
                driver.chord(chord, deadline=until)
                return
            auxiliary = 0 if chord == 'Escape' else 1
            driver.wait(lambda v: v.get('focused') == '1' and v.get('keys') == '000000000'
                                  and v.get('rebase_keys') == '00',
                        'Rebase key was not focused/released', until)
            if chord == 'ctrl+y':
                driver.raw('keydown', 'Control_L', deadline=until, minimum_wait=.2)
                driver.wait(lambda v: v['keys'][0] == '1', 'GUI did not observe Ctrl down', until)
            physical = 'Escape' if chord == 'Escape' else 'y'
            driver.raw('keydown', physical, deadline=until, minimum_wait=.2)
            driver.wait(lambda v: v['rebase_keys'][auxiliary] == '1',
                        'GUI did not observe rebase key down', until)
            driver.raw('keyup', physical, deadline=until, minimum_wait=.2)
            driver.wait(lambda v: v['rebase_keys'][auxiliary] == '0',
                        'GUI did not observe rebase key up', until)
            if chord == 'ctrl+y':
                driver.raw('keyup', 'Control_L', deadline=until, minimum_wait=.2)
                driver.wait(lambda v: v['keys'][0] == '0', 'GUI did not observe Ctrl up', until)

        def events(start):
            return re.findall(rb'^prefab source rebase ([^\n]*)\n', driver.stderr[start:], re.M)

        def event(expected, start, until):
            wait(lambda: events(start), 'Native source rebase event unavailable', until=until)
            actual = events(start)
            require(actual == [expected.encode()], 'Source rebase lost exact event identity: ' + repr(actual))
            return len(driver.stderr)

        for launch_number, read_only in enumerate((False, True), 1):
            before_launch = files(root)
            process = launch(args.editor, root, scratch / 'recent', env,
                             read_only=read_only, binary=True)
            driver = NativeSceneInput(process, args.xdotool, env, '', scratch, launch_number,
                                      minimum_wait=.2, phase_wait=.2)
            window = wait_for_window(args.xdotool, env)
            driver.window = str(window)
            until = deadline()
            driver.raw('windowfocus', '--sync', window, deadline=until)
            click_named('source_node', until)
            driver.wait(lambda v: v.get('source_selected') == '1' and
                                  v.get('source_selection_count') == '1' and 'rebase_review' in v,
                        'Single bound-node selection did not expose actual source rebase', until)
            before_review = files(root)
            until = deadline()
            start = len(driver.stderr)
            click_named('rebase_review', until)
            cursor = event('reviewed retained=1 published=2 conflicts=1', start, until)
            require(files(root) == before_review, 'Source review wrote project files')
            click_named('rebase_keep_local')
            if read_only:
                click_named('rebase_apply')
                require(not events(cursor) and files(root) == before_review,
                        'Readonly conflict choice acquired write authority')
            else:
                until = deadline()
                click_named('rebase_apply', until)
                driver.wait(lambda v: 'rebase_confirm' in v,
                            'Explicit rebase confirmation was not presented', until)
                key('Escape', until)
                driver.wait(lambda v: 'rebase_confirm' not in v,
                            'Canceled rebase retained its modal', until)
                require(not events(cursor) and files(root) == before_review, 'Canceled rebase changed files')
                until = deadline()
                click_named('rebase_apply', until)
                click_named('rebase_confirm', until)
                cursor = event('applied retained=1 published=2 choices=1', cursor, until)
                require(files(root) == before_review, 'Confirmed rebase wrote files before Scene Save')
                key('ctrl+s')
                wait(lambda: scene.read_bytes() == retained, 'Source rebase lost exact group/revision bytes', retained)
                key('ctrl+z'); key('ctrl+s')
                wait(lambda: scene.read_bytes() == original, 'One Undo lost original revision/local values', original)
                key('ctrl+y'); key('ctrl+s')
                wait(lambda: scene.read_bytes() == retained, 'One Redo lost exact rebased scene', retained)
                key('ctrl+z'); key('ctrl+s')
                wait(lambda: scene.read_bytes() == original, 'Final Undo lost readonly original fixture', original)
            request_window_close(window, env)
            until = deadline(15)
            while process.poll() is None and time.monotonic() < until:
                driver.drain(min(.05, max(0, until - time.monotonic())))
            require(process.poll() is not None, 'Native source rebase did not close within 15 seconds')
            remaining = case_deadline - time.monotonic()
            require(remaining > 0, 'Native rebase exceeded its whole-case deadline')
            _, captured = driver.finish(min(5, remaining))
            print(captured, end='', flush=True)
            require(process.returncode == 0 and 'scene_selected=1' in captured and
                    not re.search(r'Validation Error\b|VUID-|SYNC-HAZARD-', captured),
                    'Native source rebase failed or reported Vulkan errors')
            require(scene.read_bytes() == original and
                    {p: data for p, data in files(root).items() if 'prefabs' in p.parts} == sources,
                    'Rebase changed source archives or final scene')
            if read_only:
                require(files(root) == before_launch, 'Readonly reopen wrote project files')
            process = driver = None
        print('Native source conflict review, consent, atomic revision history, Save and readonly passed')
        passed = True
    finally:
        if driver is not None:
            driver.abort()
        elif process is not None:
            if process.poll() is None:
                process.kill()
            process.communicate(timeout=10)
        if xvfb.poll() is None:
            xvfb.terminate()
        xvfb.wait(timeout=10)
        if passed:
            shutil.rmtree(scratch, ignore_errors=True)
        else:
            print(f'Native source rebase fixtures retained: {scratch}', flush=True)


if __name__ == '__main__':
    main()
