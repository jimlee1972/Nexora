#!/usr/bin/env python3
"""Check the real Editor's docked native Scene pixels and draw submission under Xvfb."""

import argparse
import ctypes
import math
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import start_xvfb, wait_for_window


class XImage(ctypes.Structure):
    _fields_ = [
        ("width", ctypes.c_int), ("height", ctypes.c_int),
        ("xoffset", ctypes.c_int), ("format", ctypes.c_int),
        ("data", ctypes.c_void_p), ("byte_order", ctypes.c_int),
        ("bitmap_unit", ctypes.c_int), ("bitmap_bit_order", ctypes.c_int),
        ("bitmap_pad", ctypes.c_int), ("depth", ctypes.c_int),
        ("bytes_per_line", ctypes.c_int), ("bits_per_pixel", ctypes.c_int),
        ("red_mask", ctypes.c_ulong), ("green_mask", ctypes.c_ulong),
        ("blue_mask", ctypes.c_ulong),
    ]


def channel(pixel: int, mask: int) -> int:
    shift = (mask & -mask).bit_length() - 1
    value = (pixel & mask) >> shift
    maximum = mask >> shift
    return value * 255 // maximum


def scene_pixels(display_name: str, window: int, viewport: tuple[int, int, int, int]):
    x11 = ctypes.CDLL("libX11.so.6")
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XGetImage.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                              ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong, ctypes.c_int]
    x11.XGetImage.restype = ctypes.POINTER(XImage)
    x11.XGetPixel.argtypes = [ctypes.POINTER(XImage), ctypes.c_int, ctypes.c_int]
    x11.XGetPixel.restype = ctypes.c_ulong
    x11.XDestroyImage.argtypes = [ctypes.POINTER(XImage)]
    x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(display_name.encode())
    if not display:
        raise RuntimeError("Xvfb pixel reader could not connect")
    try:
        x11.XSync(display, 0)
        image = x11.XGetImage(display, window, 0, 0, 1280, 720,
                              ctypes.c_ulong(-1).value, 2)
        if not image:
            raise RuntimeError("Editor window pixel readback failed")
        try:
            x, y, width, height = viewport
            if not (0 <= x < 1280 and 0 <= y < 720 and
                    0 < width <= 1280 - x and 0 < height <= 720 - y):
                raise RuntimeError(f"invalid native Scene viewport: {viewport}")
            samples = []
            visible = False
            for dy in range(-3, 4):
                for dx in range(-3, 4):
                    sx = x + width // 2 + dx * min(width // 16, 16)
                    sy = y + height // 2 + dy * min(height // 16, 16)
                    pixel = x11.XGetPixel(image, sx, sy)
                    red = channel(pixel, image.contents.red_mask)
                    green = channel(pixel, image.contents.green_mask)
                    blue = channel(pixel, image.contents.blue_mask)
                    samples.append((red, green, blue))
                    if blue >= 105 and blue >= red + 35 and blue >= green + 10:
                        visible = True
            return visible, samples
        finally:
            x11.XDestroyImage(image)
    finally:
        x11.XCloseDisplay(display)


def scene_region_pixels(display_name: str, window: int,
                        viewport: tuple[int, int, int, int]):
    x11 = ctypes.CDLL("libX11.so.6")
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
    if not display:
        raise RuntimeError("Xvfb preview reader could not connect")
    try:
        image = x11.XGetImage(display, window, 0, 0, 1280, 720,
                              ctypes.c_ulong(-1).value, 2)
        if not image:
            raise RuntimeError("native rotation preview readback failed")
        try:
            x, y, width, height = viewport
            center_x, center_y = x + width // 2, y + height // 2
            pixels = []
            for py in range(max(y, center_y - 110), min(y + height, center_y + 90), 3):
                for px in range(max(x, center_x - 90), min(x + width, center_x + 90), 3):
                    pixel = x11.XGetPixel(image, px, py)
                    pixels.extend(channel(pixel, mask) for mask in
                                  (image.contents.red_mask, image.contents.green_mask,
                                   image.contents.blue_mask))
            return pixels
        finally:
            x11.XDestroyImage(image)
    finally:
        x11.XCloseDisplay(display)


