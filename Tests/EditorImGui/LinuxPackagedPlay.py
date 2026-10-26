"""Real Editor configured startup and SDK A/B/A pixels, with saved sources conserved."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Runtime"))
from NativeProjectPlayerLinux import pixels, require, start_xvfb, request_window_close


from PackagedPlayProject import write_fixture


def graphical_build(args, root, evidence, environment):
    errors = evidence / "build.stderr.log"
    editor = None
    try:
        with errors.open("wb") as stderr, (evidence / "build.stdout.log").open("wb") as stdout:
            editor = subprocess.Popen([args.editor, "--project=" + str(root), "--graphical",
                "--recent-projects=" + str(root / ".nexora/recent.json")], cwd=root,
                env=dict(environment, NEXORA_PROJECT_GAME_BUILD_EVIDENCE="1"),
                stdout=stdout, stderr=stderr)
            deadline = time.monotonic() + 15
            window = None
            while time.monotonic() < deadline and window is None:
                found = subprocess.run([args.xdotool, "search", "--name", "^Nexora Editor$"],
                    env=environment, capture_output=True, text=True, timeout=3)
                if found.returncode == 0 and found.stdout.strip():
                    window = int(found.stdout.splitlines()[0])
                else:
                    require(editor.poll() is None, errors.read_text(errors="replace"))
                    time.sleep(.05)
            require(window, "graphical build window missing")
            subprocess.run([args.xdotool, "windowfocus", "--sync", str(window)],
                           env=environment, check=True, timeout=3)


            def observation():
                records = re.findall(r"^game build ui (.*)$", errors.read_text(errors="replace"), re.M)
                return dict(part.split("=", 1) for part in records[-1].split()) if records else {}

            def click(index):
                deadline = time.monotonic() + 8
                key = "p" + str(index)
                while time.monotonic() < deadline:
                    state = observation()
                    if key in state:
                        # Wait for the same submitted position after popup sizing/layout.
                        point = state[key]
                        time.sleep(.2)
                        if observation().get(key) == point:
                            x, y = (round(float(value)) for value in point.split(","))
                            require(0 <= x < 1280 and 0 <= y < 720, (key, point))
                            subprocess.run([args.xdotool, "mousemove", "--window", str(window),
                                            str(x), str(y)], env=environment, check=True, timeout=3)
                            time.sleep(.1)
                            with (evidence / "build-input.jsonl").open("a") as inputs:
                                inputs.write(json.dumps({"control": index, "point": [x, y],
                                                         "time": time.monotonic()}) + "\n")
                            subprocess.run([args.xdotool, "click", "1"], env=environment,
                                           check=True, timeout=3)
                            time.sleep(.2)
                            return
                    require(editor.poll() is None, errors.read_text(errors="replace"))
                    time.sleep(.05)
                raise RuntimeError("graphical build control missing: " + key + str(observation()))

            click(0)   # Actual Build menu, then its actual Settings item.
            click(1)
            click(11)  # Include the authored gameplay script, using its default relative manifest.
            click(13)  # Actual Build Game control; worker and owner publisher do all file operations.
            output = root / "Build/Game"
            deadline = time.monotonic() + 120
            while time.monotonic() < deadline:
                if (output / "Game.inventory").is_file() and observation().get("busy") == "0":
                    break
                require(editor.poll() is None, errors.read_text(errors="replace"))
                time.sleep(.1)
            require((output / "Game.inventory").is_file() and (output / "libGame.so").is_file(),
                    errors.read_text(errors="replace"))
            require("mode=script-project" in (output / "Build.sources").read_text(),
                    "graphical build did not compile the selected script project")
            verification = subprocess.run([str(output / "Game"), "--verify-game-module",
                                           str(output / "libGame.so")], cwd=root, env=environment,
                                          capture_output=True, timeout=15)
            require(verification.returncode == 0, verification.stderr)
            catalog = subprocess.run([str(output / "Game"), "--verify-game-package",
                                      str(output / "Game.nxgame")], cwd=root, env=environment,
                                     capture_output=True, timeout=15)
            require(catalog.returncode == 0, catalog.stderr)
            data = json.loads(catalog.stdout)
            require(data["scene_count"] == 2 and data["asset_count"] == 4 and
                    data["start_scene"] == "00000000-0000-0001-0000-000000000002", data)
            request_window_close(str(window), environment)
            editor.wait(timeout=15)
        require(editor.returncode == 0, errors.read_text(errors="replace"))
        return {"exit": editor.returncode, "inventory_sha256": hashlib.sha256(
            (output / "Game.inventory").read_bytes()).hexdigest(), "script_abi_verified": True}
    finally:
        if editor is not None and editor.poll() is None:
            editor.terminate()
            try:
                editor.wait(timeout=5)
            except subprocess.TimeoutExpired:
                editor.kill()
                editor.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser()
    for name in ("editor", "module", "script-source", "xvfb", "xdotool", "evidence-dir"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    evidence = Path(args.evidence_dir).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="nexora-editor-packaged-play-"))
    write_fixture(root)
    (root / "Scripts").mkdir()
    shutil.copyfile(args.script_source, root / "Scripts/game.zig")
    (root / "Scripts/project.json").write_text(
        '{"schema":1,"module_name":"EditorBuiltGame","script":"game.zig"}')
    module = root / Path(args.module).name
    shutil.copyfile(args.module, module)
    sources = [path for path in root.rglob("*") if path.is_file() and ".nexora" not in path.parts]
    baseline = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest() for path in sources}
    server, display = start_xvfb(args.xvfb, "1280x720x24")
    editor = None
    passed = False
    try:
        require(display, "Xvfb unavailable")
        environment = dict(os.environ, DISPLAY=display)
        errors = evidence / "stderr.log"
        with errors.open("wb") as stderr, (evidence / "stdout.log").open("wb") as stdout:
            editor = subprocess.Popen([args.editor, "--project=" + str(root), "--graphical",
                "--native-scene-preview", "--gameplay-library=" + module.name, "--play",
                "--recent-projects=" + str(root / ".nexora/recent.json")], cwd=root,
                env=environment, stdout=stdout, stderr=stderr)
            window = None
            observed = []
            deadline = time.monotonic() + 60
            columns = len(range(8, 1280 - 8, 8))
            while time.monotonic() < deadline and len(observed) < 3:
                require(editor.poll() is None, errors.read_text(errors="replace"))
                if window is None:
                    found = subprocess.run([args.xdotool, "search", "--name", "^Nexora Editor$"],
                        env=environment, capture_output=True, text=True, timeout=3)
                    if found.returncode == 0 and found.stdout.strip():
                        window = int(found.stdout.splitlines()[0])
                    else:
                        time.sleep(0.05)
                        continue
                log = errors.read_text(errors="replace")
                views = re.findall(r"native game viewport: (\d+) (\d+) (\d+) (\d+)", log)
                if not views:
                    time.sleep(0.05)
                    continue
                x, y, width, height = map(int, views[-1])
                colors = pixels(display, window, 1280, 720)
                red = [(8 + (index % columns) * 8, 8 + (index // columns) * 8)
                       for index, (r, g, b) in enumerate(colors)
                       if r > g * 2 + 20 and r > b * 2 + 20]
                red = [px for px, py in red if x + 8 < px < x + width - 8 and y + 8 < py < y + height - 8]
                if len(red) >= 12:
                    center = sum(red) / len(red)
                    side = "left" if center < x + width / 2 else "right"
                    if side == ("left" if len(observed) % 2 == 0 else "right"):
                        observed.append({"side": side, "samples": len(red), "x": center, "viewport": views[-1]})
                time.sleep(0.05)
            require(len(observed) == 3, (observed, errors.read_text(errors="replace")))
            request_window_close(str(window), environment)
            editor.wait(timeout=15)
        log = errors.read_text(errors="replace")
        require(editor.returncode == 0 and log.count("packaged play scene committed:") == 3 and
                "scene_draws=" in log, log)
        require(all(hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
                    for name, digest in baseline.items()), "Play modified saved project sources")
        (evidence / "play-pixels.json").write_text(json.dumps({
            "sequence": observed, "exit": editor.returncode, "sources": baseline,
            "saved_sources_unchanged": True}, indent=2))
        build = graphical_build(args, root, evidence, environment)
        require(all(hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
                    for name, digest in baseline.items()), "GUI Build modified authored sources")
        receipt = {"sequence": observed, "graphical_script_build": build,
                   "sources": baseline, "exit": editor.returncode,
                   "scope": "Editor native Vulkan/Xvfb configured startup, same Play command and SDK transitions"}
        (evidence / "receipt.json").write_text(json.dumps(receipt, indent=2))
        print(json.dumps(receipt))
        passed = True
    finally:
        if editor is not None and editor.poll() is None:
            editor.terminate()
            try:
                editor.wait(timeout=5)
            except subprocess.TimeoutExpired:
                editor.kill()
                editor.wait(timeout=5)
        server.terminate()
        server.wait(timeout=5)
        if passed:
            shutil.rmtree(root)
        else:
            shutil.copytree(root, evidence / "failed-project", dirs_exist_ok=True)
            print("Failed fixture retained:", root)


if __name__ == "__main__":
    main()
