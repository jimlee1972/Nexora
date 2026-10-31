#!/usr/bin/env python3
"""Edit/save/undo/reopen an actual reflected component through the native host."""
import argparse
from collections import Counter
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
from NativeSceneInput import NativeSceneInput


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


def native_point(driver, name):
    value = driver.wait(lambda v: name in v, 'Native control geometry absent: ' + name)
    return tuple(round(float(axis)) for axis in value[name].split(','))


def wait_native_pixels(driver, display, window, name, icon=True):
    """Require actual X11 artwork at a copied control position before physical input."""
    deadline = time.monotonic() + 10
    observed = []
    while time.monotonic() < deadline:
        value = driver.observation()
        if name in value:
            x, y = (round(float(axis)) for axis in value[name].split(','))
            observed = rendered_pixels(display, window, (x - 10, y - 10, 20, 20))
            blue = sum(b >= r + 20 and b >= g + 5 for r, g, b in observed)
            neutral = sum(max(pixel) - min(pixel) <= 3 and 25 <= pixel[0] <= 235
                          for pixel in observed)
            background = Counter(observed).most_common(1)[0][0]
            # Unity-inspired controls use neutral surfaces. Require actual contrasting artwork,
            # not a particular blue fill or white glyph; Light uses dark foreground strokes.
            artwork = sum(max(abs(channel - base) for channel, base in zip(pixel, background)) >= 70
                          for pixel in observed)
            surface = blue >= 10 or (neutral >= 40 and len(set(observed)) >= 4)
            if surface and (not icon or artwork >= 5):
                return x, y
        require(driver.process.poll() is None, 'Editor exited before native pixels: ' + name)
        driver.drain(.05)
    raise RuntimeError('Native rendered control absent: ' + name + '; colors=' +
                       repr(sorted(set(observed))))


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
    # Match the native host's initial size. Resizing after its first docked frame retains
    # the original split widths, so fixed control coordinates would depend on startup timing.
    xvfb, display = start_xvfb(args.xvfb, '1280x720x24')
    process = driver = None
    launch_number = 0
    try:
        require(display is not None, 'Xvfb failed')
        env = os.environ.copy()
        env['DISPLAY'] = display
        env['XDG_STATE_HOME'] = str(scratch / 'state')
        env['MESA_SHADER_CACHE_DIR'] = str(scratch / 'shader-cache')
        env['NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY'] = '1'

        def send(*arguments):
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            if driver is None:
                time.sleep(.2)
            else:
                driver.drain(.2)

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
            # This fixed native fixture uses the default theme and 1280x720 layout.
            # A background patch outside the checkmark observes actual hovered/held frames.
            patch = (x - 4, y - 5, 3, 3)
            if observe_checkbox:
                wait_rendered(window, patch, lambda r, g, b: r == g == b and 62 <= r <= 82,
                              7, 'Boolean hover frame')
            send('mousedown', '1')
            try:
                if observe_checkbox:
                    wait_rendered(window, patch, lambda r, g, b: r == g == b and 25 <= r <= 40,
                                  7, 'Boolean held frame')
            finally:
                send('mouseup', '1')
            if observe_checkbox:
                wait_rendered(window, patch, lambda r, g, b: r == g == b and 62 <= r <= 82,
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
            nonlocal process, driver, launch_number
            launch_number += 1
            process = launch(args.editor, root, scratch / 'recent', env, read_only=read_only, binary=True)
            driver = NativeSceneInput(process, args.xdotool, env, '', scratch, launch_number)
            window = wait_for_window(args.xdotool, env)
            geometry = subprocess.run([args.xdotool, 'getwindowgeometry', '--shell', window],
                                      env=env, check=True, capture_output=True, text=True).stdout
            require('WIDTH=1280\n' in geometry and 'HEIGHT=720\n' in geometry,
                    'Native reflected fixture requires the initial 1280x720 layout: ' + geometry)
            send('windowfocus', '--sync', window)
            x, y = wait_native_pixels(driver, display, window, 'scene_select_all')
            click(window, x, y)
            x, y = native_point(driver, 'reflected_enabled')
            frame = lambda r, g, b: r == g == b and 35 <= r <= 50
            wait_rendered(window, (x - 4, y - 5, 3, 3), frame, 7,
                          'selected reflected Boolean')
            return window

        def close_host(window):
            nonlocal process, driver
            request_window_close(window, env)
            _, diagnostics = driver.finish(15)
            require('scene_selected=1' in diagnostics, 'Native fixture did not inspect the actual source selection')
            require(process.returncode == 0, 'Native reflected host close failed')
            process = driver = None

        window = open_host()
        # The top Inspector field is the actual schema-backed Boolean control.
        click(window, *native_point(driver, 'reflected_enabled'), observe_checkbox=True)
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
        click(window, *native_point(driver, 'reflected_amount'))
        key('ctrl+a')
        send('type', '--clearmodifiers', '--delay', '20', '--', '6.75')
        key('Return')
        expected[8:16] = struct.pack('<d', 6.75)
        save_until(expected)
        require(payload(scene)[23] == 255, 'Numeric edit changed unknown padding')
        close_host(window)
        before = {p.relative_to(root): p.read_bytes() for p in root.rglob('*') if p.is_file()}
        window = open_host(True)
        click(window, *native_point(driver, 'reflected_enabled'))
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
        click(window, *native_point(driver, 'reflected_enabled'), observe_checkbox=True)
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
