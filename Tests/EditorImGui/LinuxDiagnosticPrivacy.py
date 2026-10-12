#!/usr/bin/env python3
"""Exercise explicit local diagnostic consent without source writes or persistent queues."""
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
    for name in ("editor", "xvfb", "xdotool"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix="nexora-native-diagnostic-privacy-"))
    root = scratch / "project"
    (root / "Content").mkdir(parents=True)
    descriptor = root / "project.nexora"
    descriptor.write_bytes(b"schema=1\nname=Native Diagnostic Privacy\n")
    before = {p.relative_to(root): p.read_bytes() for p in root.rglob("*") if p.is_file()}
    xvfb, display = start_xvfb(args.xvfb, "1280x900x24")
    process = None
    try:
        require(display is not None, "Xvfb did not become ready")
        env = os.environ.copy()
        env["DISPLAY"] = display
        env["XDG_STATE_HOME"] = str(scratch / "state")
        env["MESA_SHADER_CACHE_DIR"] = str(scratch / "shader-cache")
        captured = bytearray()

        def send(*keys):
            subprocess.run([args.xdotool, "key", "--clearmodifiers", "--delay", "100", *keys],
                           env=env, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(.2)

        def wait_log(pattern, start=0):
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                require(process.poll() is None, "Editor exited during privacy controls")
                if select.select([process.stderr], [], [], .1)[0]:
                    captured.extend(os.read(process.stderr.fileno(), 65536))
                if re.search(pattern, captured[start:].decode("utf-8", errors="replace")):
                    return
            raise RuntimeError(f"Missing privacy diagnostic {pattern}: {captured!r}")

        process = launch(args.editor, root, scratch / "recent", env, read_only=True)
        window = wait_for_window(args.xdotool, env)
        subprocess.run([args.xdotool, "windowfocus", "--sync", window], env=env, check=True)
        time.sleep(.8)
        wait_log(r"diagnostic privacy: enabled=0 retained=0 storage=none transport=none")
        send("ctrl+alt+t")
        # Tab enters native keyboard navigation at the actual default checkbox.
        send("Tab", "space")
        wait_log(r"diagnostic consent changed enabled=1 retained=0")
        wait_log(r"diagnostic queue retained=1")
        send("Tab", "space")
        wait_log(r"diagnostic queue cleared retained=0")
        send("shift+Tab", "space")
        wait_log(r"diagnostic consent changed enabled=0 retained=0")
        cursor = len(captured)
        send("space")
        wait_log(r"diagnostic consent changed enabled=1 retained=0", cursor)
        request_window_close(window, env)
        # This source-less read-only fixture owns an unsaved bootstrap. Retain the
        # existing close guard and explicitly discard that in-memory bootstrap.
        time.sleep(.4)
        subprocess.run([args.xdotool, "mousemove", "--window", window, "655", "378", "click", "1"],
                       env=env, check=True)
        _, error = collect_output(process, 15)
        prefix = captured.decode("utf-8", errors="replace")
        print(prefix, end="", flush=True)
        require(process.returncode == 0 and "diagnostic privacy close: enabled=0 retained=0" in error,
                "Closing retained diagnostic consent/data")
        require(not re.search(r"Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+", prefix),
                "Native privacy reported Vulkan validation errors")
        process = None
        after = {p.relative_to(root): p.read_bytes() for p in root.rglob("*") if p.is_file()}
        require(before == after, "Diagnostic controls changed project source/metadata")
        process = launch(args.editor, root, scratch / "recent", env, frames=80, read_only=True)
        _, error = collect_output(process, 20)
        require(process.returncode == 0 and "diagnostic privacy: enabled=0 retained=0" in error and
                "diagnostic queue retained=" not in error,
                "Restart retained consent or admitted events without opt-in")
        process = None
        require(before == {p.relative_to(root): p.read_bytes() for p in root.rglob("*") if p.is_file()},
                "Privacy restart wrote project data")
        print("Native explicit diagnostic consent/clear/opt-out/reset passed")
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.communicate()
        xvfb.terminate()
        xvfb.communicate(timeout=10)
        shutil.rmtree(scratch)


if __name__ == "__main__":
    main()
