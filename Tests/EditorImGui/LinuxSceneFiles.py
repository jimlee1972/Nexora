#!/usr/bin/env python3
"""Drive real scene New/Open/Save As requests through the native Editor application."""

import argparse
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from NativeSceneInput import NativeSceneInput


def wait_until(predicate, message, process, timeout=10):
    """Legacy filesystem wait also imported by the native tab workflow."""
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
    driver = None
    launch_number = 0
    passed = False
    try:
        if display is None:
            raise RuntimeError("Xvfb did not become ready")
        env = os.environ.copy()
        env["DISPLAY"] = display
        env["XDG_STATE_HOME"] = str(state)
        env["NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY"] = "1"
        env["NEXORA_PROJECT_ASSET_WORKFLOW_EVIDENCE"] = "1"

        def send(*arguments):
            driver.send(*arguments)

        def content_observation():
            records = re.findall(rb'^project asset workflow ([^\n]*)\n', driver.stderr, re.M)
            return dict(part.split('=', 1) for part in records[-1].decode().split()) if records else {}

        def wait_scene(predicate, message, process, timeout=10):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                driver.drain(min(.05, max(0, deadline - time.monotonic())))
                if time.monotonic() >= deadline:
                    break
                try:
                    if predicate():
                        return
                except FileNotFoundError:
                    pass
                if process.poll() is not None:
                    break
            metadata_path = root / '.nexora/scene-session.ini'
            facts = {'copy_exists': (root / 'Content/Copy.scene').is_file(),
                     'renamed_exists': (root / 'Content/Renamed.scene').is_file(),
                     'startup': metadata_path.read_bytes() if metadata_path.is_file() else None,
                     'content': content_observation(), 'gui': driver.observation()}
            driver.retain()
            raise RuntimeError(f'{message}; actual state={facts!r}')

        def content_point(name):
            wait_scene(lambda: name in content_observation(),
                       'Actual Content control unavailable: ' + name, process)
            return tuple(map(float, content_observation()[name].split(',')))

        def content_click(name):
            driver.click(content_point(name), hold=0)

        def content_context(name):
            x, y = map(round, content_point('scene_point'))
            send('mousemove', '--window', window, str(x), str(y), 'click', '3')
            content_click(name)

        def rename_from_context(name):
            content_context('context_rename')
            content_click('rename_field')
            send('key', '--clearmodifiers', 'ctrl+a')
            send('type', '--clearmodifiers', '--delay', '2', name)
            wait_scene(lambda: content_observation().get('rename_text_hex') == name.encode().hex(),
                       'Native rename text was not committed to its actual field', process)
            content_click('rename_apply')

        def path_dialog(shortcut, path):
            # Gesture, actual modal, field focus and typed text share one absolute 10s bound.
            # Never retry the authoring command or type into an unobserved widget.
            deadline = time.monotonic() + 10
            if shortcut == "ctrl+o":
                driver.file_open(deadline)
                dialog = '1'  # Actual OpenPath state.
            else:
                driver.chord(shortcut, deadline)
                dialog = '2'  # Actual SavePath state.
            value = driver.wait(lambda v: v['file_dialog'] == dialog and
                                v['file_path_ready'] == '1' and 'file_path_point' in v,
                                'Scene path modal was not actually rendered', deadline)
            x, y = map(round, map(float, value['file_path_point'].split(',')))
            driver.pointer_move(x, y, deadline)
            driver.pointer_button(1, True, deadline)
            driver.pointer_button(1, False, deadline)
            driver.wait(lambda v: v['file_dialog'] == dialog and
                        v['file_path_active'] == '1' and v['text_input'] == '1',
                        'Scene path field did not acquire actual text focus', deadline)
            driver.chord('ctrl+a', deadline)
            driver.raw('type', '--clearmodifiers', '--delay', '2', path, deadline=deadline)
            driver.wait(lambda v: v['file_dialog'] == dialog and
                        v['file_path_hex'] == path.encode().hex(),
                        'Scene path field did not receive its committed destination', deadline)
            driver.chord('Return', deadline)

        def close_action(name):
            value = driver.wait(lambda v: name in v,
                                'Actual close-modal action unavailable: ' + name)
            x, y = map(round, map(float, value[name].split(',')))
            deadline = time.monotonic() + 8
            driver.pointer_move(x, y, deadline)
            # A successful Discard closes on button release. Send the complete native
            # click; process exit/file state below acknowledges the terminal action.
            driver.pointer_button(1, True, deadline)
            driver.raw('mouseup', '1', deadline=deadline)

        def start(read_only=False):
            nonlocal process, driver, launch_number
            process = launch(args.editor, root, state / "recent", env, read_only=read_only, binary=True)
            launch_number += 1
            driver = NativeSceneInput(process, args.xdotool, env, "0", state, launch_number,
                                      phase_wait=.1, ack_pointer=True)
            window = wait_for_window(args.xdotool, env)
            driver.window = str(window)
            send("windowfocus", "--sync", window)
            # Establish application and panel focus after the first native layout frame.
            time.sleep(0.8)
            driver.wait(lambda value: value["focused"] == "1" and value["selector"] == "0",
                        "Scene file startup did not admit native input")
            send("mousemove", "--window", window, "100", "130", "click", "1")
            return process, window

        process, window = start()
        main_scene = root / ".nexora/scenes/Main.scene"
        send("key", "--clearmodifiers", "ctrl+s")
        wait_scene(main_scene.is_file, "initial Save did not create Main.scene", process)
        original = main_scene.read_bytes()
        send("key", "--clearmodifiers", "ctrl+shift+n")
        copy = root / "Content/Copy.scene"
        path_dialog("ctrl+shift+s", "Content/Copy.scene")
        wait_scene(copy.is_file, "Save As did not write its typed destination", process)
        copied = copy.read_bytes()
        if copied == original or main_scene.read_bytes() != original:
            raise RuntimeError("Save As changed its source or failed to include the authored entity")
        # Fresh layout, one Content row: rename the actual open asset with focused F2/Enter.
        # Save/Undo after relocation proves that the application's association follows the UUID.
        identity = copy.with_suffix(".scene.meta").read_bytes()
        content_click("scene_point")
        send("key", "--clearmodifiers", "F2")
        content_point("rename_field")
        driver.wait(lambda value: value["text_input"] == "1", "F2 did not focus rename input")
        send("type", "--clearmodifiers", "--delay", "2", "Renamed.scene")
        wait_scene(lambda: content_observation().get("rename_text_hex") == b"Renamed.scene".hex(),
                   "Focused F2 rename did not receive its typed name", process)
        send("key", "--clearmodifiers", "Return")
        renamed = root / "Content/Renamed.scene"
        metadata = root / ".nexora/scene-session.ini"
        wait_scene(lambda: renamed.is_file() and not copy.exists() and
                   metadata.read_bytes().endswith(b"scene=Content/Renamed.scene\n"),
                   "Content rename did not update the clean scene/startup path", process)
        if renamed.with_suffix(".scene.meta").read_bytes() != identity:
            raise RuntimeError("Content rename replaced the scene asset UUID")
        send("mousemove", "--window", window, "500", "220", "click", "1")
        send("key", "--clearmodifiers", "ctrl+shift+n", "ctrl+s")
        wait_scene(lambda: renamed.read_bytes().count(b"node ") == copied.count(b"node ") + 1,
                   "Save after Content rename did not adopt its new destination", process)
        if copy.exists():
            raise RuntimeError("Save after Content rename recreated the old scene source")
        send("key", "--clearmodifiers", "ctrl+z", "ctrl+s")
        wait_scene(lambda: renamed.read_bytes() == copied,
                   "Scene Undo after Content rename lost history", process)
        content_click("undo")
        wait_scene(lambda: copy.is_file() and not renamed.exists() and
                   metadata.read_bytes().endswith(b"scene=Content/Copy.scene\n"),
                   "Content Undo did not restore the current scene/startup path", process)
        # Delete the same active asset; a new scene edit must not recreate its absent source.
        content_context("context_delete")
        wait_scene(lambda: not copy.exists(), "Active scene Content delete failed", process)
        send("mousemove", "--window", window, "500", "220", "click", "1")
        send("key", "--clearmodifiers", "ctrl+shift+n", "ctrl+s")
        if copy.exists():
            raise RuntimeError("Save silently recreated a deleted active scene asset")
        content_click("undo")
        wait_scene(copy.is_file, "Content delete Undo did not restore the scene asset", process)
        send("key", "--clearmodifiers", "ctrl+s")
        wait_scene(lambda: copy.read_bytes().count(b"node ") == copied.count(b"node ") + 1,
                   "Content delete Undo did not restore Save or retained scene edits", process)
        send("key", "--clearmodifiers", "ctrl+z", "ctrl+s")
        wait_scene(lambda: copy.read_bytes() == copied,
                   "Scene Undo after Content delete lost its original document", process)
        # A committed asset rename must update startup location even with an unsaved World.
        # Discard closes without saving that World; restart must load the relocated committed file.
        send("mousemove", "--window", window, "500", "220", "click", "1")
        send("key", "--clearmodifiers", "ctrl+shift+n")
        expected_nodes = len(re.findall(rb"^node ", copied, re.M)) + 1
        wait_scene(lambda: content_observation().get("dirty") == "1" and
                   content_observation().get("nodes") == str(expected_nodes),
                   "Dirty relocation did not start with the committed unsaved scene edit", process)
        rename_from_context("Renamed.scene")
        wait_scene(lambda: renamed.is_file() and not copy.exists() and
                   metadata.read_bytes().endswith(b"scene=Content/Renamed.scene\n"),
                   "Dirty Content relocation left a stale startup filename", process)
        if renamed.read_bytes() != copied:
            raise RuntimeError("Dirty relocation unexpectedly saved document changes")
        request_window_close(window, env)
        close_action("close_discard")
        _, error = driver.finish(15)
        if process.returncode != 0 or renamed.read_bytes() != copied:
            raise RuntimeError(f"Discard after dirty relocation changed its committed source: {error}")
        process = None
        process, window = start()
        send("key", "--clearmodifiers", "ctrl+shift+n", "ctrl+s")
        wait_scene(lambda: renamed.read_bytes().count(b"node ") == copied.count(b"node ") + 1,
                   "Restart after dirty relocation did not adopt the committed scene", process)
        send("key", "--clearmodifiers", "ctrl+z", "ctrl+s")
        wait_scene(lambda: renamed.read_bytes() == copied,
                   "Restart loaded discarded changes after dirty relocation", process)
        # Return to the original path for the remaining New/Open/Save As acceptance scenarios.
        rename_from_context("Copy.scene")
        wait_scene(lambda: copy.is_file() and not renamed.exists(),
                   "Restored process did not rename its active scene back", process)
        send("key", "--clearmodifiers", "ctrl+n")
        empty = root / "Content/Empty.scene"
        path_dialog("ctrl+shift+s", "Content/Empty.scene")
        wait_scene(empty.is_file, "New/Save As did not create an empty scene", process)
        if b"node " in empty.read_bytes() or b"node " not in copied:
            raise RuntimeError("New retained the old document's hierarchy")
        # An edit/save after each Open proves the application actually adopted that document.
        # Checking unchanged destination bytes alone could pass even if the Open shortcut was lost.
        for relative, destination, baseline in ((".nexora/scenes/Main.scene", main_scene, original),
                                                ("Content/Copy.scene", copy, copied)):
            path_dialog("ctrl+o", relative)
            # Open is deferred until after widgets. Observe the production startup association
            # before emitting a new authoring command, retaining the live edit/save/Undo proof.
            wait_scene(lambda: metadata.is_file() and
                       metadata.read_bytes().endswith(f"scene={relative}\n".encode("utf-8")),
                       f"Open did not acknowledge its adopted scene path: {relative}", process)
            # The path field owned text input. Restore Scene focus before authoring shortcuts;
            # Ctrl+Shift+N is deliberately disabled while ImGui still wants text input.
            send("mousemove", "--window", window, "500", "220", "click", "1")
            send("key", "--clearmodifiers", "ctrl+shift+n")
            send("key", "--clearmodifiers", "ctrl+s")
            wait_scene(lambda: destination.read_bytes().count(b"node ") ==
                       baseline.count(b"node ") + 1,
                       f"Open did not adopt the live document/save path: {relative}", process)
            if b"node " in empty.read_bytes():
                raise RuntimeError("Editing an opened scene still saved the previous empty document")
            send("key", "--clearmodifiers", "ctrl+z")
            send("key", "--clearmodifiers", "ctrl+s")
            wait_scene(lambda: destination.read_bytes() == baseline,
                       f"Opened document Undo/Save lost scene data: {relative}", process)
        # Native close on Untitled must retain the Save and Exit intent through a rejected path.
        send("key", "--clearmodifiers", "ctrl+n")
        request_window_close(window, env)
        close_action("close_save")
        driver.wait(lambda v: v['file_path_ready'] == '1' and v['file_path_active'] == '1',
                    'Save and Exit did not focus its actual path modal')
        send("key", "--clearmodifiers", "ctrl+a")
        send("type", "--clearmodifiers", "--delay", "2", "../Rejected.scene")
        send("key", "--clearmodifiers", "Return")
        if process.poll() is not None:
            raise RuntimeError("Rejected Save and Exit dismissed the Editor")
        send("key", "--clearmodifiers", "ctrl+a")
        send("type", "--clearmodifiers", "--delay", "2", "Content/Exit.scene")
        # Successful Save and Exit terminates on Enter-down. Send the complete native
        # gesture; the saved file and exit status below acknowledge this terminal action.
        driver.raw("key", "--delay", "100", "--clearmodifiers", "Return")
        exit_scene = root / "Content/Exit.scene"
        wait_scene(exit_scene.is_file, "Save and Exit did not retain a retryable path dialog", process)
        _, error = driver.finish(15)
        if process.returncode != 0:
            raise RuntimeError(f"Scene workflow exit failed: {error}")
        process = None
        for source in (main_scene, copy, empty, exit_scene):
            path = source if source == main_scene else root / ".nexora/scenes/views" / source.relative_to(root)
            if not path.with_suffix(".overview.camera").is_file() or not path.with_suffix(
                    ".preview.camera").is_file():
                raise RuntimeError(f"Scene camera state was not retained for {path}")
        metadata = root / ".nexora/scene-session.ini"
        if not metadata.read_bytes().endswith(b"scene=Content/Exit.scene\n"):
            raise RuntimeError("Save and Exit did not remember its managed scene")
        # A real writable restart must edit/save Exit.scene, rather than silently reopening Main.
        # The visible filename alone is not enough evidence that the live World was restored.
        exit_before = exit_scene.read_bytes()
        process, window = start()
        send("key", "--clearmodifiers", "ctrl+shift+n", "ctrl+s")
        wait_scene(lambda: exit_scene.read_bytes().count(b"node ") ==
                   exit_before.count(b"node ") + 1,
                   "Restart did not restore the last scene and its save destination", process)
        if main_scene.read_bytes() != original:
            raise RuntimeError("Startup restoration edited Main instead of the remembered scene")
        request_window_close(window, env)
        _, error = driver.finish(15)
        if process.returncode != 0:
            raise RuntimeError(f"Restored scene exit failed: {error}")
        process = None
        before = {path.relative_to(root): path.read_bytes()
                  for path in root.rglob("*") if path.is_file() and path.name != "editor.lock"}
        process, window = start(read_only=True)
        path_dialog("ctrl+o", "Content/Copy.scene")
        send("key", "--clearmodifiers", "ctrl+n", "ctrl+s", "ctrl+shift+s")
        request_window_close(window, env)
        _, error = driver.finish(15)
        if process.returncode != 0:
            raise RuntimeError(f"Read-only scene workflow exit failed: {error}")
        process = None
        after = {path.relative_to(root): path.read_bytes()
                 for path in root.rglob("*") if path.is_file() and path.name != "editor.lock"}
        if after != before:
            raise RuntimeError("Read-only scene workflow modified project files")
        # Broken startup metadata must fall back to Main and survive an explicit scene Save.
        malformed = b"schema=unsupported\nscene=Content/Exit.scene\n"
        metadata.write_bytes(malformed)
        exit_before = exit_scene.read_bytes()
        process, window = start()
        send("key", "--clearmodifiers", "ctrl+shift+n", "ctrl+s")
        wait_scene(lambda: main_scene.read_bytes().count(b"node ") ==
                   original.count(b"node ") + 1,
                   "Invalid startup settings did not fall back to Main", process)
        if metadata.read_bytes() != malformed or exit_scene.read_bytes() != exit_before:
            raise RuntimeError("Startup fallback overwrote broken settings or the remembered source")
        request_window_close(window, env)
        _, error = driver.finish(15)
        if process.returncode != 0 or "ignored scene startup settings" not in error:
            raise RuntimeError(f"Startup fallback did not report the preserved settings: {error}")
        process = None
        passed = True
        print("Native scene files, last-scene restart/fallback and camera persistence passed")
    finally:
        if process is not None:
            if driver is not None and driver.process is process:
                driver.abort()
            else:
                if process.poll() is None:
                    process.kill()
                process.communicate(timeout=5)
        xvfb.terminate()
        xvfb.communicate(timeout=5)
        if passed:
            shutil.rmtree(root, ignore_errors=True)
            shutil.rmtree(state, ignore_errors=True)
        else:
            print(f"Native scene file fixtures retained: {root} {state}", flush=True)


if __name__ == "__main__":
    main()
