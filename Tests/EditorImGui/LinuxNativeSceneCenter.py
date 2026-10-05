"""Real native gizmo center gestures on two separately selected scene roots."""

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

from LinuxNativeScenePreview import (XImage, axis_handle_pixels, channel,
                                    scene_region_pixels, settled_viewport, settled_scene_preview,
                                    uniform_handle_pixel, undo_and_save)
from LinuxDisplayAcceptance import request_window_close, start_xvfb, wait_for_window


def blue_proxy_pixel(display_name, window, viewport, first=False):
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
        raise RuntimeError("center test pixel reader could not connect")
    try:
        x, y, width, height = viewport
        image = x11.XGetImage(display, window, x, y, width, height,
                             ctypes.c_ulong(-1).value, 2)
        if not image:
            raise RuntimeError("center test pixel readback failed")
        try:
            pixels = []
            for px in range(width):
                for py in range(height):
                    pixel = x11.XGetPixel(image, px, py)
                    r, g, b = (channel(pixel, mask) for mask in
                               (image.contents.red_mask, image.contents.green_mask,
                                image.contents.blue_mask))
                    # Proxy blue has a green channel; blue gizmo handles do not.
                    if b > 110 and g > 75 and b > g + 20 and g > r + 25:
                        pixels.append((x + px, y + py))
            if not pixels:
                return None
            return pixels[len(pixels) // (4 if first else 2)]
        finally:
            x11.XDestroyImage(image)
    finally:
        x11.XCloseDisplay(display)


def root_poses(text):
    lines = text.splitlines()
    header = next(i for i, line in enumerate(lines) if line.startswith("NEXORA_SCENE 3 "))
    count = int(lines[header].split()[-1])
    return {int(fields[0]): tuple(map(float, fields[2:12]))
            for fields in (line.split() for line in lines[header + 1:header + 1 + count])}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--authored-meshes", action="store_true")
    parser.add_argument("--editor", required=True)
    parser.add_argument("--xvfb", required=True)
    parser.add_argument("--xdotool", required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix="nexora-native-center-"))
    user_state = Path(tempfile.mkdtemp(prefix="nexora-center-state-"))
    (root / "Content").mkdir()
    (root / ".nexora/scenes").mkdir(parents=True)
    (root / "project.nexora").write_text("schema=1\nname=Center Acceptance\n")
    (root / ".nexora/workspace").write_text("schema=1\n")
    mesh_ids = (0, 0)
    if args.authored_meshes:
        # Persistent ID golden values shared with the C++ catalog contract, not path hashes.
        mesh_ids = (12751791000609863510, 10105597576554272692)
        (root / "Content/Triangle.obj").write_text(
            "v -0.8 0 0\nv 0.8 0 0\nv 0 1.6 0\nvn 0 1 1\nf 1//1 2//1 3//1\n")
        (root / "Content/Quad.obj").write_text(
            "v -0.55 0 0\nv 0.55 0 0\nv 0.55 1 0\nv -0.55 1 0\nvn 0 1 1\n"
            "f 1//1 2//1 3//1\nf 1//1 3//1 4//1\n")
        for filename, suffix in (("Triangle.obj", "0"), ("Quad.obj", "1")):
            (root / ("Content/" + filename + ".meta")).write_text(
                "schema=1\nuuid=12345678-9abc-def0-fedc-ba987654321" + suffix + "\ntype=.obj\n")
    scene_file = root / ".nexora/scenes/Main.scene"
    scene_file.write_text(
        'NEXORA_EDITOR_SCENE 2\nnode 10 0 Left\nnode 20 0 Right\nworld\n'
        'NEXORA_SCENE 3 "Center scene" 0 2\n'
        f'10 0 -2 1 0 0 0 0 1 1 1 1 0 0 {int(args.authored_meshes)} 60 0.1 1000 1 {mesh_ids[0]} 0\n'
        f'20 0 2 1 0 0 0 0 1 1 1 1 0 0 {int(args.authored_meshes)} 60 0.1 1000 1 {mesh_ids[1]} 0\n')
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
            env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        window = int(wait_for_window(args.xdotool, environment))
        captured = b""
        viewport = None
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline and viewport is None:
            if select.select([editor.stderr], [], [], 0.5)[0]:
                captured += os.read(editor.stderr.fileno(), 4096)
                match = re.search(rb"native scene viewport: (\d+) (\d+) (\d+) (\d+)", captured)
                if match:
                    viewport = tuple(map(int, match.groups()))
        if viewport is None:
            raise RuntimeError(f"center preview did not start: {captured.decode(errors='replace')}")

        def send(*arguments):
            subprocess.run([args.xdotool, *map(str, arguments)], env=environment, check=True)

        def move(point):
            send("mousemove", "--window", window, *point)
            time.sleep(0.15)

        def save_changed(previous):
            send("key", "--delay", "80", "ctrl+s")
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline and scene_file.read_text() == previous:
                time.sleep(0.05)
            result = scene_file.read_text()
            if result == previous:
                raise RuntimeError("center gesture did not change the saved scene")
            return result

        def undo_to(previous):
            undo_and_save(args.xdotool, environment, scene_file, previous,
                          "center gesture did not undo in one step")

        if args.authored_meshes and b"native mesh geometry: meshes=2 vertices=31 indices=45" not in captured:
            raise RuntimeError(f"distinct OBJ geometry was not submitted: {captured!r}")
        send("windowfocus", window)
        viewport = settled_viewport(editor.stderr, viewport)
        # Focus the Scene panel and clear selection by clicking empty canvas, not either root.
        move((viewport[0] + 10, viewport[1] + 10))
        send("click", 1)
        time.sleep(0.2)
        # No selection yet: Home fits both roots, then restores the same view after a pan.
        move((viewport[0] + viewport[2] // 2, viewport[1] + viewport[3] // 2))
        send("key", "--delay", "100", "Home")
        time.sleep(0.2)
        all_pixels = scene_region_pixels(display, window, viewport)
        send("mousedown", 2)
        move((viewport[0] + viewport[2] // 2 + 40, viewport[1] + viewport[3] // 2 + 20))
        time.sleep(0.2)
        send("mouseup", 2)
        settled_scene_preview(display, window, viewport, all_pixels)
        send("key", "--delay", "100", "Home")
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if scene_region_pixels(display, window, viewport) == all_pixels:
                break
            time.sleep(0.05)
        else:
            raise RuntimeError("Home did not restore all-scene framing without selection")
        point = blue_proxy_pixel(display, window, viewport, first=True)
        if point is None:
            raise RuntimeError("first root proxy is not visible")
        move(point)
        send("click", 1)
        time.sleep(0.2)
        viewport = settled_viewport(editor.stderr, viewport)
        point = blue_proxy_pixel(display, window, viewport)
        if point is None:
            raise RuntimeError("second root proxy is not visible")
        move(point)
        send("keydown", "Control_L")
        send("click", 1)
        send("keyup", "Control_L")
        time.sleep(0.2)
        send("key", "p", "r", "f")
        time.sleep(0.2)
        viewport = settled_viewport(editor.stderr, viewport)
        send("key", "--delay", "80", "ctrl+s")
        time.sleep(0.15)
        baseline = scene_file.read_text()
        point = uniform_handle_pixel(display, window, viewport)
        if point is None:
            raise RuntimeError("center uniform scale handle is not visible")
        move(point)
        send("keydown", "Shift_L")
        send("mousedown", 1)
        time.sleep(0.1)
        before_preview = scene_region_pixels(display, window, viewport)
        move((point[0], point[1] - 40))
        preview = settled_scene_preview(display, window, viewport, before_preview)
        if scene_file.read_text() != baseline:
            raise RuntimeError("center scale committed before release")
        send("mouseup", 1)
        send("keyup", "Shift_L")
        scaled = root_poses(save_changed(baseline))
        for entity, expected_x in ((10, -3), (20, 3)):
            pose = scaled[entity]
            if (abs(pose[0] - expected_x) > 1e-6 or abs(pose[1] - 1) > 1e-6 or
                    abs(pose[2]) > 1e-6 or any(abs(value - 1.5) > 1e-6 for value in pose[7:10])):
                raise RuntimeError(f"center scale failed to move and scale both roots: {scaled}")
        deadline = time.monotonic() + 3
        release_matches = False
        while time.monotonic() < deadline and not release_matches:
            released = scene_region_pixels(display, window, viewport)
            release_matches = sum(abs(a - b) for a, b in zip(preview, released)) < 100
            if not release_matches:
                time.sleep(0.05)
        if not release_matches:
            raise RuntimeError("center scale geometry jumped on release")
        undo_to(baseline)
        move((viewport[0] + viewport[2] // 2, viewport[1] + viewport[3] // 2))
        send("key", "e")
        time.sleep(0.2)
        point = axis_handle_pixels(display, window, viewport)[1]
        if point is None:
            raise RuntimeError("center Y rotation ring is not visible")
        move(point)
        send("mousedown", 1)
        time.sleep(0.1)
        move((point[0] + 28, point[1] + 20))
        send("mouseup", 1)
        rotated = root_poses(save_changed(baseline))
        a, b = rotated[10], rotated[20]
        if (abs(a[2]) < 0.1 or abs(a[0] + b[0]) > 1e-6 or abs(a[2] + b[2]) > 1e-6 or
                abs(a[1] - 1) > 1e-6 or abs(b[1] - 1) > 1e-6 or
                abs(math.hypot(a[0] - b[0], a[2] - b[2]) - 4) > 1e-6):
            raise RuntimeError(f"center rotation did not retain midpoint and separation: {rotated}")
        undo_to(baseline)
        request_window_close(str(window), environment)
        _, stderr = editor.communicate(timeout=15)
        if editor.returncode != 0 or b"scene_draws=" not in stderr:
            raise RuntimeError(f"center acceptance exit failed: {stderr.decode(errors='replace')}")
        editor = None
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
