#!/usr/bin/env python3
"""Native default startup and single-scene Project double-click acceptance."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import collect_output, launch, request_window_close, start_xvfb, wait_for_window
from NativeSceneInput import NativeSceneInput


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix='nexora-single-scene-'))
    state = Path(tempfile.mkdtemp(prefix='nexora-single-scene-state-'))
    (root / 'Content').mkdir()
    (root / '.nexora').mkdir()
    (root / 'project.nexora').write_text('schema=1\nname=Single Scene Acceptance\n')
    (root / '.nexora/workspace').write_text('schema=1\n')
    legacy = root / '.nexora/scene-composition.ini'
    legacy.write_bytes(b'previous scene-set metadata is retained, never restored\n')
    original_legacy = legacy.read_bytes()
    server, display = start_xvfb(args.xvfb, '1280x900x24')
    process = driver = None
    passed = False
    try:
        if display is None:
            raise RuntimeError('Xvfb did not become ready')
        env = dict(os.environ, DISPLAY=display, XDG_STATE_HOME=str(state))
        # No --graphical, project or launcher workaround: the executable selects its GUI default.
        process = subprocess.Popen([args.editor, '--frames=8', '--recent-projects=' + str(state / 'recent')],
                                   env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        _, error = collect_output(process, 20)
        if process.returncode or 'acquired=8 presented=8' not in error:
            raise RuntimeError('Default Editor launch did not open the Project Browser: ' + error)
        process = None
        env['NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY'] = '1'
        env['NEXORA_PROJECT_TREE_TEST_PATH'] = 'Content/Second.scene'
        process = launch(args.editor, root, state / 'recent', env, binary=True)
        window = wait_for_window(args.xdotool, env)
        driver = NativeSceneInput(process, args.xdotool, env, window, state, 1)
        driver.raw('windowfocus', '--sync', window)
        driver.wait(lambda v: v['focused'] == '1', 'Editor did not receive focus')
        driver.chord('ctrl+s')
        primary = root / '.nexora/scenes/Main.scene'

        def wait(predicate, message):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                driver.drain(.05)
                if predicate():
                    return
                if process.poll() is not None:
                    break
            raise RuntimeError(message)

        wait(primary.is_file, 'Single-scene Save did not publish the starter scene')
        original = primary.read_bytes()
        second = root / 'Content/Second.scene'
        second.write_bytes(original)
        # Open the actual Project window through its public shortcut.
        for key in ('Control_L', 'Alt_L', 'j'):
            driver.raw('keydown', key)
        for key in ('j', 'Alt_L', 'Control_L'):
            driver.raw('keyup', key)

        def point():
            matches = re.findall(rb'^project tree scene point=([0-9.]+),([0-9.]+)$', driver.stderr, re.M)
            return tuple(map(float, matches[-1])) if matches else None

        wait(lambda: point() is not None, 'Project tree did not expose the refreshed scene row')
        x, y = point()
        driver.raw('mousemove', '--window', window, str(round(x)), str(round(y)))
        driver.raw('click', '1', minimum_wait=.05)
        driver.raw('click', '1')
        metadata = root / '.nexora/scene-session.ini'
        wait(lambda: metadata.is_file() and metadata.read_bytes().endswith(b'scene=Content/Second.scene\n'),
             'Left double-click did not replace the scene with the selected Project file')
        driver.chord('ctrl+shift+n')
        driver.chord('ctrl+s')
        wait(lambda: second.read_bytes() != original, 'Editing after Open did not save to the selected scene')
        if primary.read_bytes() != original or legacy.read_bytes() != original_legacy:
            raise RuntimeError('Single-scene replacement changed the previous source or old scene-set metadata')
        request_window_close(window, env)
        _, error = driver.finish(15)
        if process.returncode or 'scene_documents=1 scene_references=0' not in error:
            raise RuntimeError('Editor retained additive scenes or failed to close: ' + error)
        process = None
        passed = True
        print('Default GUI startup, Project-tree double-click and single-scene save passed.')
    finally:
        if process is not None:
            process.kill()
            process.communicate(timeout=10)
        server.terminate()
        server.communicate(timeout=10)
        if passed:
            shutil.rmtree(root)
            shutil.rmtree(state)
        else:
            print(f'Failure fixture retained: {root}; state: {state}')


if __name__ == '__main__':
    main()
