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
    parser.add_argument("--candidate", action="store_true")
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
        if args.candidate:
            env["NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY"] = "1"

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

        def wait_log(pattern, start=None):
            deadline = time.monotonic() + 15
            if start is None:
                start = len(captured)
            while time.monotonic() < deadline:
                require(process.poll() is None, "Editor exited during comparison")
                if select.select([process.stderr], [], [], .1)[0]:
                    captured.extend(os.read(process.stderr.fileno(), 65536))
                if re.search(pattern, captured[start:].decode("utf-8", errors="replace")):
                    return
            raise RuntimeError(f"Native comparison diagnostic missing: {pattern}; {captured!r}")

        def candidate_geometry(disk_choice=False, validated=False):
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                if select.select([process.stderr], [], [], .1)[0]:
                    captured.extend(os.read(process.stderr.fileno(), 65536))
                lines = re.findall(r"scene merge ui [^\n]*", captured.decode("utf-8", errors="replace"))
                for line in reversed(lines):
                    controls = dict((key, (float(x), float(y))) for key, x, y in re.findall(
                        r"(validate|disk|cancel)=([0-9.]+),([0-9.]+)", line))
                    if ("validate" in controls and (not disk_choice or "disk" in controls)
                            and (not validated or "validated=1" in line)):
                        return controls
                require(process.poll() is None, "Editor exited before actual candidate controls")
            raise RuntimeError(f"Candidate controls unavailable: {captured!r}")

        def validate_candidate(disk_choice=False):
            controls = candidate_geometry(disk_choice)
            start = len(captured)
            def click(point):
                send("mousemove", "--window", window, str(round(point[0])), str(round(point[1])))
                send("mousedown", "1")
                time.sleep(.25)
                send("mouseup", "1")
            if disk_choice:
                click(controls["disk"])
            click(controls["validate"])
            wait_log(r"scene merge candidate finished: Complete captured candidate validated", start)
            require(re.search(r"scene merge candidate started choices=" + ("1" if disk_choice else "0"),
                              captured[start:].decode("utf-8", errors="replace")),
                    "Actual native candidate request lost explicit choices")
            candidate_geometry(validated=True)

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
        if args.candidate:
            validate_candidate(disk_choice=True)
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
        if args.candidate:
            validate_candidate()
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
        if args.candidate:
            print("NATIVE_EDITOR_MERGE_CANDIDATE_PASSED explicit_choice validated read_only source_preserved")
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
