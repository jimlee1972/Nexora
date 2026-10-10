#!/usr/bin/env python3
"""Edit/save/undo/reopen an actual reflected component through the native host."""
import argparse
import ctypes
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import tempfile
import time
from LinuxDisplayAcceptance import collect_output, launch, request_window_close, start_xvfb, wait_for_window
from LinuxNativeScenePreview import XImage, channel


def require(value, message):
    if not value:
        raise RuntimeError(message)


def payload(scene):
    found = re.search(rb'^opaque \d+ 91 "Plugin.Properties" ([0-9a-f]+)$', scene.read_bytes(), re.M)
    require(found is not None, 'Saved component payload missing')
    return bytes.fromhex(found[1].decode('ascii'))


def rendered_pixels(display_name, window, rectangle):
    """Observe the presented X11 window; no synthetic UI state or edit retries."""
    x11 = ctypes.CDLL('libX11.so.6')
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                             ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]
    x11.XGetImage.restype = ctypes.POINTER(XImage)
    x11.XGetPixel.argtypes = [ctypes.POINTER(XImage), ctypes.c_int, ctypes.c_int]
    x11.XGetPixel.restype = ctypes.c_ulong
    x11.XDestroyImage.argtypes = [ctypes.POINTER(XImage)]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(display_name.encode())
    require(display, 'Rendered-control pixel connection failed')
    try:
        x, y, width, height = rectangle
        image = x11.XGetImage(display, int(window), x, y, width, height,
                             ctypes.c_ulong(-1).value, 2)
        require(image, 'Rendered-control pixel readback failed')
        try:
            masks = (image.contents.red_mask, image.contents.green_mask, image.contents.blue_mask)
            return [tuple(channel(x11.XGetPixel(image, px, py), mask) for mask in masks)
                    for py in range(height) for px in range(width)]
        finally:
            x11.XDestroyImage(image)
    finally:
        x11.XCloseDisplay(display)


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
            # Keep shortcut modifiers and the primary key observable across rendered frames.
            modifiers = {'ctrl': 'Control_L', 'alt': 'Alt_L', 'shift': 'Shift_L'}
            for combination in keys:
                parts = combination.split('+')
                held = [modifiers[name] for name in parts[:-1]]
                try:
                    for modifier in held:
                        send('keydown', modifier)
                    send('keydown', parts[-1])
                    send('keyup', parts[-1])
                finally:
                    for modifier in reversed(held):
                        send('keyup', modifier)

        def wait_rendered(window, rectangle, predicate, minimum, description):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                require(process.poll() is None, 'Editor exited before rendered ' + description)
                observed = rendered_pixels(display, window, rectangle)
                if sum(predicate(*pixel) for pixel in observed) >= minimum:
                    return
                time.sleep(.05)
            raise RuntimeError('Rendered ' + description + ' did not become ready; colors=' +
                               repr(sorted(set(observed))))

        def click(window, x, y, observe_checkbox=False):
            # Deliver a hovered frame before the button event, including across docked windows.
            send('mousemove', '--window', window, str(x), str(y))
            # This fixed native fixture uses the default theme and 1600x1200 layout.
            # A background patch outside the checkmark observes actual hovered/held frames.
            patch = (1424, 92, 3, 3)
            if observe_checkbox:
                wait_rendered(window, patch, lambda r, g, b: g >= 120 and b >= 150,
                              7, 'Boolean hover frame')
            send('mousedown', '1')
            try:
                if observe_checkbox:
                    wait_rendered(window, patch, lambda r, g, b: g >= 160 and b >= 200,
                                  7, 'Boolean held frame')
            finally:
                send('mouseup', '1')
            if observe_checkbox:
                wait_rendered(window, patch, lambda r, g, b: 120 <= g < 160 and 150 <= b < 200,
                              7, 'Boolean released frame')

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
            y = 199 if read_only else 182
            blue = lambda r, g, b: b >= r + 25 and g >= r + 10
            # Window creation precedes first presentation. Wait for the actual Scene Select all
            # control in the resized layout before issuing its one physical click.
            wait_rendered(window, (590, y - 4, 10, 10), blue, 10, 'Scene selection control')
            click(window, 595, y)
            wait_rendered(window, (1424, 92, 3, 3), blue, 7, 'selected reflected Boolean')
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
        click(window, 1428, 97, observe_checkbox=True)
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
        click(window, 1428, 97, observe_checkbox=True)
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
