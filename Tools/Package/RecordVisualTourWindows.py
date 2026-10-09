"""Record a native Windows visual tour; requires Pillow, NumPy and OpenCV."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def record(executable, output, backend, quality):
    import cv2
    import numpy as np
    from PIL import ImageGrab

    user32 = ctypes.WinDLL('user32', use_last_error=True)
    user32.SetProcessDPIAware()
    user32.GetWindowLongW.argtypes = [wintypes.HWND, ctypes.c_int]
    user32.SetWindowPos.argtypes = [wintypes.HWND, wintypes.HWND, ctypes.c_int,
                                  ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.UINT]
    user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user32.ClientToScreen.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.POINT)]
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.IsWindowVisible.argtypes = [wintypes.HWND]
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    report = output / 'tour.json'
    report.unlink(missing_ok=True)
    command = [str(executable), '--tour=visual', f'--backend={backend}', '--clean-view',
               '--vsync=off', '--no-reload', '--gameplay-module=static',
               f'--quality={quality}', f'--report={report}']
    binary_hash = sha256(executable)
    video_path = output / 'visual-tour.mp4'
    writer = None
    process = None
    callback = None
    with (output / 'stdout.log').open('w') as stdout, (output / 'stderr.log').open('w') as stderr:
        try:
            launched = time.monotonic()
            process = subprocess.Popen(command, cwd=executable.parent, stdout=stdout, stderr=stderr,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            selected = []
            def visit(window, unused):
                pid = wintypes.DWORD()
                user32.GetWindowThreadProcessId(window, ctypes.byref(pid))
                if pid.value == process.pid and user32.IsWindowVisible(window):
                    selected.append(window)
                    return False
                return True
            callback = callback_type(visit)
            while not selected:
                require(process.poll() is None, 'Native app exited before publishing its window')
                require(time.monotonic() - launched < 15, 'Native window startup timed out')
                user32.EnumWindows(callback, 0)
                time.sleep(0.02)
            window = selected[0]
            outer = wintypes.RECT(0, 0, 1280, 720)
            style = user32.GetWindowLongW(window, -16)
            require(user32.AdjustWindowRect(ctypes.byref(outer), style, False), 'Window bounds unavailable')
            require(user32.SetWindowPos(window, wintypes.HWND(-1), 0, 0,
                                       outer.right - outer.left, outer.bottom - outer.top, 0x40),
                    'Could not expose the native app')
            rectangle = wintypes.RECT()
            point = wintypes.POINT()
            require(user32.GetClientRect(window, ctypes.byref(rectangle)), 'Client bounds unavailable')
            require(user32.ClientToScreen(window, ctypes.byref(point)), 'Screen origin unavailable')
            require((rectangle.right, rectangle.bottom) == (1280, 720), 'Expected a 1280x720 client')
            bounds = (point.x, point.y, point.x + 1280, point.y + 720)
            require(bounds[2] <= user32.GetSystemMetrics(0) and bounds[3] <= user32.GetSystemMetrics(1),
                    'Client does not fit on the primary desktop')
            fps = 10
            writer = cv2.VideoWriter(str(video_path), cv2.VideoWriter_fourcc(*'mp4v'), fps, (1280, 720))
            require(writer.isOpened(), 'MP4 encoder unavailable')
            started = time.monotonic()
            frames = captures = 0
            shots = [5, 30, 55, 80, 95]
            while process.poll() is None:
                elapsed = time.monotonic() - started
                require(elapsed < 150, 'Native visual tour timed out')
                if elapsed < frames / fps:
                    time.sleep(min(0.01, frames / fps - elapsed))
                    continue
                image = ImageGrab.grab(bbox=bounds)
                frame = cv2.cvtColor(np.array(image), cv2.COLOR_RGB2BGR)
                captures += 1
                while frames / fps <= elapsed:
                    writer.write(frame)
                    frames += 1
                if shots and elapsed >= shots[0]:
                    image.save(output / f'shot-{shots.pop(0)}.png')
            require(process.wait() == 0, 'Native application failed; inspect stderr.log')
            wall = time.monotonic() - launched
            writer.release()
            writer = None
            data = json.loads(report.read_text(encoding='utf-8'))
            native = data['windowed_evidence']
            tour = data['runtime_rooms']['tour']
            require(data['status'] == 'PASS' and tour['kind'] == 'visual' and
                    tour['seconds'] == 100 and tour['paused'], 'Incomplete native visual tour')
            require(native['executed'] and native['backend'] == backend and
                    not native['backend_fallback'] and not native['software_rasterizer'],
                    'Expected the requested hardware backend')
            require(native['overlay_frames'] == 0 and native['native_scene_draws'] > 100,
                    'Expected clean native scene frames')
            require(native['surface_acquires'] == native['surface_presents'] +
                    native['surface_recoverable_presents'], 'Native acquire/present imbalance')
            require(90 <= frames / fps <= 120 and not shots, 'Incomplete recorded tour or fixed shots')
            require(sha256(executable) == binary_hash, 'Executable changed during capture')
            video = cv2.VideoCapture(str(video_path))
            require(video.isOpened(), 'Recorded MP4 cannot be decoded')
            decoded = 0
            while True:
                ok, frame = video.read()
                if not ok:
                    break
                require(frame.shape[:2] == (720, 1280), 'Unexpected recorded frame size')
                decoded += 1
            video.release()
            require(decoded == frames, 'Recorded video lost frames')
            result = dict(scope='Actual Windows native application capture; visual/budget approval is separate',
                          command=command, backend=backend, quality=quality, build=data['build'],
                          wall_seconds=wall, tour_seconds=100, fps=fps, frames=frames,
                          capture_samples=captures, duplicated_frames=frames-captures,
                          decoded_frames=decoded, duration_seconds=frames/fps,
                          device_identity=native.get('device_identity'), software_rasterizer=False,
                          executable_sha256=binary_hash, video_sha256=sha256(video_path),
                          report_sha256=sha256(report), overlay_frames=0)
            (output / 'recording.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
            print(json.dumps(result, indent=2))
        finally:
            if writer is not None:
                writer.release()
            if process is not None and process.poll() is None:
                process.kill()
                process.wait(timeout=10)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--backend', choices=('dx12', 'vulkan'), required=True)
    parser.add_argument('--quality', choices=('basic', 'standard', 'high'), default='standard')
    args = parser.parse_args()
    if sys.platform != 'win32':
        parser.error('Windows is required')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    (output / 'recording.json').unlink(missing_ok=True)
    try:
        record(args.executable.resolve(), output, args.backend, args.quality)
    except (RuntimeError, OSError, ValueError, KeyError, ImportError) as error:
        print(f'FAIL: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
