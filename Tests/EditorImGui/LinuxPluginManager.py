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
from LinuxDisplayAcceptance import collect_output, launch, request_window_close, start_xvfb, wait_for_window
from LinuxReflectedInspector import rendered_pixels


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
    try:
        require(display is not None, "Xvfb unavailable")
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(scratch / "state"),
                   NEXORA_SIGNED_FIXTURE_EVENTS=str(events))
        window = None

        def send(*arguments):
            subprocess.run([args.xdotool, *arguments], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            time.sleep(.2)

        def click(x, y):
            send("mousemove", "--window", window, str(x), str(y))
            send("mousedown", "1")
            send("mouseup", "1")

        def key(chord):
            # Modifier down/key/down/up transitions must span software-rendered frames.
            send("key", "--clearmodifiers", "--delay", "100", chord)

        def type_at(x, y, text):
            click(x, y)
            key("ctrl+a")
            send("type", "--clearmodifiers", "--delay", "15", "--", text)

        def event_text():
            return events.read_text() if events.exists() else ""

        def wait(predicate, label):
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline:
                require(process.poll() is None, "Editor exited during " + label)
                if predicate():
                    return
                time.sleep(.05)
            raise RuntimeError("Actual native operation missing: " + label + "; events=" + event_text())

        def start(readonly=False):
            nonlocal process, window
            process = launch(args.editor, root, scratch / "recent", env, read_only=readonly)
            window = wait_for_window(args.xdotool, env)
            # Initial dock splits belong to the native host's 1280x720 startup layout.
            # Select the actual rendered source before resizing to the Extensions fixture size.
            send("windowsize", window, "1280", "720")
            send("windowfocus", "--sync", window)
            time.sleep(1.5)
            y = 199 if readonly else 182
            wait(lambda: any(b >= r + 25 and g >= r + 10
                             for r, g, b in rendered_pixels(display, window, (588, y - 4, 12, 8))),
                 "rendered source control")
            click(595, y)
            time.sleep(.8)
            send("windowsize", window, "1600", "1200")
            click(397, 29)
            key("ctrl+alt+e")

        def finish(readonly=False):
            nonlocal process
            request_window_close(window, env)
            output, error = collect_output(process, 15)
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
        type_at(180, 126, "known.vendor")
        type_at(180, 149, "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a")
        click(95, 172)
        click(42, 233)
        click(125, 233)
        click(100, 280)
        click(195, 114)
        type_at(180, 141, str(root / "input.nxpkg"))
        click(90, 167)
        time.sleep(.5)
        require(not installed.exists() and not event_text(), "Review installed or initialized code")
        click(115, 258)
        wait(installed.is_file, "verified installation")
        require(installed.read_bytes() == sources[root / "input.nxpkg"] and not event_text(),
                "Install autoenabled code or changed signed bytes")
        click(70, 235)
        wait(lambda: "1 register\n" in event_text(), "explicit enable")
        click(130, 235)
        wait(lambda: "1 stop\n" in event_text() and "1 unload\n" in event_text(), "cooperative disable")
        click(70, 235)
        wait(lambda: event_text().count("1 register\n") == 2, "explicit reload")
        click(85, 114)
        click(65, 326)
        wait(lambda: event_text().count("1 stop\n") == 2 and event_text().count("1 unload\n") == 2,
             "publisher revocation")
        finish()
        retained = installed.read_bytes()
        prior_events = event_text()
        # Relaunch never restores session trust and never executes installed packages implicitly.
        start(True)
        time.sleep(.5)
        require(event_text() == prior_events, "Read-only restart autoenabled native code")
        click(195, 114)
        click(70, 218)
        click(195, 218)
        finish(True)
        require(event_text() == prior_events and installed.read_bytes() == retained,
                "Read-only controls loaded or removed native package")
        print("Native signed install, enable, disable, revoke and read-only restart passed")
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.communicate(timeout=10)
        if xvfb.poll() is None:
            xvfb.terminate()
        xvfb.communicate(timeout=10)
        shutil.rmtree(scratch)


if __name__ == "__main__":
    main()
