#!/usr/bin/env python3
"""Review and explicitly revert a persisted instance through actual native controls."""
import argparse
import os
from pathlib import Path
import queue
import re
import shutil
import subprocess
import tempfile
import threading
import time

from LinuxDisplayAcceptance import collect_output, launch, request_window_close, start_xvfb, wait_for_window
from LinuxNativeScenePreview import scene_pixels, scene_region_pixels


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
    process = None
    try:
        require(display is not None, 'Xvfb failed')
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(scratch / 'state'),
                   MESA_SHADER_CACHE_DIR=str(scratch / 'shader-cache'))

        def send(*arguments):
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(.2)

        def click(window, x, y):
            send('mousemove', '--window', window, str(x), str(y))
            send('mousedown', '1')
            send('mouseup', '1')

        def key(chord):
            parts = chord.split('+')
            for part in parts:
                send('keydown', part)
            for part in reversed(parts):
                send('keyup', part)

        def wait(predicate, message):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Native host exited before ' + message)
                if predicate():
                    return
                time.sleep(.05)
            print(''.join(diagnostics), flush=True)
            capture = os.environ.get('NEXORA_NATIVE_ACCEPTANCE_FAILURE_IMAGE')
            if capture and shutil.which('import'):
                subprocess.run(['import', '-display', display, '-window', window, capture],
                               env=env, check=False)
            raise RuntimeError(message)

        def inspector_button(window, index):
            # Observe presented controls at the initial dock size, including report wrapping.
            # Sampling left padding avoids glyph holes; every gesture occurs exactly once.
            position = []

            def presented():
                groups, run = [], []
                for top in range(80, 380, 100):
                    pixels = scene_region_pixels(display, int(window), (1010, top, 1, 100))
                    for offset, (red, green, blue) in enumerate(zip(pixels[::3], pixels[1::3], pixels[2::3])):
                        if blue >= 105 and blue >= red + 35 and blue >= green + 10:
                            run.append(top + offset * 3)
                        elif run:
                            if len(run) >= 3:
                                groups.append(run[len(run) // 2])
                            run = []
                if run and len(run) >= 3:
                    groups.append(run[len(run) // 2])
                if len(groups) > index:
                    position[:] = [groups[index]]
                    return True
                return False
            wait(presented, 'Requested Inspector control was not presented')
            print(f'Inspector control{index} presented at y{position[0]}', flush=True)
            return position[0]

        def modal_confirm(window):
            # The fixed-size confirmation is centered in the initial1280x720 viewport.
            # Observe Confirm's left padding below the title bar; title blue must not count
            # as button geometry, and underlying Scene gizmos must not count as consent.
            points = []

            def presented():
                pixels = scene_region_pixels(display, int(window), (412, 310, 1, 120))
                ys = [310 + index * 3 for index, (red, green, blue) in
                      enumerate(zip(pixels[::3], pixels[1::3], pixels[2::3]))
                      if blue >= 105 and blue >= red + 35 and blue >= green + 10]
                if len(ys) >= 3:
                    points[:] = [412, (min(ys) + max(ys)) // 2]
                    return True
                return False
            wait(presented, 'Explicit revert confirmation was not presented')
            return points

        revert_y = None
        for read_only in (False, True):
            before_launch = files(root)
            process = launch(args.editor, root, scratch / 'recent', env, read_only=read_only)
            diagnostics, events = [], queue.Queue()

            def drain():
                for line in process.stderr:
                    diagnostics.append(line)
                    if line.startswith('prefab instance '):
                        events.put(line.strip())
            reader = threading.Thread(target=drain, daemon=True)
            reader.start()
            window = wait_for_window(args.xdotool, env)
            send('windowfocus', '--sync', window)
            selection_y = 199 if read_only else 182
            wait(lambda: scene_pixels(display, int(window), (518, selection_y, 4, 4))[0],
                 'Scene selection control was not ready')
            click(window, 523, selection_y)
            before_review = files(root)
            click(window, 1065, inspector_button(window, 1))
            try:
                event = events.get(timeout=10)
            except queue.Empty as error:
                raise RuntimeError('Actual review did not emit an owning result') from error
            require(re.fullmatch(r'prefab instance reviewed rows=[1-9][0-9]* retained=1 published=2', event),
                    'Review lost retained/current source identity: ' + event)
            require(files(root) == before_review, 'Read-only review wrote project/source files')
            if read_only:
                click(window, 1080, revert_y)
                require(events.empty() and scene.read_bytes() == original,
                        'Read-only control authorized a revert')
            else:
                revert_y = inspector_button(window, 2)
                click(window, 1080, revert_y)
                key('Escape')
                require(events.empty() and files(root) == before_review,
                        'Cancel mutated files or authorized revert')
                click(window, 1080, inspector_button(window, 2))
                click(window, *modal_confirm(window))
                try:
                    event = events.get(timeout=10)
                except queue.Empty as error:
                    raise RuntimeError('Confirmed actual control did not revert') from error
                require(event == 'prefab instance reverted retained=1' and files(root) == before_review,
                        'Revert changed files before explicit Scene Save or used current source')
                key('ctrl+s')
                wait(lambda: scene.read_bytes() == retained, 'Revert Scene Save lost exact retained bytes')
                key('ctrl+z')
                key('ctrl+s')
                wait(lambda: scene.read_bytes() == original, 'One Undo did not restore all local properties')
                key('ctrl+y')
                key('ctrl+s')
                wait(lambda: scene.read_bytes() == retained, 'One Redo did not restore the entire revert')
                key('ctrl+z')
                key('ctrl+s')
                wait(lambda: scene.read_bytes() == original, 'Final Undo did not restore readonly fixture')
            request_window_close(window, env)
            process.wait(timeout=15)
            reader.join(timeout=5)
            require(not reader.is_alive(), 'Diagnostic drain did not finish')
            collect_output(process, 5)
            captured = ''.join(diagnostics)
            print(captured, end='', flush=True)
            require(process.returncode == 0 and 'scene_selected=1' in captured and
                    not re.search(r'Validation Error\b|VUID-|SYNC-HAZARD-', captured),
                    'Native instance workflow failed or reported Vulkan errors')
            require(scene.read_bytes() == original and
                    {p: data for p, data in files(root).items() if 'prefabs' in p.parts} == sources,
                    'Instance workflow changed source archives or original scene')
            if read_only:
                require(files(root) == before_launch, 'Read-only reopen wrote project files')
            process = None
        print('Native scoped override review, explicit revert, atomic Undo/Redo, Save and readonly passed')
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=10)
        if xvfb.poll() is None:
            xvfb.terminate()
        xvfb.communicate(timeout=10)
        shutil.rmtree(scratch)


if __name__ == '__main__':
    main()
