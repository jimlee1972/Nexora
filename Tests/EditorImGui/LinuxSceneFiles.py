#!/usr/bin/env python3
"""Drive real scene New/Open/Save As requests through the native Editor application."""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window


def wait_until(predicate, message, process, timeout=10):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        if process.poll() is not None:
            _, error = process.communicate()
            raise RuntimeError(f"Editor exited during {message}: {error}")
        time.sleep(0.05)
    raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", required=True)
    parser.add_argument("--xvfb", required=True)
    parser.add_argument("--xdotool", required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix="nexora-native-scene-files-"))
    state = Path(tempfile.mkdtemp(prefix="nexora-native-scene-state-"))
    (root / "Content").mkdir()
    (root / ".nexora").mkdir()
    (root / "project.nexora").write_text("schema=1\nname=Scene Files Acceptance\n")
    (root / ".nexora/workspace").write_text("schema=1\n")
    xvfb, display = start_xvfb(args.xvfb, "1280x900x24")
    process = None
    try:
        if display is None:
            raise RuntimeError("Xvfb did not become ready")
        env = os.environ.copy()
        env["DISPLAY"] = display
        env["XDG_STATE_HOME"] = str(state)

        def send(*arguments):
            # Keep chord presses visible across native GPU frames and ImGui's trickled queue.
            if arguments[0] == "key":
                arguments = ("key", "--delay", "100", *arguments[1:])
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(0.20)

        def path_dialog(shortcut, path):
            send("key", "--clearmodifiers", shortcut)
            send("key", "--clearmodifiers", "ctrl+a")
            send("type", "--clearmodifiers", "--delay", "2", path)
            send("key", "--clearmodifiers", "Return")

        def start(read_only=False):
            nonlocal process
            process = launch(args.editor, root, state / "recent", env, read_only=read_only)
            window = wait_for_window(args.xdotool, env)
            send("windowfocus", "--sync", window)
            # Establish application and panel focus after the first native layout frame.
            time.sleep(0.8)
            send("mousemove", "--window", window, "100", "130", "click", "1")
            return process, window

        process, window = start()
        main_scene = root / ".nexora/scenes/Main.scene"
        send("key", "--clearmodifiers", "ctrl+s")
        wait_until(main_scene.is_file, "initial Save did not create Main.scene", process)
        original = main_scene.read_bytes()
        send("key", "--clearmodifiers", "ctrl+shift+n")
        copy = root / "Content/Copy.scene"
        path_dialog("ctrl+shift+s", "Content/Copy.scene")
        wait_until(copy.is_file, "Save As did not write its typed destination", process)
        copied = copy.read_bytes()
        if copied == original or main_scene.read_bytes() != original:
            raise RuntimeError("Save As changed its source or failed to include the authored entity")
        send("key", "--clearmodifiers", "ctrl+n")
        empty = root / "Content/Empty.scene"
        path_dialog("ctrl+shift+s", "Content/Empty.scene")
        wait_until(empty.is_file, "New/Save As did not create an empty scene", process)
        if b"node " in empty.read_bytes() or b"node " not in copied:
            raise RuntimeError("New retained the old document's hierarchy")
        # An edit/save after each Open proves the application actually adopted that document.
        # Checking unchanged destination bytes alone could pass even if the Open shortcut was lost.
        for relative, destination, baseline in ((".nexora/scenes/Main.scene", main_scene, original),
                                                ("Content/Copy.scene", copy, copied)):
            path_dialog("ctrl+o", relative)
            send("key", "--clearmodifiers", "ctrl+shift+n")
            send("key", "--clearmodifiers", "ctrl+s")
            wait_until(lambda: destination.read_bytes().count(b"node ") ==
                       baseline.count(b"node ") + 1,
                       f"Open did not adopt the live document/save path: {relative}", process)
            if b"node " in empty.read_bytes():
                raise RuntimeError("Editing an opened scene still saved the previous empty document")
            send("key", "--clearmodifiers", "ctrl+z")
            send("key", "--clearmodifiers", "ctrl+s")
            wait_until(lambda: destination.read_bytes() == baseline,
                       f"Opened document Undo/Save lost scene data: {relative}", process)
        # Native close on Untitled must retain the Save and Exit intent through a rejected path.
        send("key", "--clearmodifiers", "ctrl+n")
        request_window_close(window, env)
        time.sleep(0.4)
        # Fresh 1280x720 window: click the real first close-modal button (Save and Exit).
        send("mousemove", "--window", window, "545", "378", "click", "1")
        send("key", "--clearmodifiers", "ctrl+a")
        send("type", "--clearmodifiers", "--delay", "2", "../Rejected.scene")
        send("key", "--clearmodifiers", "Return")
        if process.poll() is not None:
            raise RuntimeError("Rejected Save and Exit dismissed the Editor")
        send("key", "--clearmodifiers", "ctrl+a")
        send("type", "--clearmodifiers", "--delay", "2", "Content/Exit.scene")
        send("key", "--clearmodifiers", "Return")
        exit_scene = root / "Content/Exit.scene"
        wait_until(exit_scene.is_file, "Save and Exit did not retain a retryable path dialog", process)
        _, error = process.communicate(timeout=15)
        if process.returncode != 0:
            raise RuntimeError(f"Scene workflow exit failed: {error}")
        process = None
        for source in (main_scene, copy, empty, exit_scene):
            path = source if source == main_scene else root / ".nexora/scenes/views" / source.relative_to(root)
            if not path.with_suffix(".overview.camera").is_file() or not path.with_suffix(
                    ".preview.camera").is_file():
                raise RuntimeError(f"Scene camera state was not retained for {path}")
        before = {path.relative_to(root): path.read_bytes()
                  for path in root.rglob("*") if path.is_file() and path.name != "editor.lock"}
        process, window = start(read_only=True)
        path_dialog("ctrl+o", "Content/Copy.scene")
        send("key", "--clearmodifiers", "ctrl+n", "ctrl+s", "ctrl+shift+s")
        request_window_close(window, env)
        _, error = process.communicate(timeout=15)
        if process.returncode != 0:
            raise RuntimeError(f"Read-only scene workflow exit failed: {error}")
        process = None
        after = {path.relative_to(root): path.read_bytes()
                 for path in root.rglob("*") if path.is_file() and path.name != "editor.lock"}
        if after != before:
            raise RuntimeError("Read-only scene workflow modified project files")
        print("Native scene New/Open/Save As and per-scene camera persistence passed")
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.communicate(timeout=5)
        xvfb.terminate()
        xvfb.communicate(timeout=5)
        shutil.rmtree(root, ignore_errors=True)
        shutil.rmtree(state, ignore_errors=True)


if __name__ == "__main__":
    main()
