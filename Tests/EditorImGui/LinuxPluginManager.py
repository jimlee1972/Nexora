#!/usr/bin/env python3
"""Install and control actual signed native code through the graphical owner."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time
from LinuxDisplayAcceptance import launch, request_window_close, start_xvfb, wait_for_window
from LinuxReflectedInspector import rendered_pixels
from LinuxExtensionControls import ExtensionControls
from NativeSceneInput import NativeSceneInput


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    for name in ("editor", "fixture", "module", "xvfb", "xdotool"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix="nexora-native-plugin-manager-"))
    root = scratch / "project"
    subprocess.run([args.fixture, "--write-native-fixture", str(root), args.module], check=True)
    source_paths = [root / "project.nexora", root / ".nexora/scenes/Main.scene", root / "input.nxpkg"]
    sources = {path: path.read_bytes() for path in source_paths}
    installed = root / ".nexora/extensions/13-sample.plugin-1.0.0.nxpkg"
    events = scratch / "events"
    xvfb, display = start_xvfb(args.xvfb, "1600x1200x24")
    process = None
    controls = None
    driver = None
    launch_number = 0
    passed = False
    try:
        require(display is not None, "Xvfb unavailable")
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(scratch / "state"),
                   NEXORA_SIGNED_FIXTURE_EVENTS=str(events), NEXORA_EXTENSION_UI_TEST_GEOMETRY="1",
                   NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY="1", NEXORA_SOURCE_MARKER_TEST_GEOMETRY="1")
        window = None

        def send(*arguments):
            driver.send(*arguments)

        def click(x, y):
            send("mousemove", "--window", window, str(x), str(y))
            send("mousedown", "1")
            send("mouseup", "1")

        def key(chord):
            # Known chords acknowledge processed GUI phases. Ctrl+Alt+E keeps the original
            # 100 ms native gesture; the shared observer does not claim an E acknowledgement.
            send("key", "--clearmodifiers", "--delay", "100", chord)

        def event_text():
            return events.read_text() if events.exists() else ""

        def source_observation():
            records = re.findall(rb'^source marker ([^\n]*)\n', driver.stderr, re.M)
            return dict(part.split('=', 1) for part in records[-1].decode().split()) if records else {}

        def wait(predicate, label):
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline:
                require(process.poll() is None, "Editor exited during " + label)
                driver.drain(.05)
                if time.monotonic() >= deadline:
                    break
                if predicate():
                    return
            raise RuntimeError("Actual native operation missing: " + label + "; events=" + event_text() +
                               "; source=" + str(source_observation()))

        def start(readonly=False):
            nonlocal process, window, controls, driver, launch_number
            process = launch(args.editor, root, scratch / "recent", env, read_only=readonly, binary=True)
            launch_number += 1
            driver = NativeSceneInput(process, args.xdotool, env, "0", scratch, launch_number)
            window = wait_for_window(args.xdotool, env)
            driver.window = str(window)
            # Initial dock splits belong to the native host's 1280x720 startup layout.
            # Select the actual rendered source before resizing to the Extensions fixture size.
            send("windowsize", window, "1280", "720")
            send("windowfocus", "--sync", window)
            time.sleep(1.5)
            marker = {}

            def rendered_source():
                current = source_observation()
                if current.get('ready') != '1' or driver.observation().get('focused') != '1':
                    return False
                x, y = map(round, map(float, current['point'].split(',')))
                if not any(b >= r + 25 and g >= r + 10
                           for r, g, b in rendered_pixels(display, window, (x - 5, y - 4, 10, 10))):
                    return False
                marker.update(current)
                marker['position'] = (x, y)
                return True

            wait(rendered_source, "rendered source control")
            driver.click(marker['position'])
            wait(lambda: source_observation().get('id') == marker['id'] and
                 source_observation().get('selected') == '1' and
                 source_observation().get('selection_count') == '1', "selected source control")
            time.sleep(.8)
            send("windowsize", window, "1600", "1200")
            click(397, 29)
            key("ctrl+alt+e")
            controls = ExtensionControls(process, send, window, captured=driver.stderr, pump=driver.drain)

        def finish(readonly=False):
            nonlocal process
            request_window_close(window, env)
            output, error = driver.finish(15)
            output += error
            require(process.returncode == 0, "Native close failed")
            require(("access=read-only" if readonly else "access=read-write") in output,
                    "Native launch did not activate requested project access")
            require("scene_selected=1" in output, "Native fixture scene did not activate/select")
            require(not re.search(r"Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+", output),
                    "Native extension flow reported Vulkan errors")
            process = None
            for path, original in sources.items():
                require(path.read_bytes() == original, "Extension operation modified source " + path.name)

        start()
        controls.type("publisher", "known.vendor")
        controls.type("public-key", "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a")
        controls.action("trust-key", 0)
        controls.click("permission-0")
        controls.click("permission-1")
        controls.action("apply-permissions", 2)
        controls.click("packages-tab")
        controls.type("package-path", str(root / "input.nxpkg"))
        controls.action("review", 3)
        time.sleep(.5)
        require(not installed.exists() and not event_text(), "Review installed or initialized code")
        controls.action("install", 4)
        wait(installed.is_file, "verified installation")
        require(installed.read_bytes() == sources[root / "input.nxpkg"] and not event_text(),
                "Install autoenabled code or changed signed bytes")
        controls.action("sample.plugin/1.0.0/enable", 6)
        wait(lambda: "1 register\n" in event_text(), "explicit enable")
        controls.action("sample.plugin/1.0.0/disable", 7)
        wait(lambda: "1 stop\n" in event_text() and "1 unload\n" in event_text(), "cooperative disable")
        controls.action("sample.plugin/1.0.0/enable", 6)
        wait(lambda: event_text().count("1 register\n") == 2, "explicit reload")
        controls.click("trust-tab")
        controls.action("known.vendor/revoke", 1)
        wait(lambda: event_text().count("1 stop\n") == 2 and event_text().count("1 unload\n") == 2,
             "publisher revocation")
        finish()
        retained = installed.read_bytes()
        prior_events = event_text()
        # Relaunch never restores session trust and never executes installed packages implicitly.
        start(True)
        time.sleep(.5)
        require(event_text() == prior_events, "Read-only restart autoenabled native code")
        controls.click("packages-tab")
        controls.click("sample.plugin/1.0.0/enable")
        controls.click("sample.plugin/1.0.0/remove")
        finish(True)
        require(event_text() == prior_events and installed.read_bytes() == retained,
                "Read-only controls loaded or removed native package")
        passed = True
        print("Native signed install, enable, disable, revoke and read-only restart passed")
    finally:
        if process is not None:
            if driver is not None and driver.process is process:
                driver.abort()
            else:
                if process.poll() is None:
                    process.kill()
                process.communicate(timeout=10)
        if xvfb.poll() is None:
            xvfb.terminate()
        xvfb.communicate(timeout=10)
        if passed:
            shutil.rmtree(scratch)
        else:
            print(f"Native plugin manager fixtures retained: {scratch}", flush=True)


if __name__ == "__main__":
    main()
