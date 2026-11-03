"""Owned foreground-launcher precondition; never a stand-in for an Editor window."""
from contextlib import contextmanager
import ctypes as C
from ctypes import wintypes as W
import json
import os
from pathlib import Path


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
def foreground_launcher():
    user = C.WinDLL('user32', use_last_error=True)
    user.CreateWindowExW.argtypes = [W.DWORD, W.LPCWSTR, W.LPCWSTR, W.DWORD,
                                    C.c_int, C.c_int, C.c_int, C.c_int,
                                    W.HWND, W.HMENU, W.HINSTANCE, C.c_void_p]
    user.CreateWindowExW.restype = W.HWND
    user.ShowWindow.argtypes = [W.HWND, C.c_int]
    user.SetForegroundWindow.argtypes = [W.HWND]
    user.GetForegroundWindow.restype = W.HWND
    user.DestroyWindow.argtypes = [W.HWND]
    # A one-pixel tool window owns only the test launcher, with no taskbar entry. It gives
    # the child the ordinary foreground-launch context without manipulating foreign apps,
    # changing their topmost state, or activating the product's non-activating splash.
    window = user.CreateWindowExW(0x80, 'STATIC', 'Nexora native test launcher', 0x80000000,
                                  0, 0, 1, 1, None, None, None, None)
    try:
        if not window:
            raise RuntimeError('Cannot create the owned native test launcher')
        user.ShowWindow(window, 5)
        user.SetForegroundWindow(window)
        if user.GetForegroundWindow() != window:
            raise RuntimeError('Cannot establish the owned foreground-launch precondition')
        yield window
    finally:
        if window:
            user.DestroyWindow(window)
