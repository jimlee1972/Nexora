#!/usr/bin/env python3
"""Exercise the actual native build console against a real bounded child program."""
import argparse
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time
from LinuxDisplayAcceptance import collect_output, launch, request_window_close, start_xvfb, wait_for_window


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    for name in ("editor", "fixture", "xvfb", "xdotool"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix="nexora-native-build-console-"))
    root = scratch / "project"
    (root / "Content").mkdir(parents=True)
    (root / "project.nexora").write_text("schema=1\nname=Native Build Console\n")
    # Create a persisted camera scene through the ordinary Editor save action below.
    xvfb, display = start_xvfb(args.xvfb, "1280x900x24")
    process = None
    window = None
    try:
        require(display is not None, "Xvfb did not become ready")
        env = os.environ.copy()
        env["DISPLAY"] = display
        env["XDG_STATE_HOME"] = str(scratch / "state")
        env["MESA_SHADER_CACHE_DIR"] = str(scratch / "shader-cache")
        captured = bytearray()
        process = launch(args.editor, root, scratch / "recent", env)
        window = wait_for_window(args.xdotool, env)
        subprocess.run([args.xdotool, "windowfocus", "--sync", window], env=env, check=True)

        def tool(*arguments):
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(.15)

        def send(*keys):
            tool("key", "--clearmodifiers", "--delay", "60", *keys)

        def click(x, y):
            tool("mousemove", "--window", window, str(x), str(y), "click", "1")

        def wait_log(pattern, start=0):
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline:
                require(process.poll() is None, "Editor exited during build controls")
                if select.select([process.stderr], [], [], .1)[0]:
                    captured.extend(os.read(process.stderr.fileno(), 65536))
                if re.search(pattern, captured[start:].decode("utf-8", errors="replace")):
                    return
            raise RuntimeError(f"Missing build diagnostic {pattern}: {captured!r}")

        time.sleep(.6)
        click(500, 220)
        send("ctrl+s")
        deadline = time.monotonic() + 10
        while not (root / ".nexora/scenes/Main.scene").exists() and time.monotonic() < deadline:
            time.sleep(.1)
        require((root / ".nexora/scenes/Main.scene").exists(), "Native authoring save failed")
        scene_before = (root / ".nexora/scenes/Main.scene").read_bytes()
        send("ctrl+alt+b")
        # Real editable controls and discrete argv row; never a shell command string.
        click(150, 137)
        send("ctrl+a")
        tool("type", "--clearmodifiers", "--delay", "1", "--", str(Path(args.fixture).resolve()))
        click(118, 182)
        click(160, 211)
        tool("type", "--clearmodifiers", "--delay", "20", "--", "--sleep")
        send("ctrl+Return")
        wait_log(r"build process phase=2 operation=1 exit=unavailable artifact_verified=0")
        send("ctrl+shift+Return")
        wait_log(r"build process phase=6 operation=1 exit=unavailable artifact_verified=0")
        # Replace only argv; execute a real failure and then a zero exit. Neither is artifact success.
        click(160, 211)
        send("ctrl+a")
        tool("type", "--clearmodifiers", "--delay", "20", "--", "--fail")
        send("ctrl+Return")
        wait_log(r"build process phase=5 operation=2 exit=7 artifact_verified=0")
        click(160, 211)
        send("ctrl+a")
        tool("type", "--clearmodifiers", "--delay", "20", "--", "--args")
        send("ctrl+Return")
        wait_log(r"build process phase=4 operation=3 exit=0 artifact_verified=0")
        require((root / ".nexora/scenes/Main.scene").read_bytes() == scene_before,
                "Build process controls changed saved authoring")
        click(160, 211)
        send("ctrl+a")
        tool("type", "--clearmodifiers", "--delay", "20", "--", "--sleep")
        send("ctrl+Return")
        wait_log(r"build process phase=2 operation=4 exit=unavailable artifact_verified=0")
        close_started = time.monotonic()
        request_window_close(window, env)
        _, error = collect_output(process, 15)
        require(time.monotonic() - close_started < 3.5,
                "Closing did not cancel/drain the actual running build promptly")
        prefix = captured.decode("utf-8", errors="replace")
        print(prefix, end="", flush=True)
        require(process.returncode == 0, "Native build console failed graceful close")
        require(not re.search(r"Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+", prefix),
                "Native build console reported Vulkan validation errors")
        process = None
        require((root / ".nexora/scenes/Main.scene").read_bytes() == scene_before,
                "Closing build console changed authoring source")
        print("Native build console run/cancel/fail/zero-exit and authoring preservation passed")
    finally:
        if process is not None and process.poll() is None:
            if window is not None:
                request_window_close(window, env)
            try:
                process.communicate(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()
        xvfb.terminate()
        xvfb.communicate(timeout=10)
        shutil.rmtree(scratch)


if __name__ == "__main__":
    main()