def focus_root_window(display_name: str):
    x11 = ctypes.CDLL("libX11.so.6")
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
    x11.XDefaultRootWindow.restype = ctypes.c_ulong
    x11.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(display_name.encode())
    if not display:
        raise RuntimeError("focus-loss reader could not connect")
    try:
        x11.XSetInputFocus(display, x11.XDefaultRootWindow(display), 1, 0)
        x11.XFlush(display)
    finally:
        x11.XCloseDisplay(display)


def first_entity_position(scene_text: str) -> tuple[float, float, float]:
    lines = scene_text.splitlines()
    header = next((index for index, line in enumerate(lines)
                   if line.startswith("NEXORA_SCENE 3 ")), None)
    if header is None or header + 1 >= len(lines):
        raise RuntimeError("saved scene has no runtime entity")
    fields = lines[header + 1].split()
    if len(fields) < 5:
        raise RuntimeError("saved scene has a malformed runtime entity")
    return tuple(float(value) for value in fields[2:5])


def first_entity_rotation(scene_text: str) -> tuple[float, float, float, float]:
    lines = scene_text.splitlines()
    header = next(index for index, line in enumerate(lines)
                  if line.startswith("NEXORA_SCENE 3 "))
    return tuple(float(value) for value in lines[header + 1].split()[5:9])


def first_entity_scale(scene_text: str) -> tuple[float, float, float]:
    lines = scene_text.splitlines()
    header = next(index for index, line in enumerate(lines)
                  if line.startswith("NEXORA_SCENE 3 "))
    return tuple(float(value) for value in lines[header + 1].split()[9:12])


def latest_viewport(stream, viewport: tuple[int, int, int, int]):
    while select.select([stream], [], [], 0)[0]:
        output = os.read(stream.fileno(), 4096)
        if not output:
            break
        reports = re.findall(rb"native scene viewport: (\d+) (\d+) (\d+) (\d+)", output)
        if reports:
            viewport = tuple(map(int, reports[-1]))
    return viewport


def settled_viewport(stream, viewport: tuple[int, int, int, int]):
    stable_since = time.monotonic()
    while time.monotonic() - stable_since < 0.4:
        updated = latest_viewport(stream, viewport)
        if updated != viewport:
            viewport = updated
            stable_since = time.monotonic()
        time.sleep(0.05)
    return viewport


