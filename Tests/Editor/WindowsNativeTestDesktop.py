"""Owned foreground-launcher precondition; never a stand-in for an Editor window."""
from contextlib import contextmanager
import ctypes as C
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import time


@contextmanager
def input_desktop(evidence):
    """Select the existing input desktop for this thread, without switching user desktops."""
    user = C.WinDLL('user32', use_last_error=True)
    kernel = C.WinDLL('kernel32')
    user.GetThreadDesktop.argtypes = [W.DWORD]
    user.GetThreadDesktop.restype = W.HANDLE
    user.OpenInputDesktop.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    user.OpenInputDesktop.restype = W.HANDLE
    user.SetThreadDesktop.argtypes = [W.HANDLE]
    user.CloseDesktop.argtypes = [W.HANDLE]
    user.GetUserObjectInformationW.argtypes = [W.HANDLE, C.c_int, C.c_void_p, W.DWORD,
                                              C.POINTER(W.DWORD)]

    def name(handle):
        value = C.create_unicode_buffer(256)
        needed = W.DWORD()
        if not handle or not user.GetUserObjectInformationW(
                handle, 2, value, C.sizeof(value), C.byref(needed)):
            raise RuntimeError('Cannot identify the native test desktop')
        return value.value

    previous = user.GetThreadDesktop(kernel.GetCurrentThreadId())
    active = user.OpenInputDesktop(0, False, 0x1 | 0x2 | 0x8 | 0x40 | 0x80)
    if not active:
        raise RuntimeError('Interactive native input desktop is unavailable')
    selected = False
    try:
        same = name(previous) == name(active)
        if not same:
            if os.environ.get('NEXORA_NATIVE_TEST_INPUT_DESKTOP') != '1':
                raise RuntimeError('Native test is on an isolated desktop; explicitly opt in '
                                   'with NEXORA_NATIVE_TEST_INPUT_DESKTOP=1 for visible UI testing')
            if not user.SetThreadDesktop(active):
                raise RuntimeError('Cannot select the existing input desktop for the native test')
            selected = True
        Path(evidence).mkdir(parents=True, exist_ok=True)
        Path(evidence, 'desktop.json').write_text(json.dumps({
            'original_thread_matches_input': same,
            'selected_existing_input_desktop': selected,
            'user_input_desktop_switched': False,
            'thread_matches_input': name(user.GetThreadDesktop(kernel.GetCurrentThreadId())) == name(active),
        }, indent=2)+'\n')
        yield
    finally:
        restored = not selected or bool(user.SetThreadDesktop(previous))
        user.CloseDesktop(active)
        if not restored:
            raise RuntimeError('Native test could not restore its original thread desktop')


