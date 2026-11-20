#!/usr/bin/env python3
"""Route real Zig compiler diagnostics through the native Build and Runtime Consoles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
import time
from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from NativeSceneInput import NativeSceneInput
from LinuxAuthoredPreview import screenshot


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    for name in ("editor", "zig", "xvfb", "xdotool"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--evidence", type=Path)
    args = parser.parse_args()
    scratch = args.evidence or Path(tempfile.mkdtemp(prefix="nexora-native-build-diagnostics-"))
    scratch.mkdir(parents=True, exist_ok=True)
    root = scratch / "project"
    (root / "Content").mkdir(parents=True)
    (root / "project.nexora").write_text("schema=2\nuuid=aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee\nname=Native Build Console\n")
    (root / "build.zig").write_text("const unexpected = ;\n")
    authored = [root / "project.nexora", root / "build.zig"]
    baseline = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in authored}
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
        env["NEXORA_BUILD_DIAGNOSTIC_TEST_EVIDENCE"] = "1"
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
            raise RuntimeError(f"{message}; native streams retained in {state}")

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
        deadline = time.monotonic() + 12
        driver.wait(lambda value: value['focused'] == '1' and value['keys'] == '000000000',
                    'Build console chord was not released', deadline)
        try:
            for key, index in [('Control_L', 0), ('Alt_L', 1)]:
                driver.raw('keydown', key, deadline=deadline, minimum_wait=driver.phase_wait)
                driver.wait(lambda value, index=index: value['keys'][index] == '1',
                            'Build console modifier was not observed', deadline)
            driver.raw('keydown', 'b', deadline=deadline, minimum_wait=driver.phase_wait)
            wait_for(lambda: 'control0' in controls(),
                     'Actual Build console did not open from one native chord', deadline)
        finally:
            for key in ['b', 'Alt_L', 'Control_L']:
                driver.raw('keyup', key, deadline=deadline, minimum_wait=driver.phase_wait)
        driver.wait(lambda value: value['keys'] == '000000000',
                    'Build console chord release was not observed', deadline)
        executable = str(Path(args.zig).resolve())
        replace_field(0, 'executable_hex', executable)
        deadline = time.monotonic() + 12
        control_click(2, deadline)
        wait_for(lambda: controls().get('argv_count') == '1' and 'control6' in controls(),
                 'Build argument row was not added exactly once', deadline)
        initial_cwd = controls().get('cwd_hex')
        require(initial_cwd is not None, 'Actual Build working directory unavailable')
        replace_field(6, 'argv0_hex', 'build')
        run('build', r"build process phase=5 operation=1 exit=2 artifact_verified=0")
        # Preserve the actual rendered failing command and Console rows for manual inspection.
        if args.evidence:
            driver.drain(.3)
            screenshot(display, int(window), scratch / "build-failed.png")
        # The docked Console remains live; no helper injects process state.
        # Ctrl+Alt+B is the ordinary visibility command; release text ownership first.
        driver.raw('key', 'Escape')
        deadline = time.monotonic() + 12
        driver.wait(lambda value: value['text_input'] == '0',
                    'Build text ownership did not release', deadline)
        for key in ['Control_L', 'Alt_L', 'b']:
            driver.raw('keydown', key, deadline=deadline, minimum_wait=driver.phase_wait)
        wait_for(lambda: 'control0' not in controls(),
                 'Build window did not close from the ordinary chord', deadline)
        for key in ['b', 'Alt_L', 'Control_L']:
            driver.raw('keyup', key, deadline=deadline, minimum_wait=driver.phase_wait)
        if args.evidence:
            # Select the default dock's real Console tab; startup leaves Content selected.
            click(330, 550)
            driver.drain(.3)
            screenshot(display, int(window), scratch / "console-raw.png")
            click(380, 577)
            driver.wait(lambda value: value['text_input'] == '1',
                        'Console filter did not receive native text focus')
            driver.raw('type', '--clearmodifiers', '--delay', '20', '--', 'BuildDiagnostic')
            driver.drain(.3)
            screenshot(display, int(window), scratch / "console-diagnostic.png")
        require((root / ".nexora/scenes/Main.scene").read_bytes() == scene_before,
                "Compiler execution changed saved authoring")
        request_window_close(window, env)
        driver.finish(15)
        prefix = captured.decode("utf-8", errors="replace")
        print(prefix, end="", flush=True)
        require(process.returncode == 0, "Native compiler diagnostics failed graceful close")
        records = []
        for category, severity, source, message in re.findall(
                r"build console record category=(\w+) severity=(\d+) source_hex=([0-9a-f]*) message_hex=([0-9a-f]*)", prefix):
            records.append(dict(category=category, severity=int(severity),
                                source=bytes.fromhex(source).decode(),
                                message=bytes.fromhex(message).decode()))
        diagnostics = [r for r in records if r['category'] == 'BuildDiagnostic']
        require(len(diagnostics) == 1 and diagnostics[0]['severity'] == 3 and
                re.fullmatch(r'BuildProcess scope=\d+ operation=1 \| .*build\.zig:1:20', diagnostics[0]['source']) and
                diagnostics[0]['message'] == "expected expression, found ';'",
                "Actual compiler Error, owning location or message did not reach Runtime Console")
        raw = ''.join(r['message'] for r in records if r['category'] == 'BuildOutput')
        require("build.zig:1:20: error: expected expression, found ';'" in raw and
                r'\x0a' in raw and all(r['severity'] == 1 for r in records if r['category'] == 'BuildOutput'),
                "Original escaped Info output did not survive derived diagnostics")
        require(not re.search(r"Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+", prefix),
                "Native compiler diagnostics reported Vulkan validation errors")
        require((root / ".nexora/scenes/Main.scene").read_bytes() == scene_before and
                baseline == {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in authored},
                "Compiler run or close changed authored sources")
        if args.evidence:
            (scratch / 'acceptance.json').write_text(json.dumps(dict(
                records=records, authored_sha256=baseline,
                scene_sha256=hashlib.sha256(scene_before).hexdigest(),
                editor_exit=process.returncode, compiler_exit=2, artifact_verified=False), indent=2) + '\n')
        print("Real Zig compiler Error/location, original raw Info, failed process and authored bytes verified")
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
        if passed and not args.evidence:
            shutil.rmtree(scratch)
        else:
            print(f"Native build console fixtures retained: {scratch}", flush=True)


if __name__ == "__main__":
    main()
