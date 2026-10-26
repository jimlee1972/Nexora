#!/usr/bin/env python3
"""Exercise counted-array controls through the actual native window and scene files."""
import argparse
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import time
from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from LinuxReflectedInspector import rendered_pixels, native_point, wait_native_pixels
from NativeSceneInput import NativeSceneInput


def require(value, message):
    if not value:
        raise RuntimeError(message)


def wire(values):
    prefix = bytearray(16)
    prefix[:8] = struct.pack('<Q', len(values))
    prefix[15] = 255  # Unknown prefix bytes must survive every structural operation.
    return bytes(prefix) + struct.pack('<' + 'd' * len(values), *values)


def payload(scene):
    source = scene.read_bytes()
    found = re.search(rb'^opaque (\d+) 301 "Plugin\.Arrays" ([0-9a-f]+)$', source, re.M)
    require(found, 'Saved array payload missing')
    require(b'opaque ' + found[1] + b' 999 "Missing.Plugin" 00ff7f' in source.splitlines(),
            'Native array operation changed unavailable component bytes')
    return bytes.fromhex(found[2].decode('ascii'))


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix='nexora-native-arrays-'))
    process = xvfb = driver = None
    launch_number = 0
    try:
        root = scratch / 'project'
        # Use the production workspace/scene fixture, then seed version-2 opaque authoring data.
        # This is fixture setup only; subsequent edits go exclusively through Window/X11 input.
        subprocess.run([args.fixture, '--write-native-fixture', str(root)], check=True)
        scene = root / '.nexora/scenes/Main.scene'
        source, replacements = re.subn(
            rb'^opaque (\d+) 91 "Plugin\.Properties" [0-9a-f]+$',
            lambda m: b'opaque ' + m[1] + b' 301 "Plugin.Arrays" ' + wire([1., 2.]).hex().encode() +
                      b'\nopaque ' + m[1] + b' 999 "Missing.Plugin" 00ff7f',
            scene.read_bytes(), flags=re.M)
        require(replacements == 1, 'Native array fixture owner is ambiguous')
        scene.write_bytes(source)
        schema = root / '.nexora/inspector.reflection'
        schema.write_bytes(b'NXEDITORREFLECTION 2\ntype 301 "Plugin.Arrays" 16 0 1\n'
                           b'array "samples" double 0 4 0\n')
        original_schema = schema.read_bytes()
        initial = scene.read_bytes()
        xvfb, display = start_xvfb(args.xvfb, '1280x720x24')
        require(display is not None, 'Xvfb failed')
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(scratch / 'state'),
                   MESA_SHADER_CACHE_DIR=str(scratch / 'shader-cache'),
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY='1')

        def send(*arguments):
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            if driver is None:
                time.sleep(.2)
            else:
                driver.drain(.2)

        def key(chord):
            parts = chord.split('+')
            held = ['Control_L'] if parts[:-1] == ['ctrl'] else []
            try:
                for modifier in held:
                    send('keydown', modifier)
                send('keydown', parts[-1])
                send('keyup', parts[-1])
            finally:
                for modifier in reversed(held):
                    send('keyup', modifier)

        def pixels(window, rectangle):
            return rendered_pixels(display, window, rectangle)

        def wait_pixels(window, rectangle, predicate, description):
            deadline = time.monotonic() + 10
            observed = []
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Editor exited before ' + description)
                observed = pixels(window, rectangle)
                if predicate(observed):
                    return observed
                time.sleep(.05)
            raise RuntimeError('Native ' + description + ' not presented; colors=' +
                               repr(sorted(set(observed))))

        def blue(observed):
            return sum(b >= r + 25 and g >= r + 10 for r, g, b in observed) >= 10

        def caret(observed):
            # Four columns immediately after the initial single digit; an active caret has
            # a tall white stroke, unlike the digit's trailing antialiased pixels.
            return any(sum(min(observed[row * 4 + column]) >= 200 for row in range(17)) >= 9
                       for column in range(4))

        def click(window, x, y, enabled=True):
            patch = (x - 8, y - 6, 16, 12)
            before = pixels(window, patch)
            send('mousemove', '--window', window, str(x), str(y))
            if enabled:
                hovered = wait_pixels(window, patch, lambda p: p != before and blue(p),
                                      'array button hover')
            send('mousedown', '1')
            try:
                if enabled:
                    held = wait_pixels(window, patch, lambda p: p != hovered and blue(p),
                                       'array button held')
            finally:
                send('mouseup', '1')
            if enabled:
                wait_pixels(window, patch, lambda p: p != held, 'array button release')

        def save(values):
            expected = wire(values)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Editor exited before array Save')
                key('ctrl+s')
                if payload(scene) == expected:
                    require(schema.read_bytes() == original_schema, 'Inspector rewrote schema')
                    return
            raise RuntimeError('Native array Save mismatch: expected=' + expected.hex() +
                               ' actual=' + payload(scene).hex())

        def open_host(read_only=False):
            nonlocal process, driver, launch_number
            launch_number += 1
            process = launch(args.editor, root, scratch / 'recent', env, read_only=read_only, binary=True)
            driver = NativeSceneInput(process, args.xdotool, env, '', scratch, launch_number)
            window = wait_for_window(args.xdotool, env)
            geometry = subprocess.run([args.xdotool, 'getwindowgeometry', '--shell', window],
                                      env=env, check=True, capture_output=True, text=True).stdout
            require('WIDTH=1280\n' in geometry and 'HEIGHT=720\n' in geometry,
                    'Native array fixture requires initial 1280x720 layout')
            send('windowfocus', '--sync', window)
            x, y = wait_native_pixels(driver, display, window, 'scene_select_all')
            send('mousemove', '--window', window, str(x), str(y))
            send('mousedown', '1')
            send('mouseup', '1')
            # Observe the actual Add artwork above the variable-length property child.
            wait_native_pixels(driver, display, window, 'array_add', icon=not read_only)
            return window

        def close_host(window):
            nonlocal process, driver
            request_window_close(window, env)
            _, diagnostics = driver.finish(15)
            require(process.returncode == 0 and 'scene_selected=1' in diagnostics,
                    'Native array close lost source selection or failed')
            require(not re.search(r'Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+',
                                  diagnostics), 'Native array Vulkan validation error')
            process = driver = None

        window = open_host()
        click(window, *native_point(driver, 'array_add'))  # Add a typed zero to the current tail.
        save([1., 2., 0.])
        added = scene.read_bytes()
        key('ctrl+z')
        save([1., 2.])
        require(scene.read_bytes() == initial, 'Native Add Undo was not one exact transaction')
        key('ctrl+y')
        save([1., 2., 0.])
        require(scene.read_bytes() == added, 'Native Add Redo changed unrelated bytes')
        click(window, *native_point(driver, 'array_remove_0'))  # Remove first common element.
        save([2., 0.])
        key('ctrl+z')
        save([1., 2., 0.])
        key('ctrl+z')
        save([1., 2.])
        click(window, *native_point(driver, 'array_down_0'))  # Down on row zero.
        save([2., 1.])
        click(window, *native_point(driver, 'array_up_1'))  # Up on row one.
        save([1., 2.])
        click(window, *native_point(driver, 'array_insert_0'))  # Insert default before row zero.
        save([0., 1., 2.])
        click(window, *native_point(driver, 'array_remove_0'))
        save([1., 2.])
        click(window, *native_point(driver, 'array_clear'))  # Clear all elements.
        save([])
        key('ctrl+z')
        save([1., 2.])
        key('ctrl+y')
        save([])
        key('ctrl+z')
        save([1., 2.])
        # Actual typed element edit, then restart in a fresh native process.
        # This ImGui InputText version keeps a fixed frame color. Observe its caret rather
        # than treating a hover/background change as evidence of text-input focus.
        x, y = native_point(driver, 'array_value_0')
        # The reflected InputText is 180 logical pixels wide. Its right side can lie
        # beneath the child's scrollbar, so click the visible first third of the field.
        value_left = x - 90
        send('mousemove', '--window', window, str(value_left + 60), str(y))
        send('mousedown', '1')
        try:
            wait_pixels(window, (value_left + 8, y - 8, 4, 17), caret, 'numeric input caret focus')
        finally:
            send('mouseup', '1')
        key('ctrl+a')
        send('type', '--clearmodifiers', '--delay', '20', '--', '3.5')
        # The last digit lies beyond the old single digit/caret. Observe actual draft text
        # before Return; no edit is repeated if input/focus delivery fails on a slow frame.
        wait_pixels(window, (value_left + 17, y - 7, 7, 14),
                    lambda p: sum(min(pixel) >= 200 for pixel in p) >= 8,
                    'typed numeric draft')
        key('Return')
        save([3.5, 2.])
        close_host(window)
        before = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        window = open_host(True)
        click(window, *native_point(driver, 'array_add'), enabled=False)
        key('ctrl+s')
        close_host(window)
        require(before == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Read-only native array reopen changed source/metadata')
        require(payload(scene) == wire([3.5, 2.]), 'Native array reopen lost persisted values')
        window = open_host()
        click(window, *native_point(driver, 'array_clear'))
        save([])
        key('ctrl+z')
        save([3.5, 2.])
        close_host(window)
        print('Native array Add/Insert/Remove/Move/Clear/edit/Undo/Redo/save/reopen/readonly passed')
    finally:
        if process is not None:
            if process.poll() is None:
                process.kill()
            _, diagnostics = process.communicate(timeout=10)
            print(diagnostics[-8192:], flush=True)
        if xvfb is not None:
            if xvfb.poll() is None:
                xvfb.terminate()
            xvfb.communicate(timeout=10)
        shutil.rmtree(scratch)


if __name__ == '__main__':
    main()
