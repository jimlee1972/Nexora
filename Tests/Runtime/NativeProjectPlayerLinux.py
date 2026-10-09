"""Real standalone StaticView package pixels, resize, close and bounded presentation."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import re
import select
import subprocess
import sys
import tempfile
import time

# Reuse only generic Xvfb/WM_DELETE/XImage helpers; no Editor executable, project or SDK is used.
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "EditorImGui"))
from LinuxDisplayAcceptance import start_xvfb, request_window_close
from LinuxNativeScenePreview import XImage, channel


def require(value, message):
    if not value:
        raise RuntimeError(message)


def pixels(display_name, window, width, height):
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
    require(display, "native pixel reader could not connect")
    try:
        image = x11.XGetImage(display, window, 0, 0, width, height,
                              ctypes.c_ulong(-1).value, 2)
        require(image, "native pixel readback failed")
        try:
            colors = []
            for y in range(8, height - 8, 8):
                for x in range(8, width - 8, 8):
                    value = x11.XGetPixel(image, x, y)
                    colors.append(tuple(channel(value, mask) for mask in
                                        (image.contents.red_mask, image.contents.green_mask,
                                         image.contents.blue_mask)))
            return colors
        finally:
            x11.XDestroyImage(image)
    finally:
        x11.XCloseDisplay(display)


def main():
    parser = argparse.ArgumentParser()
    for name in ("fixture", "player", "xvfb", "xdotool"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="nexora-native-player-") as temporary:
        root = Path(temporary).resolve()
        package = root / "專案 µ package with spaces.nxproject"
        written = subprocess.run([args.fixture, "--write-fixture", str(package)],
                                 capture_output=True, text=True, timeout=15)
        require(written.returncode == 0, written.stderr)
        baseline = package.read_bytes()
        server, display = start_xvfb(args.xvfb, "1280x720x24")
        player = None
        try:
            require(display, "Xvfb did not become ready")
            environment = dict(os.environ, DISPLAY=display)
            player = subprocess.Popen([args.player, "--run-package", str(package), "--backend=vulkan"],
                                      env=environment, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            captured = b""
            deadline = time.monotonic() + 15
            window = None
            while time.monotonic() < deadline:
                searched = subprocess.run([args.xdotool, "search", "--name", "^Nexora Project Player$"],
                                          env=environment, capture_output=True, text=True, timeout=3)
                if searched.returncode == 0 and searched.stdout.strip():
                    window = int(searched.stdout.splitlines()[0])
                    break
                require(player.poll() is None, "native player exited before its window")
                time.sleep(0.05)
            require(window, "native player window missing")

            def wait_frame(width, height, resized=False):
                nonlocal captured
                deadline = time.monotonic() + 10
                while time.monotonic() < deadline:
                    if select.select([player.stderr], [], [], 0.1)[0]:
                        captured += os.read(player.stderr.fileno(), 4096)
                    records = re.findall(rb"native project frame: (\d+) (\d+) (\d+) (\d+) (\d+)",
                                         captured)
                    if any(int(w) == width and int(h) == height and int(draw) > 0 and
                           int(present) > 0 and (not resized or int(generation) > 0)
                           for w, h, draw, present, generation in records):
                        return
                    require(player.poll() is None, "native player failed: " + captured.decode(errors="replace"))
                raise RuntimeError("native frame observation missing: " + captured.decode(errors="replace"))

            def require_material_pixels(width, height):
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    colors = pixels(display, window, width, height)
                    red = sum(r > g * 2 + 20 and r > b * 2 + 20 for r, g, b in colors)
                    green = sum(g > r * 2 + 20 and g > b * 2 + 20 for r, g, b in colors)
                    if red >= 20 and green >= 20:
                        return
                    time.sleep(0.05)
                raise RuntimeError(f"actual scalar materials absent from pixels: red={red}, green={green}")

            wait_frame(960, 640)
            require_material_pixels(960, 640)
            subprocess.run([args.xdotool, "windowsize", str(window), "800", "600"],
                           env=environment, check=True, timeout=3)
            wait_frame(800, 600, resized=True)
            require_material_pixels(800, 600)
            request_window_close(str(window), environment)
            output, errors = player.communicate(timeout=10)
            require(player.returncode == 0, captured.decode(errors="replace") + errors.decode(errors="replace"))
            report = json.loads(output)
            require(report["status"] == "RENDERED_STATIC_VIEW" and report["drained"] and
                    report["native_rendering"] and not report["gameplay_loaded"] and
                    report["render_instances"] == 2 and report["inactive_components"] == 1 and
                    report["resize_generations"] > 0 and report["presented_frames"] > 0 and
                    report["scene_draw_calls"] > 0, "actual native close/drain report invalid")
            bounded = subprocess.run([args.player, "--run-package", str(package), "--backend=vulkan",
                                      "--frames=4"], env=environment, capture_output=True, text=True, timeout=15)
            require(bounded.returncode == 0, bounded.stderr)
            bounded_report = json.loads(bounded.stdout)
            require(bounded_report["presented_frames"] == 4 and bounded_report["drained"],
                    "bounded native run did not present exactly four frames and drain")
            for arguments in (["--frames=0"], ["--frames=1000001"], ["--frames=-1"],
                              ["--frames=2", "--frames=3"], ["--backend=unknown"]):
                invalid = subprocess.run([args.player, "--run-package", str(package), *arguments],
                                         env=environment, capture_output=True, text=True, timeout=3)
                require(invalid.returncode == 2 and not invalid.stdout, "invalid native arguments admitted")
            damaged = root / "corrupt.nxproject"
            damaged.write_bytes(baseline[:50])
            invalid = subprocess.run([args.player, "--run-package", str(damaged)],
                                     env=environment, capture_output=True, text=True, timeout=3)
            require(invalid.returncode == 1 and not invalid.stdout, "invalid native package reported success")
            require(package.read_bytes() == baseline, "native viewing changed cooked project bytes")
            print("Standalone cooked StaticView produced real red/green pixels, resized, closed and drained")
        finally:
            if player is not None and player.poll() is None:
                player.terminate()
                try:
                    player.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    player.kill()
                    player.wait(timeout=5)
            server.terminate()
            server.wait(timeout=5)


if __name__ == "__main__":
    main()
