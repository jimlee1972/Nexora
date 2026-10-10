#!/usr/bin/env python3
"""Review source conflicts and confirm one native live placement rebase."""
import argparse
import difflib
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

        def wait(predicate, message, expected=None):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Native host exited before ' + message)
                if predicate():
                    return
                time.sleep(.05)
            print(''.join(diagnostics), flush=True)
            if expected is not None:
                print(''.join(difflib.unified_diff(expected.decode().splitlines(keepends=True),
                      scene.read_text().splitlines(keepends=True), fromfile='expected fixture',
                      tofile='saved fixture')), flush=True)
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
            wait(presented, 'Explicit rebase confirmation was not presented')
            return points

        def conflict_row(window, review_y):
            points = []
            def presented():
                groups, run = [], []
                for top in range(240, 640, 100):
                    pixels = scene_region_pixels(display, int(window), (1018, top, 1, 100))
                    for offset, (red, green, blue) in enumerate(zip(pixels[::3], pixels[1::3], pixels[2::3])):
                        y = top + offset * 3
                        if y > review_y + 80 and blue >= 45 and blue >= red + 20 and blue >= green + 10:
                            run.append(y)
                        elif run:
                            if len(run) >= 2:
                                groups.append(run[len(run) // 2])
                            run = []
                if run and len(run) >= 2:
                    groups.append(run[len(run) // 2])
                if groups:
                    points[:] = [groups[0]]
                    return True
                return False
            wait(presented, 'Actual Keep Local conflict control was not presented')
            return points[0]

        apply_offset = None
        for read_only in (False, True):
            before_launch = files(root)
            process = launch(args.editor, root, scratch / 'recent', env, read_only=read_only)
            diagnostics, events = [], queue.Queue()
            def drain():
                for line in process.stderr:
                    diagnostics.append(line)
                    if line.startswith('prefab source rebase '):
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
            review_y = inspector_button(window, 2)
            click(window, 1065, review_y)
            event = events.get(timeout=10)
            require(event == 'prefab source rebase reviewed retained=1 published=2 conflicts=1',
                    'Review lost exact source conflict: ' + event)
            require(files(root) == before_review, 'Source review wrote project files')
            click(window, 1018, conflict_row(window, review_y))
            if read_only:
                click(window, 1080, review_y + apply_offset)
                require(events.empty() and files(root) == before_review,
                        'Readonly conflict choice acquired write authority')
            else:
                apply_y = inspector_button(window, 3)
                apply_offset = apply_y - review_y
                click(window, 1080, apply_y)
                key('Escape')
                require(events.empty() and files(root) == before_review, 'Canceled rebase changed files')
                click(window, 1080, inspector_button(window, 3))
                click(window, *modal_confirm(window))
                event = events.get(timeout=10)
                require(event == 'prefab source rebase applied retained=1 published=2 choices=1' and
                        files(root) == before_review, 'Confirmed rebase wrote files before Scene Save')
                key('ctrl+s')
                wait(lambda: scene.read_bytes() == retained, 'Source rebase lost exact group/revision bytes', retained)
                key('ctrl+z'); key('ctrl+s')
                wait(lambda: scene.read_bytes() == original, 'One Undo lost original revision/local values', original)
                key('ctrl+y'); key('ctrl+s')
                wait(lambda: scene.read_bytes() == retained, 'One Redo lost exact rebased scene', retained)
                key('ctrl+z'); key('ctrl+s')
                wait(lambda: scene.read_bytes() == original, 'Final Undo lost readonly original fixture', original)
            request_window_close(window, env)
            process.wait(timeout=15)
            reader.join(timeout=5)
            require(not reader.is_alive(), 'Diagnostic drain did not finish')
            collect_output(process, 5)
            captured = ''.join(diagnostics)
            print(captured, end='', flush=True)
            require(process.returncode == 0 and 'scene_selected=1' in captured and
                    not re.search(r'Validation Error\b|VUID-|SYNC-HAZARD-', captured),
                    'Native source rebase failed or reported Vulkan errors')
            require(scene.read_bytes() == original and
                    {p: data for p, data in files(root).items() if 'prefabs' in p.parts} == sources,
                    'Rebase changed source archives or final scene')
            if read_only:
                require(files(root) == before_launch, 'Readonly reopen wrote project files')
            process = None
        print('Native source conflict review, consent, atomic revision history, Save and readonly passed')
    finally:
        if process is not None and process.poll() is None:
            process.kill(); process.wait(timeout=10)
        if xvfb.poll() is None:
            xvfb.terminate()
        xvfb.wait(timeout=10)
        shutil.rmtree(scratch, ignore_errors=True)


if __name__ == '__main__':
    main()
