#!/usr/bin/env python3
"""Publish one existing source field through real X11 controls; never synthesize host requests."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from LinuxReflectedInspector import wait_native_pixels
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
    scratch = Path(tempfile.mkdtemp(prefix='nexora-native-instance-source-'))
    process = xvfb = driver = None
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
                   MESA_SHADER_CACHE_DIR=str(scratch / 'shader-cache'),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1')

        def send(*arguments):
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            driver.drain(.2)

        def click(name, icon=True):
            # The first selected frame can reflow the Inspector as its scrollbar appears.
            # Reconcile copied geometry after an acknowledged real hover, before sending
            # the single down/up gesture. Only pointer motion is repeated when layout moves.
            deadline = time.monotonic() + 10
            for _ in range(3):
                point = wait_native_pixels(driver, display, window, name, icon=icon)
                driver.pointer_move(*point, deadline)
                value = driver.wait(lambda v: name in v and v.get('focused') == '1' and
                                              v.get('buttons') == '000',
                                    'Source control was not focused/rendered/released: ' + name,
                                    deadline)
                current = tuple(round(float(axis)) for axis in value[name].split(','))
                if current == point:
                    driver.pointer_button(1, True, deadline)
                    driver.pointer_button(1, False, deadline)
                    return
            raise RuntimeError('Source control geometry did not settle before one click: ' + name)

        def wait(predicate, message):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Native host exited: ' + message)
                if predicate():
                    return
                driver.drain(.05)
            print(bytes(driver.stderr).decode(errors='replace'), flush=True)
            raise RuntimeError(message)

        event_offset = 0

        def pending_events():
            return list(re.finditer(rb'^prefab instance [^\n]*\n', driver.stderr[event_offset:], re.M))

        def take_event():
            nonlocal event_offset
            wait(lambda: bool(pending_events()), 'Native action did not emit an owning result')
            found = pending_events()
            require(len(found) == 1, 'One native action emitted multiple owning results')
            event = found[0]
            event_offset += event.end()
            return event.group().decode().strip()

        for read_only in (False, True):
            before = files(root)
            process = launch(args.editor, root, scratch / 'recent', env, read_only=read_only, binary=True)
            driver = NativeSceneInput(process, args.xdotool, env, '', scratch, int(read_only) + 1)
            event_offset = 0
            window = wait_for_window(args.xdotool, env)
            driver.window = str(window)
            send('windowfocus', '--sync', window)
            click('scene_select_all')
            click('override_review')
            event = take_event()
            require(re.fullmatch(r'prefab instance reviewed rows=[1-9][0-9]* retained=1 published=[12]', event),
                    'Review did not retain the original binding: ' + event)
            if read_only:
                click('override_row_' + str(name_row), icon=False)
                click('override_source_apply', icon=False)
                send('key', 'Escape')
                require(not pending_events() and files(root) == before,
                        'Read-only reopen emitted publication or wrote files')
            else:
                click('override_row_' + str(name_row), icon=False)
                require(scene.read_bytes() == original_scene and current.read_bytes() == retained and
                        archive.read_bytes() == retained, 'Review/selection changed authoring files')
                before_consent = files(root)
                click('override_source_apply')
                wait_native_pixels(driver, display, window, 'override_confirm')
                send('key', 'Escape')
                require(not pending_events() and files(root) == before_consent, 'Source Cancel changed files')
                click('override_source_apply')
                click('override_confirm')
                require(take_event() == 'prefab instance source applied retained=1 published=2 rows=1',
                        'Native confirmation did not publish exactly one selected row')
                wait(lambda: current.read_bytes() == expected, 'Published source lost independent exact-byte oracle')
                require(archive.read_bytes() == retained and scene.read_bytes() == original_scene,
                        'Source publication changed retained archive or live Scene file')
            request_window_close(window, env)
            _, captured = driver.finish(15)
            require(process.returncode == 0 and 'scene_selected=1' in captured and
                    not re.search(r'Validation Error\b|VUID-|SYNC-HAZARD-', captured),
                    'Native source workflow failed or reported Vulkan errors')
            require(scene.read_bytes() == original_scene and archive.read_bytes() == retained,
                    'Closing native host changed Scene or archived source')
            if read_only:
                require(files(root) == before, 'Read-only teardown wrote project files')
            process = driver = None
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
