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
from LinuxDisplayAcceptance import collect_output, launch, request_window_close, start_xvfb, wait_for_window
from LinuxReflectedInspector import rendered_pixels


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
    found = re.search(rb'^opaque \d+ 301 "Plugin.Arrays" ([0-9a-f]+)$', source, re.M)
    require(found, 'Saved array payload missing')
    require(re.search(rb'^opaque \d+ 999 "Missing.Plugin" 00ff7f$', source, re.M),
            'Native array operation changed unavailable component bytes')
    return bytes.fromhex(found[1].decode('ascii'))


def main():
    parser = argparse.ArgumentParser()
    for name in ('editor', 'fixture', 'xvfb', 'xdotool'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix='nexora-native-arrays-'))
    process = xvfb = None
    try:
        root = scratch / 'project'
        # Use the production workspace/scene fixture, then seed version-2 opaque authoring data.
        # This is fixture setup only; subsequent edits go exclusively through Window/X11 input.
        subprocess.run([args.fixture, '--write-native-fixture', str(root)], check=True)
        scene = root / '.nexora/scenes/Main.scene'
        source, replacements = re.subn(
            rb'^opaque (\d+) 91 "Plugin.Properties" [0-9a-f]+$',
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
                   MESA_SHADER_CACHE_DIR=str(scratch / 'shader-cache'))

        def send(*arguments):
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(.2)

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
            nonlocal process
            process = launch(args.editor, root, scratch / 'recent', env, read_only=read_only)
            window = wait_for_window(args.xdotool, env)
            geometry = subprocess.run([args.xdotool, 'getwindowgeometry', '--shell', window],
                                      env=env, check=True, capture_output=True, text=True).stdout
            require('WIDTH=1280\n' in geometry and 'HEIGHT=720\n' in geometry,
                    'Native array fixture requires initial 1280x720 layout')
            send('windowfocus', '--sync', window)
            y = 199 if read_only else 182
            wait_pixels(window, (518, y - 4, 10, 10), blue, 'Scene select-all control')
            send('mousemove', '--window', window, '523', str(y))
            send('mousedown', '1')
            send('mouseup', '1')
            # Add/Clear have stable positions above the clipped variable-length property child.
            wait_pixels(window, (1016, 101, 12, 12),
                        lambda p: len(set(p)) > 1, 'selected array Add control')
            return window

        def close_host(window):
            nonlocal process
            request_window_close(window, env)
            _, diagnostics = collect_output(process, 15)
            require(process.returncode == 0 and 'scene_selected=1' in diagnostics,
                    'Native array close lost source selection or failed')
            require(not re.search(r'Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+',
                                  diagnostics), 'Native array Vulkan validation error')
            process = None

        window = open_host()
        click(window, 1023, 107)  # Add a typed zero to the current tail.
        save([1., 2., 0.])
        added = scene.read_bytes()
        key('ctrl+z')
        save([1., 2.])
        require(scene.read_bytes() == initial, 'Native Add Undo was not one exact transaction')
        key('ctrl+y')
        save([1., 2., 0.])
        require(scene.read_bytes() == added, 'Native Add Redo changed unrelated bytes')
        click(window, 1100, 160)  # Remove first common element.
        save([2., 0.])
        key('ctrl+z')
        save([1., 2., 0.])
        key('ctrl+z')
        save([1., 2.])
        click(window, 1181, 160)  # Down on row zero.
        save([2., 1.])
        click(window, 1144, 206)  # Up on row one.
        save([1., 2.])
        click(window, 1042, 160)  # Insert default before row zero.
        save([0., 1., 2.])
        click(window, 1100, 160)
        save([1., 2.])
        click(window, 1066, 107)  # Clear all elements.
        save([])
        key('ctrl+z')
        save([1., 2.])
        key('ctrl+y')
        save([])
        key('ctrl+z')
        save([1., 2.])
        # Actual typed element edit, then restart in a fresh native process.
        send('mousemove', '--window', window, '1230', '137')
        send('mousedown', '1')
        send('mouseup', '1')
        key('ctrl+a')
        send('type', '--clearmodifiers', '--delay', '20', '--', '3.5')
        key('Return')
        save([3.5, 2.])
        close_host(window)
        before = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        window = open_host(True)
        click(window, 1023, 107, enabled=False)
        key('ctrl+s')
        close_host(window)
        require(before == {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()},
                'Read-only native array reopen changed source/metadata')
        require(payload(scene) == wire([3.5, 2.]), 'Native array reopen lost persisted values')
        window = open_host()
        click(window, 1066, 107)
        save([])
        key('ctrl+z')
        save([3.5, 2.])
        close_host(window)
        print('Native array Add/Insert/Remove/Move/Clear/edit/Undo/Redo/save/reopen/readonly passed')
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.communicate(timeout=10)
        if xvfb is not None:
            if xvfb.poll() is None:
                xvfb.terminate()
            xvfb.communicate(timeout=10)
        shutil.rmtree(scratch)


if __name__ == '__main__':
    main()
