"""Owned foreground-launcher precondition; never a stand-in for an Editor window."""
from contextlib import contextmanager
import ctypes as C
from ctypes import wintypes as W


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
