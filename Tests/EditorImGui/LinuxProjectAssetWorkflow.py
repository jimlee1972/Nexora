#!/usr/bin/env python3
"""Create/import/search/move/reimport/Undo/reopen using real native pointer/key input."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time

from LinuxDisplayAcceptance import collect_output, request_window_close, start_xvfb, wait_for_window


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    for name in ("editor", "xvfb", "xdotool"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    scratch = Path(tempfile.mkdtemp(prefix="nexora-native-asset-workflow-"))
    root = scratch / "project"
    content = root / "Content"
    (content / "Moved").mkdir(parents=True)
    untouched = content / "Moved/Preserved.bin"
    untouched.write_bytes(b"\x00\xffunrelated asset\r\n")
    (content / "Unrelated.txt").write_bytes(b"source outside the query\n")
    original = b"v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"
    updated = original.replace(b"v 1 0 0", b"v 3 0 0")
    source = content / "WorkflowHero.obj"
    source.write_bytes(original)
    xvfb, display = start_xvfb(args.xvfb, "1280x900x24")
    process = None
    window = None
    passed = False
    captured = bytearray()
    latest = {}
    try:
        require(display, "Xvfb did not become ready")
        env = os.environ.copy()
        env.update(DISPLAY=display, XDG_STATE_HOME=str(scratch / "state"),
                   NEXORA_PROJECT_ASSET_WORKFLOW_EVIDENCE="1")

        def pump():
            if select.select([process.stderr], [], [], .05)[0]:
                chunk = os.read(process.stderr.fileno(), 65536)
                captured.extend(chunk)
                lines = captured.decode("utf-8", errors="replace").splitlines()
                # Only complete observations are parsed; never act on a partial coordinate.
                if not captured.endswith(b"\n"):
                    lines = lines[:-1]
                for line in reversed(lines):
                    if line.startswith("project asset workflow "):
                        latest.clear()
                        latest.update(re.findall(r"(\w+)=([^ ]+)", line))
                        break

        def wait(predicate, label, timeout=15):
            deadline = time.monotonic() + timeout
            while time.monotonic() < deadline:
                require(process.poll() is None, f"Editor exited: {label}")
                pump()
                if predicate(latest):
                    return dict(latest)
            raise RuntimeError(f"Missing {label}; latest={latest}; output={captured[-16000:]!r}")

        def send(*arguments):
            subprocess.run([args.xdotool, *map(str, arguments)], env=env, check=True,
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, timeout=10)
            time.sleep(.12)
            pump()

        def key(chord):
            parts = chord.split("+")
            for phase, keys in (("keydown", parts), ("keyup", reversed(parts))):
                for value in keys:
                    send(phase, value)

        def position(name):
            state = wait(lambda s: name in s, name)
            point, stable = state[name], time.monotonic()
            deadline = stable + 15
            while time.monotonic() < deadline:
                require(process.poll() is None, f"Editor exited positioning {name}")
                pump()
                if latest.get(name) != point:
                    point, stable = latest.get(name), time.monotonic()
                if point is not None and time.monotonic() - stable >= .4:
                    return [round(float(value)) for value in point.split(",")]
            raise RuntimeError(f"Unstable rendered position: {name}")

        def click(name, button=1):
            x, y = position(name)
            send("mousemove", "--window", window, x, y)
            send("mousedown", button)
            send("mouseup", button)

        def text(name, value):
            click(name)
            key("ctrl+a")
            if value:
                send("type", "--clearmodifiers", "--delay", "1", value)
            else:
                key("BackSpace")

        def start(label, direct=False, read_only=False):
            nonlocal process, window
            captured.clear()
            latest.clear()
            command = [args.editor, "--graphical", "--native-scene-preview",
                       "--recent-projects=" + str(scratch / (label + "-recent"))]
            if direct:
                command.append(f"--project={root}")
            if read_only:
                command.append("--read-only")
            process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True)
            window = wait_for_window(args.xdotool, env)
            send("windowfocus", "--sync", window)
            wait(lambda s: s.get("focused") == "1", "focused rendered UI")

        def finish():
            nonlocal process
            request_window_close(window, env)
            _, remaining = collect_output(process, 15)
            output = captured.decode("utf-8", errors="replace") + remaining
            print(output, flush=True)
            require(process.returncode == 0, "Native editor shutdown failed")
            require(not re.search(r"Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+", output),
                    "Vulkan validation reported errors")
            draws = re.search(r"scene_draws=(\d+)", output)
            require(draws and int(draws[1]) > 0 and
                    re.search(r"native mesh geometry: meshes=[1-9]\d* vertices=", output),
                    "Authored mesh was not submitted by the actual native Scene")
            process = None
            return output

        start("create")
        text("root", str(root))
        text("name", "Asset Workflow")
        key("ctrl+n")
        initial = wait(lambda s: s.get("project") == "1" and s.get("mesh") == "1" and
                       s.get("max_x") == "1", "actual asynchronous initial OBJ import")
        identity, initial_stamp = initial["hero"], initial["stamp"]
        require((root / "project.nexora").is_file(), "Create did not persist project")
        text("search", "WorkflowHero")
        wait(lambda s: s.get("visible") == "1", "search filters out unrelated root asset")
        click("hero_point")
        wait(lambda s: s.get("selected") == "1" and "addmesh" in s, "searched mesh inspection")
        click("addmesh")
        wait(lambda s: int(s.get("nodes", "0")) > int(initial.get("nodes", "0")),
             "authored mesh added from Content")
        text("search", "")
        wait(lambda s: s.get("visible") == "2", "cleared asset search")
        sx, sy = position("hero_point")
        tx, ty = position("moved")
        send("mousemove", "--window", window, sx, sy)
        send("mousedown", 1)
        for step in range(1, 7):
            send("mousemove", "--window", window, round(sx + (tx-sx)*step/6),
                 round(sy + (ty-sy)*step/6))
        send("mouseup", 1)
        wait(lambda s: s.get("moved_path") == "1" and s.get("can_undo") == "1",
             "actual folder drop")
        moved = content / "Moved/WorkflowHero.obj"
        require(moved.read_bytes() == original and not source.exists(), "Move lost source")
        # Navigate with the same folder widget; no model/test action commands are used.
        tx, ty = position("moved")
        send("mousemove", "--window", window, tx, ty, "click", "--repeat", "2", "--delay", "100", "1")
        wait(lambda s: "hero_point" in s, "double-click folder navigation")
        moved.write_bytes(updated)

        def reimport():
            click("hero_point", 3)
            click("reimport")

        reimport()
        imported = wait(lambda s: s.get("busy") == "0" and s.get("reimport_state") == "4" and
                        s.get("max_x") == "3", "real reimport publication")
        require(imported["hero"] == identity and imported["stamp"] != initial_stamp,
                "Reimport changed identity or retained stale artifact")
        new_stamp = imported["stamp"]
        moved.write_bytes(b"f invalid broken source\n")
        reimport()
        failed = wait(lambda s: s.get("busy") == "0" and s.get("reimport_state") == "6",
                      "failed reimport")
        require(failed["stamp"] == new_stamp and failed["max_x"] == "3",
                "Failed reimport lost last-good geometry")
        moved.write_bytes(updated)
        reimport()
        wait(lambda s: s.get("reimport_state") == "4" and s.get("busy") == "0", "reimport retry")
        click("undo")
        undone = wait(lambda s: s.get("moved_path") == "0" and s.get("can_undo") == "0",
                      "Content Undo after reimport")
        require(source.read_bytes() == updated and not moved.exists(), "Undo restored stale source")
        require(undone["hero"] == identity and undone["stamp"] == new_stamp and undone["max_x"] == "3",
                "Undo lost current imported asset")
        key("ctrl+s")
        scene = root / ".nexora/scenes/Main.scene"
        wait(lambda s: s.get("dirty") == "0" and scene.is_file(), "actual scene save")
        finish()
        retained = {p.relative_to(root): hashlib.sha256(p.read_bytes()).hexdigest()
                    for p in root.rglob("*") if p.is_file()}
        start("readonly", direct=True, read_only=True)
        reopened = wait(lambda s: s.get("project") == "1" and s.get("writable") == "0" and
                        s.get("mesh") == "1" and s.get("max_x") == "3", "read-only reopen/import")
        require(reopened["hero"] == identity and reopened["stamp"] == new_stamp and
                reopened.get("nodes") == undone.get("nodes") and
                reopened.get("dirty") == "0", "Reopen lost persistent asset/scene identity")
        click("hero_point", 3)
        click("reimport")  # Disabled production item must not enqueue work or mutate source.
        key("Escape")
        require(latest.get("busy") == "0" and latest.get("can_undo") == "0",
                "Read-only UI accepted a mutation")
        finish()
        require(retained == {p.relative_to(root): hashlib.sha256(p.read_bytes()).hexdigest()
                             for p in root.rglob("*") if p.is_file()},
                "Read-only reopen changed source, metadata or retained evidence")
        require(untouched.read_bytes() == b"\x00\xffunrelated asset\r\n", "Unrelated data changed")
        print("native asset workflow: create/import/search/inspect/move/reimport/failure/retry/Undo/save/reopen/read-only passed")
        passed = True
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.communicate(timeout=10)
        xvfb.terminate()
        xvfb.communicate(timeout=10)
        if passed:
            shutil.rmtree(scratch)
        else:
            print(f"Preserved asset-workflow evidence: {scratch}", flush=True)


if __name__ == "__main__":
    main()
