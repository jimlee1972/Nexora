#!/usr/bin/env python3
"""Inspect a saved bound scene's retained prefab source through the actual native UI."""
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
    # Keep the native initial size: resizing after docking retains split pixel widths.
    xvfb, display = start_xvfb(args.xvfb, '1280x720x24')
    process = None
    try:
        require(display is not None, 'Xvfb failed')
        env = os.environ.copy()
        env['DISPLAY'] = display
        env['XDG_STATE_HOME'] = str(scratch / 'state')
        env['MESA_SHADER_CACHE_DIR'] = str(scratch / 'shader-cache')

        def send(*arguments):
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(.2)

        def click(window, x, y):
            send('mousemove', '--window', window, str(x), str(y))
            send('mousedown', '1')
            send('mouseup', '1')

        def wait_control(window, x, y):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Host exited before rendered source control')
                ready, colors = scene_pixels(display, int(window), (x - 2, y - 2, 4, 4))
                if ready:
                    return
                time.sleep(.05)
            raise RuntimeError('Native control did not become ready: ' + repr(sorted(set(colors))))

        def source_button(window):
            # The first blue control in this bound Inspector is Inspect source. A report can
            # add a vertical scrollbar, wrap the UUID onto another line and move the button.
            # Observe its presented position for each gesture; never retry an inspection.
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Host exited before source inspection control')
                pixels = scene_region_pixels(display, int(window), (1010, 80, 1, 110))
                colors = list(zip(pixels[::3], pixels[1::3], pixels[2::3]))
                run = []
                for index, (red, green, blue) in enumerate(colors):
                    if blue >= 105 and blue >= red + 35 and blue >= green + 10:
                        run.append(80 + index * 3)
                        if len(run) == 3:
                            return run[1]
                    else:
                        run.clear()
                time.sleep(.05)
            raise RuntimeError('Source button not presented: ' + repr(colors))

        for read_only in (False, True):
            before_launch = files(root)
            process = launch(args.editor, root, scratch / 'recent', env, read_only=read_only)
            diagnostics = []
            events = queue.Queue()

            def drain():
                for line in process.stderr:
                    diagnostics.append(line)
                    if line.startswith('prefab source inspected '):
                        events.put(line)

            reader = threading.Thread(target=drain, daemon=True)
            reader.start()
            window = wait_for_window(args.xdotool, env)
            send('windowfocus', '--sync', window)
            selection_y = 199 if read_only else 182
            wait_control(window, 520, selection_y + 5)
            click(window, 523, selection_y)

            def inspect(resolved):
                before = files(root)
                click(window, 1058, source_button(window))
                try:
                    event = events.get(timeout=10)
                except queue.Empty as error:
                    raise RuntimeError('Actual source button did not produce a scoped inspection') from error
                require(event.strip() == f'prefab source inspected resolved={resolved} retained=1 published=2',
                        'Native source inspection lost retained/current identity: ' + event)
                require(files(root) == before and scene.read_bytes() == original_scene,
                        'Inspect source changed scene, bindings, archives or project metadata')

            inspect(1)
            # Explicitly refresh after source availability changes; no frame performs source IO.
            archive.unlink()
            inspect(0)
            archive.write_bytes(original_archive)
            inspect(1)
            request_window_close(window, env)
            process.wait(timeout=15)
            reader.join(timeout=5)
            require(not reader.is_alive(), 'Native diagnostic drain did not finish')
            collect_output(process, 5)
            captured = ''.join(diagnostics)
            print(captured, end='', flush=True)
            require(process.returncode == 0 and 'scene_selected=1' in captured,
                    'Bound native scene inspection/close failed')
            require(not re.search(r'Validation Error\b|VUID-|SYNC-HAZARD-', captured),
                    'Native source inspection reported a Vulkan validation error')
            process = None
            require(scene.read_bytes() == original_scene and archive.read_bytes() == original_archive,
                    'Native close changed bound scene or retained source')
            if read_only:
                require(files(root) == before_launch, 'Read-only inspection wrote project files')
        print('Native bound-source inspection, retained/current revisions, missing/restored source, '
              'read-only reopen and source conservation passed')
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
