#!/usr/bin/env python3
"""Check the real Editor's docked native Scene pixels and draw submission under Xvfb."""

import argparse
import ctypes
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


def axis_handle_pixels(display_name: str, window: int,
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
            for py in range(max(y, center_y - 70), min(y + height, center_y + 70)):
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
        handles = [None, None, None]
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline and any(handle is None for handle in handles):
            handles = axis_handle_pixels(display, window, viewport)
            if any(handle is None for handle in handles):
                time.sleep(0.05)
        if any(handle is None for handle in handles):
            raise RuntimeError(f"selected proxy lacks visible XYZ axis handles: {handles}")
        handle_x, handle_y = handles[0]
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(handle_x), str(handle_y)], env=environment, check=True)
        subprocess.run([args.xdotool, "mousedown", "1"], env=environment, check=True)
        time.sleep(0.1)
        subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                        str(handle_x + 40), str(handle_y)], env=environment, check=True)
        time.sleep(0.15)
        subprocess.run([args.xdotool, "mouseup", "1"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        before = first_entity_position(initial_scene)
        after = before
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and abs(after[0] - before[0]) < 0.1:
            time.sleep(0.05)
            after = first_entity_position(scene_file.read_text())
        if (abs(after[0] - before[0]) < 0.1 or abs(after[1] - before[1]) > 1e-6 or
                abs(after[2] - before[2]) > 1e-6):
            raise RuntimeError(f"X handle drag did not constrain movement: {before} -> {after}")
        subprocess.run([args.xdotool, "key", "ctrl+z"], env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and scene_file.read_text() != initial_scene:
            time.sleep(0.05)
        if scene_file.read_text() != initial_scene:
            raise RuntimeError("axis handle drag did not undo atomically")
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
                camera_values[5] >= 17.55 or camera_values[1] < 0.1 or
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
