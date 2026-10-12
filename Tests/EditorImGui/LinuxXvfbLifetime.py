#!/usr/bin/env python3
"""Verify actual display lifetime across the last temporary client's disconnect."""
import argparse
import ctypes
import importlib.util
import os
import select
import subprocess
import time


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def load_helper(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, "Cannot load display helper")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.start_xvfb


def reset_server(xvfb, screen):
    """Actual default-reset negative control, with the same displayfd readiness protocol."""
    read_fd, write_fd = os.pipe()
    server = subprocess.Popen(
        [xvfb, "-displayfd", str(write_fd), "-screen", "0", screen, "-nolisten", "tcp"],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, pass_fds=(write_fd,), text=True)
    os.close(write_fd)
    try:
        require(select.select([read_fd], [], [], 10)[0], "Negative-control Xvfb did not start")
        number = os.read(read_fd, 64)
        require(number.endswith(b"\n"), "Negative-control Xvfb failed readiness")
        return server, f":{int(number)}"
    except BaseException:
        server.terminate()
        server.communicate(timeout=3)
        raise
    finally:
        os.close(read_fd)


def inspect_lifetime(start, xvfb, retained):
    x11 = ctypes.CDLL("libX11.so.6")
    pointer = ctypes.c_void_p
    integer = ctypes.c_int
    word = ctypes.c_ulong
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = pointer
    x11.XDefaultRootWindow.argtypes = [pointer]
    x11.XDefaultRootWindow.restype = word
    x11.XInternAtom.argtypes = [pointer, ctypes.c_char_p, integer]
    x11.XInternAtom.restype = word
    x11.XChangeProperty.argtypes = [pointer, word, word, word, integer, integer, pointer, integer]
    x11.XSync.argtypes = [pointer, integer]
    x11.XCloseDisplay.argtypes = [pointer]
    x11.XGetWindowProperty.argtypes = [
        pointer, word, word, ctypes.c_long, ctypes.c_long, integer, word,
        ctypes.POINTER(word), ctypes.POINTER(integer), ctypes.POINTER(word),
        ctypes.POINTER(word), ctypes.POINTER(pointer)]
    x11.XFree.argtypes = [pointer]
    server, name = start(xvfb, "640x480x24")
    display = None
    try:
        require(name is not None and server.poll() is None, "Display server did not become ready")
        display = x11.XOpenDisplay(name.encode())
        require(display, "Initial native X11 connection failed")
        atom = x11.XInternAtom(display, b"_NEXORA_DISPLAY_LIFETIME", 0)
        marker = word(0x123456)
        x11.XChangeProperty(display, x11.XDefaultRootWindow(display), atom, 6, 32, 0,
                            ctypes.byref(marker), 1)
        x11.XSync(display, 0)
        x11.XCloseDisplay(display)
        display = None
        # This is an intentional interval with zero clients, not a startup readiness sleep/retry.
        time.sleep(0.1)
        display = x11.XOpenDisplay(name.encode())
        require(display, "Connection after last-client disconnect failed")
        atom = x11.XInternAtom(display, b"_NEXORA_DISPLAY_LIFETIME", 0)
        actual, count, remaining = word(), word(), word()
        format_bits, data = integer(), pointer()
        result = x11.XGetWindowProperty(
            display, x11.XDefaultRootWindow(display), atom, 0, 1, 0, 6,
            ctypes.byref(actual), ctypes.byref(format_bits), ctypes.byref(count),
            ctypes.byref(remaining), ctypes.byref(data))
        try:
            require(result == 0 and server.poll() is None, "Native property observation failed")
            if retained:
                require(actual.value == 6 and format_bits.value == 32 and count.value == 1 and
                        remaining.value == 0 and data and
                        word.from_address(data.value).value == marker.value,
                        "Temporary-client disconnect reset the owned display")
            else:
                require(actual.value == 0 and count.value == 0,
                        "Default-reset negative control did not observe display reset")
        finally:
            if data:
                x11.XFree(data)
    finally:
        if display:
            x11.XCloseDisplay(display)
        if server.poll() is None:
            server.terminate()
        server.communicate(timeout=3)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--xvfb", required=True)
    parser.add_argument("--editor-helper", required=True)
    parser.add_argument("--showcase-helper", required=True)
    args = parser.parse_args()
    inspect_lifetime(reset_server, args.xvfb, False)
    inspect_lifetime(load_helper(args.editor_helper, "editor_display"), args.xvfb, True)
    inspect_lifetime(load_helper(args.showcase_helper, "showcase_display"), args.xvfb, True)
    print("Xvfb default-reset negative control and both retained-display helpers passed")


if __name__ == "__main__":
    main()
