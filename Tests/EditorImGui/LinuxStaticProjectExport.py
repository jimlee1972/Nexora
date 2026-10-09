#!/usr/bin/env python3
"""Drive actual Editor StaticView export and verify with the standalone player."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument("--editor", required=True)
parser.add_argument("--player", required=True)
parser.add_argument("--xvfb", required=True)
parser.add_argument("--xdotool", required=True)
args = parser.parse_args()
from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window

def require(value, message):
    if not value:
        raise RuntimeError(message)

root = Path(tempfile.mkdtemp(prefix="nexora-native-static-export-")) / "資料 µ"
root.mkdir()
state = Path(tempfile.mkdtemp(prefix="nexora-native-static-export-state-"))
(root / "Content").mkdir()
(root / ".nexora").mkdir()
(root / "project.nexora").write_text("schema=1\nname=Static Export Acceptance\n")
(root / ".nexora/workspace").write_text("schema=1\n")
process = None
xvfb, display = start_xvfb(args.xvfb, "1280x900x24")
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
    def wait_until(predicate, message):
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if predicate():
                return
            if process.poll() is not None:
                _, error = process.communicate()
                raise RuntimeError(f"Editor exited during {message}: {error}")
            time.sleep(.05)
        raise RuntimeError(message)
    def export():
        send("mousemove", "--window", window, "53", "9", "click", "1")
        send("mousemove", "--window", window, "130", "33", "click", "1")
    def verify():
        result = subprocess.run([args.player, "--verify-package", str(package)],
                                env=env, capture_output=True, text=True, timeout=10)
        require(result.returncode == 0, f"Actual Player rejected package: {result.stderr}")
        report = json.loads(result.stdout)
        require(report["status"] == "VERIFIED_STATIC_VIEW" and
                report["native_rendering"] is False and report["gameplay_loaded"] is False,
                "Player did not report actual StaticView verification")
        return report
    process = launch(args.editor, root, state / "recent", env)
    window = wait_for_window(args.xdotool, env)
    send("windowfocus", "--sync", window)
    time.sleep(.8)
    send("mousemove", "--window", window, "100", "130", "click", "1")
    send("key", "--clearmodifiers", "ctrl+shift+s")
    send("key", "--clearmodifiers", "ctrl+a")
    send("type", "--clearmodifiers", "--delay", "2", "Content/Export.scene")
    send("key", "--clearmodifiers", "Return")
    scene = root / "Content/Export.scene"
    wait_until(scene.is_file, "Managed Save As failed")
    original_scene = scene.read_bytes()
    package = root / ".nexora/exports/static-view.nxproject"
    export()
    wait_until(package.is_file, "Native Build menu did not publish a StaticView package")
    original_package = package.read_bytes()
    initial = verify()
    require(initial["entity_count"] >= 1 and scene.read_bytes() == original_scene,
            "First native export lost scene or saved it implicitly")
    previous_stamp = package.stat().st_mtime_ns
    export()
    wait_until(lambda: package.stat().st_mtime_ns != previous_stamp,
               "Repeated native export did not publish")
    require(package.read_bytes() == original_package and verify() == initial,
            "Unchanged native input did not reproduce output")
    send("key", "--clearmodifiers", "ctrl+shift+n")
    export()
    wait_until(lambda: package.read_bytes() != original_package,
               "Native export omitted unsaved authoring edits")
    changed = verify()
    require(changed["entity_count"] == initial["entity_count"] + 1 and
            changed["scene_asset"] == initial["scene_asset"] and
            changed["project"] == initial["project"] and
            scene.read_bytes() == original_scene,
            "Unsaved native export changed source/identity or lost entity")
    # Undo the edit and return to a clean scene before real WM_DELETE_WINDOW shutdown.
    send("mousemove", "--window", window, "100", "130", "click", "1")
    send("key", "--clearmodifiers", "ctrl+z")
    request_window_close(window, env)
    output, error = process.communicate(timeout=15)
    require(process.returncode == 0, f"Native export Editor shutdown failed: {error}")
    require(scene.read_bytes() == original_scene and package.is_file(),
            "Native shutdown changed the saved scene/export")
    process = None
    print(json.dumps({"status": "NATIVE_EDITOR_STATIC_EXPORT_PASSED",
                      "initial_entities": initial["entity_count"],
                      "unsaved_entities": changed["entity_count"],
                      "deterministic": True, "source_preserved": True}))
finally:
    if process is not None and process.poll() is None:
        process.kill()
        process.communicate(timeout=5)
    xvfb.terminate()
    xvfb.communicate(timeout=5)
    shutil.rmtree(root.parent, ignore_errors=True)
    shutil.rmtree(state, ignore_errors=True)
