"""Observe actual packaged-scene pixels and queued script transitions on Vulkan/Xvfb."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from NativeProjectPlayerLinux import pixels, require, start_xvfb, request_window_close


def main():
    parser = argparse.ArgumentParser()
    for name in ("fixture", "player", "module", "visual-module", "stress-module", "xvfb", "xdotool"):
        parser.add_argument("--" + name, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="nexora-native-game-") as temporary:
        root = Path(temporary).resolve()
        package = root / "遊戲 multi scene.nxgame"
        subprocess.run([args.fixture, "--write-game", str(package)], check=True, timeout=15)
        baseline = package.read_bytes()
        visual = root / Path(args.visual_module).name
        shutil.copyfile(args.visual_module, visual)
        server, display = start_xvfb(args.xvfb, "1280x720x24")
        player = None
        try:
            require(display, "Xvfb unavailable")
            environment = dict(os.environ, DISPLAY=display)
            error_file = root / "visual.stderr.log"
            with error_file.open("wb") as errors:
                player = subprocess.Popen([args.player, "--run-game-package", str(package),
                    "--backend=vulkan", "--gameplay-module=" + str(visual)], cwd=root,
                    env=environment, stdout=subprocess.PIPE, stderr=errors)
                window = None
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline:
                    found = subprocess.run([args.xdotool, "search", "--name", "^Nexora Game$"],
                        env=environment, capture_output=True, text=True, timeout=3)
                    if found.returncode == 0 and found.stdout.strip():
                        window = int(found.stdout.splitlines()[0])
                        break
                    require(player.poll() is None, error_file.read_text(errors="replace"))
                    time.sleep(0.05)
                require(window, "game window missing")
                # The two scenes share one red mesh/material, but put it on opposite camera sides.
                observed = []
                deadline = time.monotonic() + 60
                columns = len(range(8, 960 - 8, 8))
                while len(observed) < 3 and time.monotonic() < deadline:
                    require(player.poll() is None, error_file.read_text(errors="replace"))
                    colors = pixels(display, window, 960, 640)
                    red = [8 + (index % columns) * 8 for index, (r, g, b) in enumerate(colors)
                           if r > g * 2 + 20 and r > b * 2 + 20]
                    if len(red) >= 16:
                        center = sum(red) / len(red)
                        side = "left" if center < 480 else "right"
                        if side == ("left" if len(observed) % 2 == 0 else "right"):
                            observed.append({"side": side, "red_samples": len(red), "x": center})
                    time.sleep(0.05)
                require(len(observed) == 3, f"Level01/Title/Level01 pixels missing: {observed}; " +
                        error_file.read_text(errors="replace"))
                request_window_close(str(window), environment)
                output, _ = player.communicate(timeout=10)
            require(player.returncode == 0, error_file.read_text(errors="replace"))
            report = json.loads(output)
            require(report["status"] == "RENDERED_GAME_PACKAGE" and report["scene_commits"] == 3
                    and report["gameplay_loaded"] and report["drained"], report)
            print("Native Vulkan scene pixel sequence:", json.dumps(observed))
            for name, writer, module, frames, commits, rejected in (
                ("failure", "--write-game-bad-camera", args.module, 100, 2, 1),
                ("stress", "--write-game", args.stress_module, 300, 121, 0),
                ("empty", "--write-game-empty", None, 4, 1, 0),
            ):
                candidate = root / (name + ".nxgame")
                subprocess.run([args.fixture, writer, str(candidate)], check=True, timeout=15)
                command = [args.player, "--run-game-package", str(candidate), "--backend=vulkan",
                           "--frames=" + str(frames)]
                if module:
                    command.append("--gameplay-module=" + module)
                result = subprocess.run(command, cwd=root, env=environment,
                                        capture_output=True, text=True, timeout=30)
                require(result.returncode == 0, (name, result.returncode, result.stderr))
                data = json.loads(result.stdout)
                require(data["scene_commits"] == commits and data["rejected_requests"] == rejected
                        and data["presented_frames"] == frames and data["drained"]
                        and data["scene_records"] == 1 and data["instances"] == 1
                        and data["stored_entities"] == (0 if name == "empty" else 3), data)
                if name == "failure":
                    require("rejected with startup/camera retained" in result.stderr, result.stderr)
                if name != "empty" and data["geometry_residency_available"]:
                    require(data["geometry_uploads"] == 1 and data["geometry_hits"] == frames - 1, data)
                print(name, result.stdout.strip())
            require(package.read_bytes() == baseline and not list(root.glob("*.scene")),
                    "runtime modified package or required authored scene sources")
        finally:
            if player is not None and player.poll() is None:
                player.terminate()
                try:
                    player.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    player.kill()
                    player.wait(timeout=5)
            server.terminate()
            server.wait(timeout=5)


if __name__ == "__main__":
    main()
