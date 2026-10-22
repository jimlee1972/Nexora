#!/usr/bin/env python3
"""Inspect a saved bound scene's retained prefab source through the actual native UI."""
import argparse
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
    scratch = Path(tempfile.mkdtemp(prefix='nexora-native-prefab-source-'))
    root = scratch / 'project'
    subprocess.run([args.fixture, '--write-native-fixture', str(root)], check=True)
    scene = root / '.nexora/scenes/Main.scene'
    original_scene = scene.read_bytes()
    require(original_scene.startswith(b'NEXORA_EDITOR_SCENE 4\n') and b'prefab-placement ' in original_scene,
            'Native fixture did not persist a real scene prefab binding')
    archives = list((root / '.nexora/prefabs/revisions').glob('*/1.nxprefab'))
    require(len(archives) == 1, 'Retained source archive fixture missing or ambiguous')
    archive = archives[0]
    original_archive = archive.read_bytes()
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
            return min(case_deadline, time.monotonic() + seconds)

        def click(name, until):
            # Layout can settle after the first record and before pointer processing.
            # Refresh only positioning; send exactly one native down/up at the named item.
            value = driver.wait(lambda v: v.get('focused') == '1' and
                                          v.get('buttons') == '000' and name in v,
                                'Named source control was not focused/rendered/released', until)
            for _ in range(3):
                point = tuple(map(round, map(float, value[name].split(','))))
                driver.pointer_move(*point, until)
                value = driver.wait(lambda v: v.get('focused') == '1' and
                                              v.get('buttons') == '000' and name in v,
                                    'Named source control disappeared before its one click', until)
                current = tuple(map(round, map(float, value[name].split(','))))
                if current == point:
                    break
            else:
                raise RuntimeError('Named source geometry did not settle before one click: ' + name)
            driver.pointer_button(1, True, until)
            driver.pointer_button(1, False, until)

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
            value = driver.wait(lambda v: v.get('focused') == '1' and 'source_node' in v,
                                'Bound Scene marker was not rendered', until)
            click('source_node', until)
            driver.wait(lambda v: v.get('source_selected') == '1' and
                                  v.get('source_selection_count') == '1' and 'source_inspect' in v,
                        'Single native bound-node selection did not expose source inspection', until)

            def inspect(resolved):
                before = files(root)
                until = deadline()
                value = driver.wait(lambda v: v.get('focused') == '1' and
                                              v.get('source_selected') == '1' and
                                              v.get('source_selection_count') == '1' and
                                              'source_inspect' in v,
                                    'Actual source inspection control was not rendered', until)
                start = len(driver.stderr)
                click('source_inspect', until)
                expected = f'prefab source inspected resolved={resolved} retained=1 published=2'.encode()
                driver.wait(lambda _: re.search(rb'^prefab source inspected [^\n]*\n',
                                               driver.stderr[start:], re.M) is not None,
                            'Actual source button did not produce a scoped inspection', until)
                events = re.findall(rb'^prefab source inspected ([^\n]*)\n',
                                    driver.stderr[start:], re.M)
                require(events == [expected.removeprefix(b'prefab source inspected ')],
                        'Native source inspection lost retained/current identity or repeated: ' + repr(events))
                require(files(root) == before and scene.read_bytes() == original_scene,
                        'Inspect source changed scene, bindings, archives or project metadata')

            inspect(1)
            # Explicitly refresh after source availability changes; no frame performs source IO.
            archive.unlink()
            inspect(0)
            archive.write_bytes(original_archive)
            inspect(1)
            request_window_close(window, env)
            until = deadline(15)
            while process.poll() is None and time.monotonic() < until:
                driver.drain(min(.05, max(0, until - time.monotonic())))
            require(process.poll() is not None, 'Native source inspection did not close within 15 seconds')
            _, captured = driver.finish(5)
            require(process.returncode == 0 and 'scene_selected=1' in captured,
                    'Bound native scene inspection/close failed')
            require(not re.search(r'Validation Error\b|VUID-|SYNC-HAZARD-', captured),
                    'Native source inspection reported a Vulkan validation error')
            process = driver = None
            require(scene.read_bytes() == original_scene and archive.read_bytes() == original_archive,
                    'Native close changed bound scene or retained source')
            if read_only:
                require(files(root) == before_launch, 'Read-only inspection wrote project files')
        print('Native bound-source inspection, retained/current revisions, missing/restored source, '
              'read-only reopen and source conservation passed')
        passed = True
    finally:
        if driver is not None:
            driver.abort()
        elif process is not None and process.poll() is None:
            process.kill()
            process.communicate(timeout=10)
        if xvfb.poll() is None:
            xvfb.terminate()
        xvfb.communicate(timeout=10)
        if passed:
            shutil.rmtree(scratch)
        else:
            print(f'Native prefab source fixtures retained: {scratch}', flush=True)


if __name__ == '__main__':
    main()
