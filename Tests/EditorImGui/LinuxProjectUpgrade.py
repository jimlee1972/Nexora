#!/usr/bin/env python3
"""Inspect and explicitly open real legacy projects through the native browser."""
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


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    for argument in ("editor", "xvfb", "xdotool"):
        parser.add_argument("--" + argument, required=True)
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix="nexora-native-project-upgrade-"))
    root = scratch / "legacy"
    (root / "Content").mkdir(parents=True)
    descriptor = root / "project.nexora"
    original = b"schema=1\r\nname=Native Upgrade\r\n"
    descriptor.write_bytes(original)
    xvfb, display = start_xvfb(args.xvfb, "1280x900x24")
    process = None
    window = None
    try:
        require(display is not None, "Xvfb did not become ready")
        env = os.environ.copy()
        env["DISPLAY"] = display
        env["XDG_STATE_HOME"] = str(scratch / "state")
        captured = bytearray()

        def send(*arguments):
            if arguments[0] == "key":
                arguments = ("key", "--delay", "100", *arguments[1:])
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(.2)

        def wait_log(pattern):
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                require(process.poll() is None, "Editor exited during preview")
                if select.select([process.stderr], [], [], .1)[0]:
                    captured.extend(os.read(process.stderr.fileno(), 65536))
                if re.search(pattern, captured.decode("utf-8", errors="replace")):
                    return
            raise RuntimeError(f"Missing native diagnostic {pattern}: {captured!r}")

        def start(label, read_only=False):
            nonlocal process, window
            captured.clear()
            process = launch(args.editor, None, scratch / (label + "-recent"), env,
                             read_only=read_only)
            window = wait_for_window(args.xdotool, env)
            send("windowfocus", "--sync", window)
            time.sleep(.8)
            send("mousemove", "--window", window, "120", "70", "click", "1")
            send("key", "--clearmodifiers", "ctrl+a")
            send("type", "--clearmodifiers", "--delay", "1", str(root))

        def finish():
            nonlocal process
            request_window_close(window, env)
            _, error = collect_output(process, 15)
            prefix = captured.decode("utf-8", errors="replace")
            print(prefix, end="", flush=True)
            require(process.returncode == 0, f"Native shutdown failed: {error}")
            require(not re.search(r"Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+", prefix),
                    "Preview reported Vulkan validation errors")
            process = None
            return prefix + error

        for read_only in (True, False):
            start("readonly" if read_only else "legacy", read_only)
            send("key", "--clearmodifiers", "ctrl+alt+m")
            wait_log(r"project upgrade preview ready from=1 required=1 documents=0")
            require(descriptor.read_bytes() == original and not (root / ".nexora").exists(),
                    "Preview changed source or created project metadata")
            if read_only:
                finish()
                require(descriptor.read_bytes() == original and not (root / ".nexora").exists(),
                        "Read-only preview shutdown wrote project metadata")
            else:
                send("key", "--clearmodifiers", "ctrl+o")
                deadline = time.monotonic() + 15
                while descriptor.read_bytes() == original and time.monotonic() < deadline:
                    require(process.poll() is None, "Editor exited before explicit Open")
                    time.sleep(.05)
                require(descriptor.read_bytes().startswith(b"schema=2\n"), "Explicit Open did not upgrade")
                recent = scratch / "legacy-recent"
                deadline = time.monotonic() + 15
                while not (recent.is_file() and str(root.resolve()) in recent.read_text()) and time.monotonic() < deadline:
                    require(process.poll() is None, "Editor exited before project activation")
                    time.sleep(.05)
                require(recent.is_file() and str(root.resolve()) in recent.read_text(),
                        "Explicit Open did not finish background activation")
                # Writable activation bootstraps a dirty scene. Explicitly save that scene
                # through the real host before requesting graceful close.
                send("mousemove", "--window", window, "500", "220", "click", "1")
                send("key", "--clearmodifiers", "ctrl+s")
                scene = root / ".nexora/scenes/Main.scene"
                deadline = time.monotonic() + 10
                while not scene.is_file() and time.monotonic() < deadline:
                    require(process.poll() is None, "Editor exited before explicit scene Save")
                    time.sleep(.05)
                require(scene.is_file(), "Activated project did not save its real bootstrap scene")
                result = finish()
                require("selector=opened" in result and "access=read-write" in result,
                        "Explicit Open did not activate the selected project")
                require((root / ".nexora/project-upgrade.schema1.backup").read_bytes() == original,
                        "Explicit Open did not retain the exact legacy backup")
                require(b"publication=plan-only" in (root / ".nexora/project-upgrade.schema1.report").read_bytes(),
                        "Upgrade evidence misrepresented publication")
        current = descriptor.read_bytes()
        backup = (root / ".nexora/project-upgrade.schema1.backup").read_bytes()
        start("current", True)
        send("key", "--clearmodifiers", "ctrl+alt+m")
        wait_log(r"project upgrade preview ready from=2 required=0 documents=")
        finish()
        require(descriptor.read_bytes() == current and
                (root / ".nexora/project-upgrade.schema1.backup").read_bytes() == backup,
                "Current-schema inspection changed retained source/evidence")
        descriptor.write_bytes(b"schema=99\nname=Unsupported\n")
        start("corrupt", True)
        send("key", "--clearmodifiers", "ctrl+alt+m")
        wait_log(r"project upgrade preview failed")
        finish()
        require(descriptor.read_bytes() == b"schema=99\nname=Unsupported\n" and
                (root / ".nexora/project-upgrade.schema1.backup").read_bytes() == backup,
                "Failed preview replaced the unsupported descriptor or backup")
        print("Native no-write project preview and explicit backed-up upgrade passed")
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.communicate()
        xvfb.terminate()
        xvfb.communicate(timeout=10)
        shutil.rmtree(scratch)


if __name__ == "__main__":
    main()
