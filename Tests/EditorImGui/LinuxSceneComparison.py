#!/usr/bin/env python3
"""Inspect actual external scene revisions through the native Editor without publication."""
import argparse
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import (
    collect_output, launch, request_window_close, start_xvfb, wait_for_window,
)


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", required=True)
    parser.add_argument("--xvfb", required=True)
    parser.add_argument("--xdotool", required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix="nexora-native-comparison-"))
    state = Path(tempfile.mkdtemp(prefix="nexora-native-comparison-state-"))
    (root / "Content").mkdir()
    (root / ".nexora").mkdir()
    (root / "project.nexora").write_text("schema=1\nname=Semantic Native\n")
    (root / ".nexora/workspace").write_text("schema=1\n")
    xvfb, display = start_xvfb(args.xvfb, "1280x900x24")
    process = None
    passed = False
    try:
        require(display is not None, "Xvfb did not become ready")
        env = os.environ.copy()
        env["DISPLAY"] = display
        env["XDG_STATE_HOME"] = str(state)

        def send(*arguments):
            if arguments[0] == "key":
                arguments = ("key", "--delay", "100", *arguments[1:])
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(.2)

        def wait_file(path):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and not path.is_file():
                require(process.poll() is None, "Editor exited before managed save")
                time.sleep(.05)
            require(path.is_file(), "Managed source save failed")

        captured = bytearray()

        def wait_log(pattern):
            deadline = time.monotonic() + 15
            start = len(captured)
            while time.monotonic() < deadline:
                require(process.poll() is None, "Editor exited during comparison")
                if select.select([process.stderr], [], [], .1)[0]:
                    captured.extend(os.read(process.stderr.fileno(), 65536))
                if re.search(pattern, captured[start:].decode("utf-8", errors="replace")):
                    return
            raise RuntimeError(f"Native comparison diagnostic missing: {pattern}; {captured!r}")

        def finish():
            request_window_close(window, env)
            _, error = collect_output(process, 15)
            prefix = captured.decode("utf-8", errors="replace")
            print(prefix, end="", flush=True)
            require(process.returncode == 0, f"Editor shutdown failed: {error}")
            require(not re.search(r"Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+", prefix),
                    "Native comparison reported Vulkan validation errors")

        process = launch(args.editor, root, state / "recent", env)
        window = wait_for_window(args.xdotool, env)
        send("windowfocus", "--sync", window)
        time.sleep(.8)
        send("key", "--clearmodifiers", "ctrl+shift+n")
        send("key", "--clearmodifiers", "ctrl+shift+s")
        send("key", "--clearmodifiers", "ctrl+a")
        send("type", "--clearmodifiers", "--delay", "2", "Content/Main.scene")
        send("key", "--clearmodifiers", "Return")
        scene = root / "Content/Main.scene"
        wait_file(scene)
        base = scene.read_bytes()
        lines = base.decode("utf-8").splitlines(keepends=True)
        for index, line in enumerate(lines):
            if line.startswith("node "):
                fields = line.split(" ", 3)
                lines[index] = " ".join(fields[:3]) + " Disk authored name\n"
                break
        else:
            raise RuntimeError("Actual native saved source has no authoring node")
        disk = "".join(lines).encode("utf-8")
        require(disk != base, "Actual external revision did not change")
        scene.write_bytes(disk)
        send("key", "--clearmodifiers", "ctrl+shift+n")
        send("key", "--clearmodifiers", "ctrl+alt+d")
        wait_log(r"scene comparison ready fields=[1-9][0-9]* conflicts=")
        require(scene.read_bytes() == disk, "Comparison saved unsaved authoring or replaced disk")
        send("mousemove", "--window", window, "100", "130", "click", "1")
        send("key", "--clearmodifiers", "ctrl+z")
        finish()
        require(scene.read_bytes() == disk, "Shutdown changed the external revision")
        process = None
        captured.clear()
        process = launch(args.editor, root, state / "reader-recent", env, read_only=True)
        window = wait_for_window(args.xdotool, env)
        send("windowfocus", "--sync", window)
        time.sleep(.8)
        send("key", "--clearmodifiers", "ctrl+alt+d")
        wait_log(r"scene comparison ready fields=0 conflicts=0")
        scene.write_bytes(b"corrupt external scene")
        send("key", "--clearmodifiers", "ctrl+alt+d")
        wait_log(r"scene comparison finished: .*corrupt")
        require(scene.read_bytes() == b"corrupt external scene", "Failed comparison changed corrupt source")
        scene.write_bytes(disk)
        finish()
        process = None
        require(scene.read_bytes() == disk, "Read-only inspection changed source")
        passed = True
        print("NATIVE_EDITOR_SEMANTIC_COMPARISON_PASSED source_preserved read_only corrupt_rejected")
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.communicate(timeout=5)
        xvfb.terminate()
        xvfb.communicate(timeout=5)
        if passed:
            shutil.rmtree(root)
            shutil.rmtree(state)
        else:
            print(f"Native comparison fixtures retained: {root} {state}", flush=True)


if __name__ == "__main__":
    main()
