#!/usr/bin/env python3
"""Exercise the actual native build console against a real bounded child program."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from NativeSceneInput import NativeSceneInput


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
    state = scratch / "state"
    state.mkdir()
    # Create a persisted camera scene through the ordinary Editor save action below.
    xvfb, display = start_xvfb(args.xvfb, "1280x900x24")
    process = None
    window = None
    driver = None
    passed = False
    try:
        require(display is not None, "Xvfb did not become ready")
        env = os.environ.copy()
        env["DISPLAY"] = display
        env["XDG_STATE_HOME"] = str(state)
        env["MESA_SHADER_CACHE_DIR"] = str(scratch / "shader-cache")
        env["NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY"] = "1"
        process = launch(args.editor, root, scratch / "recent", env, binary=True)
        driver = NativeSceneInput(process, args.xdotool, env, "0", state, 1,
                                  minimum_wait=.15, phase_wait=.06, ack_pointer=True)
        captured = driver.stderr
        window = wait_for_window(args.xdotool, env)
        driver.window = str(window)
        driver.raw("windowfocus", "--sync", window)

        def tool(*arguments):
            driver.raw(*arguments)

        def send(*keys, deadline=None):
            for key in keys:
                driver.chord(key, deadline=deadline)

        def click(x, y):
            driver.click((x, y), hold=0)

        def wait_for(predicate, message, deadline=None):
            deadline = time.monotonic() + 12 if deadline is None else deadline
            while time.monotonic() < deadline:
                require(process.poll() is None, "Editor exited during build controls")
                if predicate() and time.monotonic() < deadline:
                    return
                driver.drain(min(.1, max(0, deadline - time.monotonic())))
            raise RuntimeError(f"{message}: {captured!r}")

        def wait_log(pattern, start=0, deadline=None):
            wait_for(lambda: re.search(pattern + r"\r?\n", captured[start:].decode("utf-8", errors="replace")),
                     f"Missing build diagnostic {pattern}", deadline)

        def controls():
            records = re.findall(rb'^build console ui ([^\n]*)\n', captured, re.M)
            return dict(part.split('=', 1) for part in records[-1].decode().split()) if records else {}

        def control_click(index, deadline):
            name = f'control{index}'
            wait_for(lambda: name in controls(), f'Actual Build control unavailable: {name}', deadline)
            point = tuple(map(float, controls()[name].split(',')))
            driver.pointer_move(*map(round, point), deadline=deadline)
            driver.pointer_button(1, True, deadline)
            driver.pointer_button(1, False, deadline)

        def replace_field(index, field, text):
            deadline = time.monotonic() + 12
            control_click(index, deadline)
            driver.wait(lambda value: value['text_input'] == '1',
                        'Actual Build input did not receive focus', deadline)
            send('ctrl+a', deadline=deadline)
            driver.raw('type', '--clearmodifiers', '--delay', '1' if index == 0 else '20',
                       '--', text, deadline=deadline)
            wait_for(lambda: controls().get(field) == text.encode().hex(),
                     f'Actual Build field did not commit {field}', deadline)

        def run(expected_argument, pattern):
            require(controls().get('executable_hex') == executable.encode().hex() and
                    controls().get('cwd_hex') == initial_cwd and controls().get('argv_count') == '1' and
                    controls().get('argv0_hex') == expected_argument.encode().hex(),
                    'Actual Build command differs from the exact requested argv')
            # The one real gesture and its lifecycle observation share the original 12 seconds.
            deadline = time.monotonic() + 12
            send('ctrl+Return', deadline=deadline)
            wait_log(pattern, deadline=deadline)

        time.sleep(.6)
        click(500, 220)
        send("ctrl+s")
        deadline = time.monotonic() + 10
        while not (root / ".nexora/scenes/Main.scene").exists() and time.monotonic() < deadline:
            driver.drain(min(.1, max(0, deadline - time.monotonic())))
        require((root / ".nexora/scenes/Main.scene").exists(), "Native authoring save failed")
        scene_before = (root / ".nexora/scenes/Main.scene").read_bytes()
        # B is not part of the existing copied key set; observe the opened panel itself.
        tool('key', '--clearmodifiers', '--delay', '60', 'ctrl+alt+b')
        executable = str(Path(args.fixture).resolve())
        replace_field(0, 'executable_hex', executable)
        deadline = time.monotonic() + 12
        control_click(2, deadline)
        wait_for(lambda: controls().get('argv_count') == '1' and 'control6' in controls(),
                 'Build argument row was not added exactly once', deadline)
        initial_cwd = controls().get('cwd_hex')
        require(initial_cwd is not None, 'Actual Build working directory unavailable')
        replace_field(6, 'argv0_hex', '--sleep')
        run('--sleep', r"build process phase=2 operation=1 exit=unavailable artifact_verified=0")
        deadline = time.monotonic() + 12
        send("ctrl+shift+Return", deadline=deadline)
        wait_log(r"build process phase=6 operation=1 exit=unavailable artifact_verified=0", deadline=deadline)
        # Replace only argv; execute a real failure and then a zero exit. Neither is artifact success.
        replace_field(6, 'argv0_hex', '--fail')
        run('--fail', r"build process phase=5 operation=2 exit=7 artifact_verified=0")
        replace_field(6, 'argv0_hex', '--args')
        run('--args', r"build process phase=4 operation=3 exit=0 artifact_verified=0")
        require((root / ".nexora/scenes/Main.scene").read_bytes() == scene_before,
                "Build process controls changed saved authoring")
        replace_field(6, 'argv0_hex', '--sleep')
        run('--sleep', r"build process phase=2 operation=4 exit=unavailable artifact_verified=0")
        close_started = time.monotonic()
        with (state / 'input.jsonl').open('a', encoding='utf-8') as log:
            log.write(json.dumps(dict(time=close_started, launch=1, input=['window_close'])) + '\n')
        request_window_close(window, env)
        driver.finish(15)
        require(time.monotonic() - close_started < 3.5,
                "Closing did not cancel/drain the actual running build promptly")
        prefix = captured.decode("utf-8", errors="replace")
        print(prefix, end="", flush=True)
        require(process.returncode == 0, "Native build console failed graceful close")
        match = re.search(r"console build evidence: records=(\d+) consumed_raw_bytes=(\d+) "
                          r"missing_raw_bytes=(\d+) retained=(\d+)", prefix)
        require(match is not None and all(int(match[i]) > 0 for i in (1, 2, 4)),
                "Actual process output did not reach owning Runtime Console records")
        require(not re.search(r"Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+", prefix),
                "Native build console reported Vulkan validation errors")
        require((root / ".nexora/scenes/Main.scene").read_bytes() == scene_before,
                "Closing build console changed authoring source")
        print("Native build console run/cancel/fail/zero-exit and authoring preservation passed")
        passed = True
    finally:
        if driver is not None:
            driver.abort()
        elif process is not None:
            if process.poll() is None:
                process.kill()
            out, error = process.communicate(timeout=5)
            (state / 'editor-1.stdout.log').write_bytes(out)
            (state / 'editor-1.stderr.log').write_bytes(error)
        xvfb.terminate()
        xvfb.communicate(timeout=10)
        if passed:
            shutil.rmtree(scratch)
        else:
            print(f"Native build console fixtures retained: {scratch}", flush=True)


if __name__ == "__main__":
    main()