def axis_handle_pixels(display_name: str, window: int,
                       viewport: tuple[int, int, int, int], vertical_radius: int = 70):
    x11 = ctypes.CDLL("libX11.so.6")
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
    if not display:
        raise RuntimeError("Xvfb axis pixel reader could not connect")
    try:
        image = x11.XGetImage(display, window, 0, 0, 1280, 720,
                              ctypes.c_ulong(-1).value, 2)
        if not image:
            raise RuntimeError("axis handle pixel readback failed")
        try:
            x, y, width, height = viewport
            center_x, center_y = x + width // 2, y + height // 2
            found = [[], [], []]
            for py in range(max(y, center_y - vertical_radius),
                            min(y + height, center_y + vertical_radius)):
                for px in range(max(x, center_x - 100), min(x + width, center_x + 100)):
                    pixel = x11.XGetPixel(image, px, py)
                    rgb = [channel(pixel, mask) for mask in
                           (image.contents.red_mask, image.contents.green_mask,
                            image.contents.blue_mask)]
                    for axis in range(3):
                        if rgb[axis] > 80 and all(rgb[other] < 60 for other in range(3)
                                                   if other != axis):
                            found[axis].append((px, py))
            return [pixels[len(pixels) // 2] if pixels else None for pixels in found]
        finally:
            x11.XDestroyImage(image)
    finally:
        x11.XCloseDisplay(display)


def uniform_handle_pixel(display_name: str, window: int,
                         viewport: tuple[int, int, int, int]):
    x11 = ctypes.CDLL("libX11.so.6")
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
    if not display:
        raise RuntimeError("Xvfb uniform-handle reader could not connect")
    try:
        image = x11.XGetImage(display, window, 0, 0, 1280, 720,
                              ctypes.c_ulong(-1).value, 2)
        if not image:
            raise RuntimeError("uniform-handle pixel readback failed")
        try:
            x, y, width, height = viewport
            center_x, center_y = x + width // 2, y + height // 2
            found = []
            for py in range(max(y, center_y - 100), min(y + height, center_y + 100)):
                for px in range(max(x, center_x - 100), min(x + width, center_x + 100)):
                    pixel = x11.XGetPixel(image, px, py)
                    rgb = [channel(pixel, mask) for mask in
                           (image.contents.red_mask, image.contents.green_mask,
                            image.contents.blue_mask)]
                    if min(rgb) > 180 and max(rgb) - min(rgb) < 35:
                        found.append((px, py))
            return found[len(found) // 2] if found else None
        finally:
            x11.XDestroyImage(image)
    finally:
        x11.XCloseDisplay(display)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", required=True)
    parser.add_argument("--xvfb", required=True)
    parser.add_argument("--xdotool", required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix="nexora-native-scene-"))
    user_state = Path(tempfile.mkdtemp(prefix="nexora-native-scene-state-"))
    (root / "Content").mkdir()
    (root / ".nexora").mkdir()
    (root / "project.nexora").write_text("schema=1\nname=Native Scene Acceptance\n")
    (root / ".nexora/workspace").write_text("schema=1\n")
    xvfb, display = start_xvfb(args.xvfb, "1280x720x24")
    editor = None
    try:
        if display is None:
            raise RuntimeError("Xvfb did not become ready")
        environment = os.environ.copy()
        environment["DISPLAY"] = display
        environment["XDG_STATE_HOME"] = str(user_state)
        editor = subprocess.Popen(
            [args.editor, f"--project={root}", "--graphical", "--native-scene-preview",
             "--frames=10000", f"--recent-projects={user_state / 'recent-projects'}"],
            env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        window = int(wait_for_window(args.xdotool, environment))
        deadline = time.monotonic() + 12
        captured = b""
        viewport = None
        while time.monotonic() < deadline and viewport is None:
            if not select.select([editor.stderr], [], [], 0.5)[0]:
                continue
            captured += os.read(editor.stderr.fileno(), 4096)
            match = re.search(rb"native scene viewport: (\d+) (\d+) (\d+) (\d+)", captured)
            if match:
                viewport = tuple(map(int, match.groups()))
        if viewport is None:
            raise RuntimeError(f"native Scene draw did not start: {captured.decode(errors='replace')}")
        pixels_visible = False
        samples = []
        deadline = time.monotonic() + 6
        while time.monotonic() < deadline and not pixels_visible:
            pixels_visible, samples = scene_pixels(display, window, viewport)
            if not pixels_visible:
                time.sleep(0.05)
        if not pixels_visible:
            raise RuntimeError(f"native Scene pixels were hidden inside viewport {viewport}; "
                               f"samples={samples}")
        subprocess.run([args.xdotool, "windowfocus", str(window)], env=environment, check=True)
        center_x = viewport[0] + viewport[2] // 2
        center_y = viewport[1] + viewport[3] // 2
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "click", "1"], env=environment, check=True)
        time.sleep(0.15)
        scene_file = root / ".nexora/scenes/Main.scene"
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and not scene_file.is_file():
            time.sleep(0.05)
        if not scene_file.is_file():
            raise RuntimeError("native Scene could not save before proxy drag")
        initial_scene = scene_file.read_text()
        # Selection changes the docked panel layout. Wait for the new physical-pixel
        # viewport before sampling handle pixels or sending mouse input.
        viewport = settled_viewport(editor.stderr, viewport)
        center_x = viewport[0] + viewport[2] // 2
        center_y = viewport[1] + viewport[3] // 2
        handles = [None, None, None]
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline and any(handle is None for handle in handles):
            handles = axis_handle_pixels(display, window, viewport)
            if any(handle is None for handle in handles):
                time.sleep(0.05)
        if any(handle is None for handle in handles):
            raise RuntimeError(f"selected proxy lacks visible XYZ axis handles: {handles}")
        before = first_entity_position(initial_scene)
        for axis, (dx, dy) in enumerate(((40, 0), (0, -40), (-30, 25))):
            viewport = settled_viewport(editor.stderr, viewport)
            handles = axis_handle_pixels(display, window, viewport)
            if handles[axis] is None:
                raise RuntimeError(f"axis handle {axis} disappeared after layout update")
            handle_x, handle_y = handles[axis]
            subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                            str(handle_x), str(handle_y)], env=environment, check=True)
            time.sleep(0.2)
            subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
            time.sleep(0.1)
            subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                            str(handle_x + dx), str(handle_y + dy)], env=environment, check=True)
            time.sleep(0.15)
            subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
            subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
            after = before
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and abs(after[axis] - before[axis]) < 0.1:
                time.sleep(0.05)
                after = first_entity_position(scene_file.read_text())
            if (abs(after[axis] - before[axis]) < 0.1 or
                    any(abs(after[other] - before[other]) > 1e-6 for other in range(3)
                        if other != axis)):
                raise RuntimeError(f"XYZ handle {axis} did not constrain movement: "
                                   f"{before} -> {after}; handle={handles[axis]}, "
                                   f"viewport={viewport}; "
                                   f"visible={axis_handle_pixels(display, window, viewport)}; "
                                   f"latest={latest_viewport(editor.stderr, viewport)}")
            subprocess.run([args.xdotool, "key", "ctrl+z"], env=environment, check=True)
            subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and scene_file.read_text() != initial_scene:
                time.sleep(0.05)
            if scene_file.read_text() != initial_scene:
                # On a loaded virtual display the first save can run before the queued
                # Undo command is applied. Save the settled document once more, without
                # issuing another Undo that could change an earlier transaction.
                subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline and scene_file.read_text() != initial_scene:
                    time.sleep(0.05)
            if scene_file.read_text() != initial_scene:
                raise RuntimeError(f"axis handle {axis} drag did not undo atomically; "
                                   f"saved={scene_file.read_text()!r}; expected={initial_scene!r}")
        viewport = settled_viewport(editor.stderr, viewport)
        center_x = viewport[0] + viewport[2] // 2
        center_y = viewport[1] + viewport[3] // 2
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y)], env=environment, check=True)
        _, before_drag_pixels = scene_pixels(display, window, viewport)
        subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x + 48), str(center_y + 24)], env=environment, check=True)
        deadline = time.monotonic() + 3
        pixels_changed = False
        while time.monotonic() < deadline and not pixels_changed:
            _, preview_pixels = scene_pixels(display, window, viewport)
            pixels_changed = sum(abs(a - b) for before, after in
                                 zip(before_drag_pixels, preview_pixels)
                                 for a, b in zip(before, after)) > 100
            if not pixels_changed:
                time.sleep(0.05)
        if not pixels_changed:
            raise RuntimeError("native proxy did not visibly move before mouse release")
        subprocess.run([args.xdotool, "key", "Escape"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        time.sleep(0.2)
        if scene_file.read_text() != initial_scene:
            raise RuntimeError("Escape did not cancel the native proxy drag")
        # A real FocusOut while the mouse is held must cancel, including the input
        # releases synthesized by Dear ImGui on the next frame.
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x + 48), str(center_y + 24)], env=environment, check=True)
        time.sleep(0.2)
        focus_root_window(display)
        time.sleep(0.2)
        subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
        subprocess.run([args.xdotool, "windowfocus", str(window)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        time.sleep(0.2)
        if scene_file.read_text() != initial_scene:
            raise RuntimeError("focus loss committed the prospective native proxy move")
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y)], env=environment, check=True)
        subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x + 48), str(center_y + 24)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() == initial_scene:
            time.sleep(0.05)
        if scene_file.read_text() == initial_scene:
            raise RuntimeError("native proxy drag did not change the saved scene")
        subprocess.run([args.xdotool, "key", "ctrl+z"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() != initial_scene:
            time.sleep(0.05)
        if scene_file.read_text() != initial_scene:
            raise RuntimeError("native proxy drag did not undo before vertical movement")
        viewport = settled_viewport(editor.stderr, viewport)
        center_x = viewport[0] + viewport[2] // 2
        center_y = viewport[1] + viewport[3] // 2
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y)], env=environment, check=True)
        subprocess.run([args.xdotool, "keydown", "Shift_L"], env=environment, check=True)
        subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y - 48)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
        subprocess.run([args.xdotool, "keyup", "Shift_L"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        before = first_entity_position(initial_scene)
        after = before
        while time.monotonic() < deadline and abs(after[1] - before[1]) < 0.1:
            time.sleep(0.05)
            after = first_entity_position(scene_file.read_text())
        if (abs(after[1] - before[1]) < 0.1 or abs(after[0] - before[0]) > 1e-6 or
                abs(after[2] - before[2]) > 1e-6):
            raise RuntimeError(f"Shift-drag did not move only world Y: {before} -> {after}")
        before_rotation = scene_file.read_text()
        viewport = settled_viewport(editor.stderr, viewport)
        center_x = viewport[0] + viewport[2] // 2
        center_y = viewport[1] + viewport[3] // 2
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "key", "e"], env=environment, check=True)
        time.sleep(0.2)
        rotation_handle = axis_handle_pixels(display, window, viewport)[1]
        if rotation_handle is None:
            raise RuntimeError("Rotate tool did not show a visible Y ring")
        handle_x, handle_y = rotation_handle
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(handle_x), str(handle_y)], env=environment, check=True)
        time.sleep(0.2)
        before_rotation_pixels = scene_region_pixels(display, window, viewport)
        subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(handle_x + 28), str(handle_y + 20)], env=environment, check=True)
        deadline = time.monotonic() + 3
        rotation_visible = False
        while time.monotonic() < deadline and not rotation_visible:
            preview_pixels = scene_region_pixels(display, window, viewport)
            rotation_visible = sum(abs(a - b) for a, b in
                                   zip(before_rotation_pixels, preview_pixels)) > 400
            if not rotation_visible:
                time.sleep(0.05)
        if not rotation_visible:
            raise RuntimeError("rotation ring did not visibly preview before mouse release")
        if scene_file.read_text() != before_rotation:
            raise RuntimeError("rotation preview committed before mouse release")
        subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        rotated = before_rotation
        while time.monotonic() < deadline and rotated == before_rotation:
            time.sleep(0.05)
            rotated = scene_file.read_text()
        quaternion = first_entity_rotation(rotated)
        if (abs(quaternion[1]) < 0.03 or first_entity_position(rotated) != after):
            raise RuntimeError(f"Y rotation ring did not commit an in-place turn: {quaternion}")
        subprocess.run([args.xdotool, "key", "ctrl+z"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() != before_rotation:
            time.sleep(0.05)
        if scene_file.read_text() != before_rotation:
            raise RuntimeError("Y rotation ring did not undo atomically")
        viewport = settled_viewport(editor.stderr, viewport)
        snapped_ring = axis_handle_pixels(display, window, viewport)[1]
        if snapped_ring is None:
            raise RuntimeError("Y rotation ring disappeared before snapped drag")
        handle_x, handle_y = snapped_ring
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(handle_x), str(handle_y)], env=environment, check=True)
        subprocess.run([args.xdotool, "keydown", "Shift_L"], env=environment, check=True)
        subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(handle_x + 28), str(handle_y + 20)], env=environment, check=True)
        time.sleep(0.2)
        subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
        subprocess.run([args.xdotool, "keyup", "Shift_L"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() == before_rotation:
            time.sleep(0.05)
        snapped_rotation = first_entity_rotation(scene_file.read_text())
        snapped_angle = 2.0 * math.atan2(abs(snapped_rotation[1]),
                                         abs(snapped_rotation[3]))
        snap_units = snapped_angle / (math.pi / 12.0)
        if snap_units < 0.5 or abs(snap_units - round(snap_units)) > 1e-5:
            raise RuntimeError(f"Shift rotation did not snap to 15 degrees: "
                               f"{snapped_rotation}")
        subprocess.run([args.xdotool, "key", "ctrl+z"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() != before_rotation:
            time.sleep(0.05)
        if scene_file.read_text() != before_rotation:
            raise RuntimeError("snapped Y rotation did not undo atomically")
        viewport = settled_viewport(editor.stderr, viewport)
        center_x = viewport[0] + viewport[2] // 2
        center_y = viewport[1] + viewport[3] // 2
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "key", "r"], env=environment, check=True)
        time.sleep(0.2)
        for axis, (dx, dy) in enumerate(((30, 0), (0, -30), (-25, 15))):
            viewport = settled_viewport(editor.stderr, viewport)
            scale_handle = axis_handle_pixels(display, window, viewport, 130)[axis]
            if scale_handle is None:
                raise RuntimeError(f"Scale tool lacks visible axis {axis} cube")
            handle_x, handle_y = scale_handle
            subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                            str(handle_x), str(handle_y)], env=environment, check=True)
            time.sleep(0.2)
            before_scale_pixels = scene_region_pixels(display, window, viewport)
            subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
            time.sleep(0.1)
            subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                            str(handle_x + dx), str(handle_y + dy)], env=environment, check=True)
            deadline = time.monotonic() + 3
            scale_visible = False
            while time.monotonic() < deadline and not scale_visible:
                preview_pixels = scene_region_pixels(display, window, viewport)
                scale_visible = sum(abs(a - b) for a, b in zip(before_scale_pixels,
                                                                preview_pixels)) > 400
                if not scale_visible:
                    time.sleep(0.05)
            if not scale_visible:
                raise RuntimeError(f"axis {axis} scale drag did not redraw before release")
            if scene_file.read_text() != before_rotation:
                raise RuntimeError("Scale preview changed saved scene before release")
            subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
            subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
            deadline = time.monotonic() + 5
            scaled = before_rotation
            while time.monotonic() < deadline and scaled == before_rotation:
                time.sleep(0.05)
                scaled = scene_file.read_text()
            factors = first_entity_scale(scaled)
            if (factors[axis] < 1.1 or
                    any(abs(factors[other] - 1.0) > 1e-6 for other in range(3)
                        if other != axis) or first_entity_position(scaled) != after):
                raise RuntimeError(f"axis {axis} scale cube did not constrain scale: {factors}")
            subprocess.run([args.xdotool, "key", "ctrl+z"], env=environment, check=True)
            subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and scene_file.read_text() != before_rotation:
                time.sleep(0.05)
            if scene_file.read_text() != before_rotation:
                raise RuntimeError(f"axis {axis} scale cube did not undo atomically")
        viewport = settled_viewport(editor.stderr, viewport)
        uniform_handle = uniform_handle_pixel(display, window, viewport)
        if uniform_handle is None:
            raise RuntimeError("Scale tool lacks a visible uniform cube")
        center_x, center_y = uniform_handle
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y)], env=environment, check=True)
        time.sleep(0.2)
        before_uniform_pixels = scene_region_pixels(display, window, viewport)
        subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y - 40)], env=environment, check=True)
        deadline = time.monotonic() + 3
        uniform_visible = False
        while time.monotonic() < deadline and not uniform_visible:
            preview_pixels = scene_region_pixels(display, window, viewport)
            uniform_visible = sum(abs(a - b) for a, b in zip(before_uniform_pixels,
                                                              preview_pixels)) > 400
            if not uniform_visible:
                time.sleep(0.05)
        if not uniform_visible:
            raise RuntimeError("uniform scale cube did not redraw before release")
        if scene_file.read_text() != before_rotation:
            raise RuntimeError("uniform scale preview changed saved scene before release")
        # Sample the settled preview at the final cursor position. Uniform scaling must
        # render the same geometry on release, including the proxy's fixed Y offset.
        time.sleep(0.15)
        final_uniform_preview = scene_region_pixels(display, window, viewport)
        subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() == before_rotation:
            time.sleep(0.05)
        deadline = time.monotonic() + 3
        release_matches = False
        while time.monotonic() < deadline and not release_matches:
            released_pixels = scene_region_pixels(display, window, viewport)
            release_matches = sum(abs(a - b) for a, b in
                                  zip(final_uniform_preview, released_pixels)) < 100
            if not release_matches:
                time.sleep(0.05)
        if not release_matches:
            raise RuntimeError("uniform scale geometry jumped between preview and commit")
        uniform = first_entity_scale(scene_file.read_text())
        if any(factor < 1.3 for factor in uniform) or max(uniform) - min(uniform) > 1e-6:
            raise RuntimeError(f"uniform cube did not scale all axes equally: {uniform}; "
                               f"handle={uniform_handle}; viewport={viewport}; "
                               f"axis_handles={axis_handle_pixels(display, window, viewport, 130)}")
        subprocess.run([args.xdotool, "key", "ctrl+z"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() != before_rotation:
            time.sleep(0.05)
        if scene_file.read_text() != before_rotation:
            raise RuntimeError("uniform scale cube did not undo atomically")
        viewport = settled_viewport(editor.stderr, viewport)
        snapped_handle = uniform_handle_pixel(display, window, viewport)
        if snapped_handle is None:
            raise RuntimeError("uniform cube disappeared before snapped drag")
        handle_x, handle_y = snapped_handle
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(handle_x), str(handle_y)], env=environment, check=True)
        subprocess.run([args.xdotool, "keydown", "Shift_L"], env=environment, check=True)
        subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(handle_x), str(handle_y - 40)], env=environment, check=True)
        time.sleep(0.2)
        subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
        subprocess.run([args.xdotool, "keyup", "Shift_L"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() == before_rotation:
            time.sleep(0.05)
        snapped = first_entity_scale(scene_file.read_text())
        if any(abs(factor - 1.5) > 1e-6 for factor in snapped):
            raise RuntimeError(f"Shift uniform scale did not snap to 0.25 increments: {snapped}")
        subprocess.run([args.xdotool, "key", "ctrl+z"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() != before_rotation:
            time.sleep(0.05)
        if scene_file.read_text() != before_rotation:
            raise RuntimeError("snapped uniform scale did not undo atomically")
        viewport = settled_viewport(editor.stderr, viewport)
        center_x = viewport[0] + viewport[2] // 2
        center_y = viewport[1] + viewport[3] // 2
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "key", "Delete"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() == before_rotation:
            time.sleep(0.05)
        deleted = scene_file.read_text()
        runtime_header = next((line for line in deleted.splitlines()
                               if line.startswith("NEXORA_SCENE 3 ")), "")
        if not runtime_header or runtime_header.split()[-1] != "0":
            raise RuntimeError(f"Delete over native canvas did not remove selection: "
                               f"{runtime_header!r}")
        subprocess.run([args.xdotool, "key", "ctrl+z"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() != before_rotation:
            time.sleep(0.05)
        if scene_file.read_text() != before_rotation:
            raise RuntimeError("native canvas Delete did not restore scene with Undo")
        center_x = viewport[0] + viewport[2] // 2
        center_y = viewport[1] + viewport[3] // 2
        subprocess.run([args.xdotool, "key", "f"], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "click", "4"], env=environment, check=True)
        subprocess.run([args.xdotool, "mousedown", "3"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x + 40), str(center_y + 30)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "mouseup", "3"], env=environment, check=True)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y)], env=environment, check=True)
        subprocess.run([args.xdotool, "keydown", "Shift_L"], env=environment, check=True)
        subprocess.run([args.xdotool, "mousedown", "2"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(center_x), str(center_y + 48)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "mouseup", "2"], env=environment, check=True)
        subprocess.run([args.xdotool, "keyup", "Shift_L"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and not scene_file.is_file():
            time.sleep(0.05)
        if not scene_file.is_file():
            raise RuntimeError("native Scene smoke could not save the starter scene")
        subprocess.run([args.xdotool, "windowclose", str(window)], env=environment, check=True)
        _, remaining = editor.communicate(timeout=15)
        evidence = captured + remaining
        match = re.search(rb"scene_draws=(\d+)", evidence)
        if (editor.returncode != 0 or not match or int(match[1]) == 0 or
                b"ui_draws=" not in evidence or b"scene_selected=1" not in evidence):
            raise RuntimeError(f"native Scene/UI presentation failed: {evidence.decode(errors='replace')}")
        editor = None
        camera_path = root / ".nexora/scenes/Main.preview.camera"
        camera_lines = camera_path.read_text().splitlines()
        if len(camera_lines) != 2 or camera_lines[0] != "NEXORA_SCENE_CAMERA 1":
            raise RuntimeError(f"native preview camera was not saved: {camera_lines!r}")
        camera_values = [float(value) for value in camera_lines[1].split()]
        if (len(camera_values) != 8 or abs(camera_values[4] - 0.588) < 0.01 or
                camera_values[5] >= 5.0 or camera_values[1] < 0.1 or
                camera_values[6] != 0 or abs(camera_values[0]) > 1e-6 or
                abs(camera_values[2]) > 1e-6):
            raise RuntimeError(f"native preview gestures were not saved: {camera_values!r}")
        editor = subprocess.Popen(
            [args.editor, f"--project={root}", "--graphical", "--native-scene-preview",
             "--frames=8", f"--recent-projects={user_state / 'recent-projects'}"],
            env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        _, reopened_stderr = editor.communicate(timeout=30)
        if editor.returncode != 0 or b"scene_draws=" not in reopened_stderr:
            raise RuntimeError(f"native preview reopen failed: {reopened_stderr.decode(errors='replace')}")
        editor = None
        reopened_values = [float(value) for value in camera_path.read_text().splitlines()[1].split()]
        if reopened_values != camera_values:
            raise RuntimeError("native preview orbit did not survive project reopen")
        return 0
    finally:
        if editor is not None and editor.poll() is None:
            editor.kill()
            editor.wait(timeout=5)
        xvfb.terminate()
        xvfb.wait(timeout=5)
        shutil.rmtree(root, ignore_errors=True)
        shutil.rmtree(user_state, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