@contextmanager
def foreground_launcher(evidence=None, *, native_click=False):
    user = C.WinDLL('user32', use_last_error=True)
    user.CreateWindowExW.argtypes = [W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD,
                                    C.c_int, C.c_int, C.c_int, C.c_int,
                                    W.HWND, W.HMENU, W.HINSTANCE, C.c_void_p]
    user.CreateWindowExW.restype = W.HWND
    user.ShowWindow.argtypes = [W.HWND, C.c_int]
    user.SetForegroundWindow.argtypes = [W.HWND]
    user.GetForegroundWindow.restype = W.HWND
    user.DestroyWindow.argtypes = [W.HWND]
    user.PeekMessageW.argtypes = [C.POINTER(W.MSG), W.HWND, W.UINT, W.UINT, W.UINT]
    user.GetWindowRect.argtypes = [W.HWND, C.POINTER(W.RECT)]
    user.SetWindowPos.argtypes = [W.HWND, W.HWND, C.c_int, C.c_int, C.c_int, C.c_int, W.UINT]
    user.WindowFromPoint.argtypes = [W.POINT]
    user.WindowFromPoint.restype = W.HWND
    user.GetCursorPos.argtypes = [C.POINTER(W.POINT)]
    user.SetCursorPos.argtypes = [C.c_int, C.c_int]
    user.GetAsyncKeyState.argtypes = [C.c_int]
    user.SetThreadDpiAwarenessContext.argtypes = [W.HANDLE]
    user.SetThreadDpiAwarenessContext.restype = W.HANDLE
    user.SystemParametersInfoW.argtypes = [W.UINT, W.UINT, C.c_void_p, W.UINT]

    def wait_owned():
        deadline = time.monotonic() + 3
        message = W.MSG()
        while time.monotonic() < deadline:
            while user.PeekMessageW(C.byref(message), None, 0, 0, 1):
                user.TranslateMessage(C.byref(message))
                user.DispatchMessageW(C.byref(message))
            if user.GetForegroundWindow() == window:
                return True
            time.sleep(.01)
        return False

    def click_owned():
        # A real launcher click supplies the ordinary input-driven launch context when a
        # background script cannot activate itself. Use an interior point of a real Button,
        # not a one-pixel corner that a themed control or desktop edge can exclude.
        class Mouse(C.Structure):
            _fields_ = [('dx', W.LONG), ('dy', W.LONG), ('data', W.DWORD),
                        ('flags', W.DWORD), ('time', W.DWORD), ('extra', C.c_size_t)]

        class Payload(C.Union):
            _fields_ = [('mouse', Mouse)]

        class Input(C.Structure):
            _fields_ = [('kind', W.DWORD), ('payload', Payload)]

        class GuiThread(C.Structure):
            _fields_ = [('size', W.DWORD), ('flags', W.DWORD),
                        ('active', W.HWND), ('focus', W.HWND), ('capture', W.HWND),
                        ('menu', W.HWND), ('moving', W.HWND), ('caret', W.HWND),
                        ('caret_rect', W.RECT)]

        user.SendInput.argtypes = [W.UINT, C.POINTER(Input), C.c_int]
        user.SendInput.restype = W.UINT
        user.GetGUIThreadInfo.argtypes = [W.DWORD, C.POINTER(GuiThread)]
        bounds, cursor = W.RECT(), W.POINT()
        if (not user.SetWindowPos(window, None, 0, 0, 0, 0, 0x13) or
                not user.GetWindowRect(window, C.byref(bounds)) or
                not user.GetCursorPos(C.byref(cursor))):
            raise RuntimeError('Cannot expose the owned launcher for native input')
        point = W.POINT((bounds.left + bounds.right) // 2, (bounds.top + bounds.bottom) // 2)
        width, height = user.GetSystemMetrics(0), user.GetSystemMetrics(1)
        info = GuiThread()
        info.size = C.sizeof(info)
        available = bool(user.GetGUIThreadInfo(0, C.byref(info)))
        record['click_guard'] = {'gui_info': available, 'capture': bool(info.capture),
                                 'flags': info.flags, 'point': [point.x, point.y],
                                 'hit_owned': user.WindowFromPoint(point) == window,
                                 'buttons_held': any(user.GetAsyncKeyState(button) & 0x8000
                                                     for button in (1, 2, 4, 5, 6))}
        if (not available or info.capture or info.flags & 0x1e or
                not record['click_guard']['hit_owned'] or
                not (0 <= point.x < width and 0 <= point.y < height) or
                record['click_guard']['buttons_held']):
            raise RuntimeError('Owned launcher input is occluded or a mouse button is held')
        # Acknowledge the actual up event on this owned control before restoring the cursor.
        # This also prevents a queued click from following a premature cursor restoration.
        procedure_type = C.WINFUNCTYPE(C.c_ssize_t, W.HWND, W.UINT, W.WPARAM, W.LPARAM)
        user.SetWindowLongPtrW.argtypes = [W.HWND, C.c_int, C.c_ssize_t]
        user.SetWindowLongPtrW.restype = C.c_ssize_t
        user.CallWindowProcW.argtypes = [C.c_ssize_t, W.HWND, W.UINT, W.WPARAM, W.LPARAM]
        user.CallWindowProcW.restype = C.c_ssize_t
        mouse_up = False
        previous = 0

        @procedure_type
        def acknowledge(hwnd, message, wp, lp):
            nonlocal mouse_up
            result = user.CallWindowProcW(previous, hwnd, message, wp, lp)
            if message == 0x202:
                mouse_up = True
            return result

        previous = user.SetWindowLongPtrW(window, -4, C.cast(acknowledge, C.c_void_p).value)
        if not previous:
            raise RuntimeError('Cannot observe the owned launcher native click')
        try:
            if not user.SetCursorPos(point.x, point.y):
                raise RuntimeError('Cannot position native input over the owned launcher')
            actual = W.POINT()
            if (not user.GetCursorPos(C.byref(actual)) or
                    (actual.x, actual.y) != (point.x, point.y) or
                    user.WindowFromPoint(actual) != window):
                raise RuntimeError('Refusing native input outside the owned launcher')
            events = (Input * 2)()
            # Each event carries the verified physical point on the primary monitor.
            # Wait for the owned up callback before restoring the original cursor.
            for event in events:
                event.payload.mouse.dx = min(65535, (point.x * 65536 + 32768) // width)
                event.payload.mouse.dy = min(65535, (point.y * 65536 + 32768) // height)
            events[0].payload.mouse.flags = 0x8003
            events[1].payload.mouse.flags = 0x8005
            inserted = user.SendInput(2, events, C.sizeof(Input))
            if inserted != 2:
                # A partial insertion must not leave this test's synthetic button held.
                if inserted == 1:
                    user.SendInput(1, C.byref(events[1]), C.sizeof(Input))
                raise RuntimeError('Owned launcher native click was not delivered completely')
            deadline = time.monotonic() + 3
            message = W.MSG()
            while not mouse_up and time.monotonic() < deadline:
                while user.PeekMessageW(C.byref(message), None, 0, 0, 1):
                    user.TranslateMessage(C.byref(message))
                    user.DispatchMessageW(C.byref(message))
                if not mouse_up:
                    time.sleep(.01)
            if not mouse_up:
                raise RuntimeError('Owned launcher did not acknowledge its native mouse release')
        finally:
            user.SetWindowLongPtrW(window, -4, previous)
            actual = W.POINT()
            if user.GetCursorPos(C.byref(actual)) and (actual.x, actual.y) == (point.x, point.y):
                user.SetCursorPos(cursor.x, cursor.y)
    # A small tool window owns only the test launcher, with no taskbar entry. It gives
    # the child the ordinary foreground-launch context without manipulating foreign apps,
    # changing their topmost state, or activating the product's non-activating splash.
    previous_dpi = user.SetThreadDpiAwarenessContext(C.c_void_p(-4))
    if not previous_dpi:
        raise RuntimeError('Cannot select physical coordinates for the native launcher')
    work = W.RECT()
    if not user.SystemParametersInfoW(0x30, 0, C.byref(work), 0):
        user.SetThreadDpiAwarenessContext(previous_dpi)
        raise RuntimeError('Cannot locate the primary work area for the owned launcher')
    window = user.CreateWindowExW(0x80, 'BUTTON', 'Nexora native test launcher', 0x80000000,
                                  work.left + 8, work.top + 8, 32, 32, None, None, None, None)
    record = {'owned_launcher_class': 'BUTTON', 'native_click_attempted': False,
              'native_click': False, 'foreground_owned': False}
    try:
        if not window:
            raise RuntimeError('Cannot create the owned native test launcher')
        user.ShowWindow(window, 5)
        record['set_foreground_returned'] = bool(user.SetForegroundWindow(window))
        if native_click or not wait_owned():
            record['native_click_attempted'] = True
            click_owned()
            record['native_click'] = True
        if not wait_owned():
            raise RuntimeError('Cannot establish the owned foreground-launch precondition')
        record['foreground_owned'] = True
        yield window
    finally:
        if window:
            user.DestroyWindow(window)
        user.SetThreadDpiAwarenessContext(previous_dpi)
        if evidence:
            Path(evidence).write_text(json.dumps(record, indent=2)+'\n')


if __name__ == '__main__':
    import sys
    root = Path(sys.argv[1]).resolve()
    root.mkdir(parents=True, exist_ok=False)
    windows = []
    user = C.WinDLL('user32')
    user.IsWindow.argtypes = [W.HWND]
    with input_desktop(root):
        for index in range(3):
            with foreground_launcher(root / f'launcher-{index}.json', native_click=True) as window:
                windows.append(window)
                if not user.IsWindow(window):
                    raise RuntimeError('Owned native launcher vanished during acceptance')
    if any(user.IsWindow(window) for window in windows):
        raise RuntimeError('Native launcher acceptance left an owned window alive')
    (root / 'result.json').write_text(json.dumps({
        'status': 'PASS', 'launches': 3, 'actual_owned_mouse_click': True,
        'mouse_release_acknowledged': True, 'remaining_owned_windows': 0,
        'scope': 'Native test launcher only; no Editor acceptance',
    }, indent=2)+'\n')
    print('Three owned native launcher clicks, foreground admission and window teardown passed')
