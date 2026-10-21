#!/usr/bin/env python3
"""Publish one existing source field through real X11 controls; never synthesize host requests."""
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
    scratch = Path(tempfile.mkdtemp(prefix='nexora-native-instance-source-'))
    process = xvfb = None
    try:
        root, oracle = scratch / 'project', scratch / 'retained.scene'
        subprocess.run([args.fixture, '--write-native-selected-fixture', str(root), str(oracle)], check=True)
        name_row = int(Path(str(oracle) + '.rows').read_text().splitlines()[0])
        current, = (root / '.nexora/prefabs').glob('*.nxprefab')
        archive, = (root / '.nexora/prefabs/revisions').glob('*/1.nxprefab')
        retained = archive.read_bytes()
        # Seed a current revision1 before launch; all subsequent publication is native input.
        current.write_bytes(retained)
        require(retained.startswith(b'NXPFAB1\n') and int.from_bytes(retained[24:32], 'little') == 1,
                'Fixture is not an independent version-one prefab container')
        size = int.from_bytes(retained[64:72], 'little')
        body = retained[-size:]
        old_name, new_name = b'node 2 0 Retained source\n', b'node 2 0 Local instance\n'
        require(body.count(old_name) == 1, 'Independent Name oracle absent')
        replacement = body.replace(old_name, new_name)
        expected = bytearray(retained[:-size])
        expected[24:32] = (2).to_bytes(8, 'little')
        expected[64:72] = len(replacement).to_bytes(8, 'little')
        expected.extend(replacement)
        scene = root / '.nexora/scenes/Main.scene'
        original_scene = scene.read_bytes()
        xvfb, display = start_xvfb(args.xvfb, '1280x720x24')
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

        def wait(predicate, message):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Native host exited: ' + message)
                if predicate():
                    return
                time.sleep(.05)
            print(''.join(diagnostics), flush=True)
            raise RuntimeError(message)

        def control(x, top, bottom, ordinal, threshold):
            position = []

            def presented():
                groups, run = [], []
                for chunk in range(top, bottom, 100):
                    pixels = scene_region_pixels(display, int(window), (x, chunk, 1, min(100, bottom - chunk)))
                    for index, (red, green, blue) in enumerate(zip(pixels[::3], pixels[1::3], pixels[2::3])):
                        if blue >= threshold and blue >= red + 20 and blue >= green + 10:
                            run.append(chunk + index * 3)
                        elif run:
                            if len(run) >= 3:
                                groups.append(run[len(run) // 2])
                            run = []
                if run and len(run) >= 3:
                    groups.append(run[len(run) // 2])
                if len(groups) > ordinal:
                    position[:] = [x, groups[ordinal]]
                    return True
                return False
            wait(presented, 'Requested native control was not presented')
            return position

        for read_only in (False, True):
            before = files(root)
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
            y = 199 if read_only else 182
            wait(lambda: scene_pixels(display, int(window), (518, y, 4, 4))[0], 'Scene selection absent')
            click(window, 523, y)
            click(window, 1065, control(1010, 80, 480, 1, 105)[1])
            wait(lambda: not events.empty(), 'Native review did not emit an owning result')
            event = events.get_nowait()
            require(re.fullmatch(r'prefab instance reviewed rows=[1-9][0-9]* retained=1 published=[12]', event),
                    'Review did not retain the original binding: ' + event)
            if read_only:
                click(window, name_position[0], name_position[1] + 17)
                click(window, apply_position[0], apply_position[1] + 17)
                send('key', 'Escape')
                require(events.empty() and files(root) == before,
                        'Read-only reopen emitted publication or wrote files')
            else:
                whole_y = control(1010, 80, 480, 2, 105)[1]
                name_position = control(1018, whole_y + 75, 700, name_row, 45)
                click(window, *name_position)
                require(scene.read_bytes() == original_scene and current.read_bytes() == retained and
                        archive.read_bytes() == retained, 'Review/selection changed authoring files')
                before_consent = files(root)
                apply_position = control(1010, 80, 480, 4, 105)
                click(window, *apply_position)
                control(392, 300, 450, 0, 105)
                send('key', 'Escape')
                require(events.empty() and files(root) == before_consent, 'Source Cancel changed files')
                click(window, *apply_position)
                click(window, *control(392, 300, 450, 0, 105))
                wait(lambda: not events.empty(), 'Native source confirmation did not complete')
                require(events.get_nowait() == 'prefab instance source applied retained=1 published=2 rows=1',
                        'Native confirmation did not publish exactly one selected row')
                wait(lambda: current.read_bytes() == expected, 'Published source lost independent exact-byte oracle')
                require(archive.read_bytes() == retained and scene.read_bytes() == original_scene,
                        'Source publication changed retained archive or live Scene file')
            request_window_close(window, env)
            process.wait(timeout=15)
            reader.join(timeout=5)
            require(not reader.is_alive(), 'Diagnostic drain did not finish')
            collect_output(process, 5)
            captured = ''.join(diagnostics)
            print(captured, end='', flush=True)
            require(process.returncode == 0 and 'scene_selected=1' in captured and
                    not re.search(r'Validation Error\b|VUID-|SYNC-HAZARD-', captured),
                    'Native source workflow failed or reported Vulkan errors')
            require(scene.read_bytes() == original_scene and archive.read_bytes() == retained,
                    'Closing native host changed Scene or archived source')
            if read_only:
                require(files(root) == before, 'Read-only teardown wrote project files')
            process = None
        print('Native source confirmation/Cancel, exact selected publication, archive/Scene conservation and readonly passed')
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=10)
        if xvfb is not None:
            if xvfb.poll() is None:
                xvfb.terminate()
            xvfb.communicate(timeout=10)
        shutil.rmtree(scratch)


if __name__ == '__main__':
    main()
