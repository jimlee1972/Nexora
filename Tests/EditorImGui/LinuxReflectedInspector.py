#!/usr/bin/env python3
"""Edit/save/undo/reopen an actual reflected component through the native host."""
import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import time
from LinuxDisplayAcceptance import collect_output, launch, request_window_close, start_xvfb, wait_for_window


def require(value, message):
    if not value:
        raise RuntimeError(message)


def payload(scene):
    found = re.search(rb'^opaque \d+ 91 "Plugin.Properties" ([0-9a-f]+)$', scene.read_bytes(), re.M)
    require(found is not None, 'Saved component payload missing')
    return bytes.fromhex(found[1].decode('ascii'))


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix='nexora-native-reflected-'))
    root = scratch / 'project'
    subprocess.run([args.fixture, '--write-native-fixture', str(root)], check=True)
    scene = root / '.nexora/scenes/Main.scene'
    initial = scene.read_bytes()
    original_schema = (root / '.nexora/inspector.reflection').read_bytes()
    xvfb, display = start_xvfb(args.xvfb, '1600x1200x24')
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

        def key(*keys):
            send('key', '--clearmodifiers', '--delay', '100', *keys)

        def click(window, x, y):
            # Deliver a hovered frame before the button event, including across docked windows.
            send('mousemove', '--window', window, str(x), str(y))
            # Keep press/release observable in separate frames on slower software renderers.
            send('mousedown', '1')
            send('mouseup', '1')

        def save_until(expected):
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Editor exited before save')
                key('ctrl+s')
                if payload(scene) == expected:
                    return
            raise RuntimeError('Actual reflected source did not save expected bytes: '
                               f'expected={expected.hex()} actual={payload(scene).hex()}')

        def open_host(read_only=False):
            nonlocal process
            process = launch(args.editor, root, scratch / 'recent', env, read_only=read_only)
            window = wait_for_window(args.xdotool, env)
            send('windowsize', '--sync', window, '1600', '1200')
            send('windowfocus', '--sync', window)
            time.sleep(.8)
            # Select the existing source through the actual Hierarchy; metadata does not select it.
            click(window, 595, 199 if read_only else 182)
            # Selection changes the Inspector contents; allow that layout to settle before edits.
            time.sleep(.8)
            return window

        def close_host(window):
            nonlocal process
            request_window_close(window, env)
            _, diagnostics = collect_output(process, 15)
            require('scene_selected=1' in diagnostics, 'Native fixture did not inspect the actual source selection')
            require(process.returncode == 0, 'Native reflected host close failed')
            process = None

        window = open_host()
        # The top Inspector field is the actual schema-backed Boolean control.
        click(window, 1428, 97)
        expected = bytearray(payload(scene))
        expected[0] = 1
        save_until(expected)
        edited = scene.read_bytes()
        key('ctrl+z')
        save_until(bytes([0]) + bytes(expected[1:]))
        require(scene.read_bytes() == initial, 'Undo did not restore exact scene source')
        key('ctrl+y')
        save_until(expected)
        require(scene.read_bytes() == edited and payload(scene)[23] == 255,
                'Redo changed padding or unrelated source bytes')
        click(window, 1510, 120)
        key('ctrl+a')
        send('type', '--clearmodifiers', '--delay', '20', '--', '6.75')
        key('Return')
        expected[8:16] = struct.pack('<d', 6.75)
        save_until(expected)
        require(payload(scene)[23] == 255, 'Numeric edit changed unknown padding')
        close_host(window)
        before = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        window = open_host(True)
        click(window, 1428, 97)
        key('ctrl+s')
        close_host(window)
        require(before == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Read-only reflected restart wrote source/metadata')
        require((root / '.nexora/inspector.reflection').read_bytes() == original_schema,
                'Inspector rewrote reflection metadata')
        schema = root / '.nexora/inspector.reflection'
        schema.write_bytes(b'NXEDITORREFLECTION 99\n')
        corrupt_before = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        process = launch(args.editor, root, scratch / 'recent', env, frames=80, read_only=True)
        _, diagnostics = collect_output(process, 20)
        require(process.returncode == 0 and 'Inspector reflection metadata rejected' in diagnostics,
                'Unsupported metadata did not revoke interpretation')
        process = None
        require(corrupt_before == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Invalid metadata restart changed source')
        schema.write_bytes(original_schema)
        window = open_host()
        click(window, 1428, 97)
        expected[0] = 0
        save_until(expected)
        close_host(window)
        require(payload(scene)[23] == 255 and schema.read_bytes() == original_schema,
                'Compatible metadata restoration lost unknown bytes')
        print('Native reflected edit/Undo/Redo/save/reopen/readonly/format restoration passed')
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.communicate(timeout=10)
        if xvfb.poll() is None:
            xvfb.terminate()
        xvfb.communicate(timeout=10)
        shutil.rmtree(scratch)


if __name__ == '__main__':
    main()
