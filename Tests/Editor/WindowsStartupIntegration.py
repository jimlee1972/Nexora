"""Bounded, real no-argument GUI startup/input/shutdown; never use a helper HWND as Editor."""
import ctypes as C
from ctypes import wintypes as W
import json
import os
import re
import shutil
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time


def verify_startup(editor, evidence, environment=None):
    evidence = Path(evidence).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    user = C.WinDLL('user32', use_last_error=True)
    gdi = C.WinDLL('gdi32', use_last_error=True)
    user.SetThreadDpiAwarenessContext.argtypes = [W.HANDLE]
    user.SetThreadDpiAwarenessContext.restype = W.HANDLE
    user.SetThreadDpiAwarenessContext(C.c_void_p(-4))
    user.GetWindowLongPtrW.restype = C.c_ssize_t
    user.GetWindowLongPtrW.argtypes = [W.HWND, C.c_int]
    user.GetWindow.restype = W.HWND
    user.GetWindow.argtypes = [W.HWND, W.UINT]
    user.GetForegroundWindow.restype = W.HWND
    user.SetForegroundWindow.argtypes = [W.HWND]
    user.IsWindowVisible.argtypes = [W.HWND]
    user.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
    user.GetWindowTextW.argtypes = [W.HWND, W.LPWSTR, C.c_int]
    user.GetClassNameW.argtypes = [W.HWND, W.LPWSTR, C.c_int]
    user.SendMessageTimeoutW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM,
                                       W.UINT, W.UINT, C.POINTER(C.c_size_t)]
    user.SendMessageTimeoutW.restype = C.c_ssize_t
    user.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
    user.GetClientRect.argtypes = [W.HWND, C.POINTER(W.RECT)]
    user.GetWindowRect.argtypes = [W.HWND, C.POINTER(W.RECT)]
    user.ClientToScreen.argtypes = [W.HWND, C.POINTER(W.POINT)]
    user.GetDC.argtypes = [W.HWND]
    user.GetDC.restype = W.HDC
    user.ReleaseDC.argtypes = [W.HWND, W.HDC]
    gdi.CreateCompatibleDC.argtypes = [W.HDC]
    gdi.CreateCompatibleDC.restype = W.HDC
    gdi.CreateCompatibleBitmap.argtypes = [W.HDC, C.c_int, C.c_int]
    gdi.CreateCompatibleBitmap.restype = W.HBITMAP
    gdi.SelectObject.argtypes = [W.HDC, W.HGDIOBJ]
    gdi.SelectObject.restype = W.HGDIOBJ
    gdi.BitBlt.argtypes = [W.HDC, C.c_int, C.c_int, C.c_int, C.c_int,
                         W.HDC, C.c_int, C.c_int, W.DWORD]
    gdi.GetDIBits.argtypes = [W.HDC, W.HBITMAP, W.UINT, W.UINT, C.c_void_p,
                            C.c_void_p, W.UINT]
    gdi.DeleteObject.argtypes = [W.HGDIOBJ]
    gdi.DeleteDC.argtypes = [W.HDC]

    def require(ok, message):
        if not ok:
            raise RuntimeError(message)

    def text(hwnd, class_name=False):
        buffer = C.create_unicode_buffer(256)
        (user.GetClassNameW if class_name else user.GetWindowTextW)(hwnd, buffer, 256)
        return buffer.value

    def responsive(hwnd):
        result = C.c_size_t()
        return bool(user.SendMessageTimeoutW(hwnd, 0, 0, 0, 2, 500, C.byref(result)))

    def capture(hwnd, path, splash=False):
        rect = W.RECT()
        user.GetClientRect(hwnd, C.byref(rect))
        origin = W.POINT()
        user.ClientToScreen(hwnd, C.byref(origin))
        width, height = rect.right, rect.bottom
        require(width > 0 and height > 0, 'Empty main client')
        dc = user.GetDC(None)
        memory = gdi.CreateCompatibleDC(dc)
        bitmap = gdi.CreateCompatibleBitmap(dc, width, height)
        old = gdi.SelectObject(memory, bitmap)
        require(gdi.BitBlt(memory, 0, 0, width, height, dc, origin.x, origin.y, 0xCC0020),
                'Screen capture failed')
        gdi.SelectObject(memory, old)
        header = struct.pack('<IiiHHIIiiII', 40, width, -height, 1, 32, 0, width*height*4, 0, 0, 0, 0)
        info = C.create_string_buffer(header)
        pixels = C.create_string_buffer(width*height*4)
        require(gdi.GetDIBits(memory, bitmap, 0, height, pixels, info, 0) == height,
                'Screen pixels unavailable')
        gdi.DeleteObject(bitmap)
        gdi.DeleteDC(memory)
        user.ReleaseDC(None, dc)
        data = pixels.raw
        Path(path).write_bytes(struct.pack('<HIHHI', 0x4D42, 54+len(data), 0, 0, 54)+header+data)
        white = sum(min(data[i:i+3]) > 235 for i in range(0, len(data), 4)) / (width*height)
        require(white < 0.8, 'Unprepared white main window was exposed')
        dark = sum(max(data[i:i+3]) < 90 for i in range(0, len(data), 4)) / (width*height)
        require(dark > 0.5, 'Prepared dark content is missing at reveal')
        if splash:
            cyan = sum(data[i] > 200 and data[i+1] > 160 and data[i+2] < 120
                       for i in range(0, len(data), 4))
            require(cyan > 100, 'Visible splash wordmark missing')
        else:
            # A black/uniform client must fail. Require actual light UI text/edges
            # in multiple spatial regions, not just a non-white background.
            regions = []
            for row in range(3):
                for col in range(3):
                    light = shade = 0
                    for y in range(height*row//3, height*(row+1)//3, 2):
                        for x in range(width*col//3, width*(col+1)//3, 2):
                            i = (y*width+x)*4
                            light += min(data[i:i+3]) > 160
                            shade += max(data[i:i+3]) < 90
                    regions.append(light > 20 and shade > 100)
            require(sum(regions) >= 2, 'Non-uniform Project Browser/UI regions missing at reveal')
        return white

    hook_type = C.WINFUNCTYPE(None, W.HANDLE, W.DWORD, W.HWND, W.LONG, W.LONG, W.DWORD, W.DWORD)
    user.SetWinEventHook.argtypes = [W.DWORD, W.DWORD, W.HMODULE, hook_type,
                                    W.DWORD, W.DWORD, W.DWORD]
    user.SetWinEventHook.restype = W.HANDLE
    user.UnhookWinEvent.argtypes = [W.HANDLE]
    env = dict(os.environ if environment is None else environment)
    env['NEXORA_EDITOR_STARTUP_EVIDENCE'] = '1'
    results = []
    with tempfile.TemporaryDirectory(prefix='Nexora no-argument GUI 陰陽 ') as temporary:
        root = Path(temporary).resolve()
        env['LOCALAPPDATA'] = str(root / 'Local AppData')
        project = root / 'Recent project 陰陽'
        created = subprocess.run([str(editor), '--create-project='+str(project),
                                  '--project-name=Recent startup project'], cwd=root, env=env,
                                 capture_output=True, timeout=30)
        (evidence/'create-recent.log').write_bytes(created.stdout+b'\n'+created.stderr)
        require(created.returncode == 0, 'Could not populate the normal recent-project store')
        recent = root/'Local AppData/Nexora/Editor/recent-projects'
        require(recent.is_file() and recent.stat().st_size > 0, 'Default recent store is empty')
        for attempt in range(2):
            log = evidence/f'no-arguments-{attempt}.stderr.log'
            process = None
            observed = []
            errors = []
            splash = None
            main = None
            passed = False

            @hook_type
            def event(hook, kind, hwnd, obj, child, thread, tick):
                nonlocal splash, main
                try:
                    pid = W.DWORD()
                    user.GetWindowThreadProcessId(hwnd, C.byref(pid))
                    if not process or pid.value != process.pid or obj != 0 or child != 0:
                        return
                    name = text(hwnd, True)
                    if name == 'NexoraEditorStartup':
                        splash = hwnd
                        if kind == 0x8002:
                            observed.append('splash-show')
                    elif name == 'Nexora.Window' and text(hwnd) == 'Nexora Editor':
                        main = hwnd
                        if kind == 0x8000:
                            observed.append('main-create-observed')
                        if kind == 0x8002:
                            output = log.read_text(errors='replace')
                            require('editor splash: main-frame-ready' in output and
                                    'editor splash: closed' in output,
                                    'Main shown before prepared frame and splash teardown')
                            clocks = dict((event, int(value)) for value, event in re.findall(
                                r'editor splash-clock: tick=(\d+) event=([^\r\n]+)', output))
                            require('main-window-hidden' in clocks and 'closed' in clocks,
                                    'Missing timestamped in-process hidden/close boundaries')
                            require((tick-clocks['closed']) % (2**32) < 2**31,
                                    'Native SHOW preceded splash close (event-clock order)')
                            capture(hwnd, evidence/f'no-arguments-{attempt}-first-show.bmp')
                            observed.append('main-show')
                except Exception as error:
                    errors.append(str(error))

            hook = user.SetWinEventHook(0x8000, 0x8003, None, event, 0, 0, 2)
            require(hook, 'Cannot observe native window lifecycle')
            try:
                with log.open('wb') as stderr, (evidence/f'no-arguments-{attempt}.stdout.log').open('wb') as stdout:
                    # Deliberately no --frames, --graphical, or --recent-projects override.
                    process = subprocess.Popen([str(editor)], cwd=root, env=env, stdout=stdout, stderr=stderr)
                    deadline = time.monotonic()+35
                    message = W.MSG()
                    probes = 0
                    while time.monotonic() < deadline:
                        while user.PeekMessageW(C.byref(message), None, 0, 0, 1):
                            user.TranslateMessage(C.byref(message))
                            user.DispatchMessageW(C.byref(message))
                        require(not errors, '; '.join(errors))
                        require(process.poll() is None, 'No-argument Editor exited during startup')
                        if splash and user.IsWindowVisible(splash):
                            require(responsive(splash), 'Splash UI stopped responding')
                            require(not user.GetWindow(splash, 4), 'Cross-thread splash owner installed')
                            require(not user.GetWindowLongPtrW(splash, -20) & 8, 'Splash is globally topmost')
                            if probes == 0:
                                capture(splash, evidence/f'no-arguments-{attempt}-splash.bmp', True)
                            probes += 1
                        if 'main-show' in observed:
                            break
                        time.sleep(0.005)
                    require('splash-show' in observed and 'main-show' in observed,
                            'Startup/Ready deadline exceeded (possible teardown deadlock)')
                    require(probes > 0, 'Splash was never externally observed responsive')
                    require(responsive(main), 'Prepared main is unresponsive')
                    require(not splash or not user.IsWindowVisible(splash), 'Splash survived Ready')
                    user.SetForegroundWindow(main)
                    # Actual native mouse input on the title bar, without selecting/opening a project.
                    rect = W.RECT()
                    user.GetWindowRect(main, C.byref(rect))
                    user.SetCursorPos(rect.left+200, rect.top+12)
                    user.mouse_event(2, 0, 0, 0, 0)
                    user.mouse_event(4, 0, 0, 0, 0)
                    until = time.monotonic()+3
                    while user.GetForegroundWindow() != main and time.monotonic() < until:
                        time.sleep(0.01)
                    require(user.GetForegroundWindow() == main and responsive(main),
                            'Prepared main does not respond to foreground/input')
                    white = capture(main, evidence/f'no-arguments-{attempt}-main.bmp')
                    user.PostMessageW(main, 0x10, 0, 0)
                    require(process.wait(timeout=10) == 0, 'Normal WM_CLOSE failed')
                    require(not user.IsWindowVisible(main) and
                            (not splash or not user.IsWindowVisible(splash)), 'Orphan window after exit')
                    output = log.read_text(errors='replace')
                    required = ['opened', 'stage=0', 'stage=1', 'main-window-hidden',
                                'stage=3', 'main-frame-ready', 'closed', 'main-window-shown']
                    positions = [output.index('editor splash: '+token+'\n') for token in required]
                    require(positions == sorted(positions), 'Invalid real EXE startup order')
                    require('console_attached=0' in output, 'Startup attached a console')
                    results.append({'argv': [], 'normal_recent_store': True, 'events': observed,
                                    'splash_responsive_probes': probes, 'main_responsive': True,
                                    'foreground_input': True, 'white_fraction': white,
                                    'normal_close_exit': process.returncode, 'orphan_windows': False})
                    passed = True
            finally:
                user.UnhookWinEvent(hook)
                if not passed:
                    shutil.copytree(root, evidence/f'failed-fixture-{attempt}', dirs_exist_ok=True)
                if process and process.poll() is None:
                    process.kill()
                    process.wait(timeout=10)
                (evidence/f'no-arguments-{attempt}-events.json').write_text(
                    json.dumps({'events': observed, 'errors': errors}, indent=2)+'\n')
    result = {'status': 'PASS', 'launches': results, 'startup_timeout_seconds': 35,
              'message_timeout_ms': 500, 'shutdown_timeout_seconds': 10}
    (evidence/'result.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


if __name__ == '__main__':
    verify_startup(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
    print('Real no-argument Editor hidden startup, responsive Ready/input and bounded close passed')
