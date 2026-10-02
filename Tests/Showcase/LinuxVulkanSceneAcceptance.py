#!/usr/bin/env python3
"""Run native scene pixel contracts or the Rendering Room on an isolated Xvfb server."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

from LinuxVirtualDisplaySmoke import start_xvfb, unavailable


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--native-test")
    target.add_argument("--showcase")
    parser.add_argument("--interactive", action="store_true")
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    output = args.evidence_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    for name in ("launch.json", "stdout.log", "stderr.log", "scene.json", "scene.ppm"):
        (output / name).unlink(missing_ok=True)
    xvfb = shutil.which("Xvfb")
    if not xvfb:
        return unavailable("Xvfb is not installed")
    if args.interactive and not shutil.which("xdotool"):
        return unavailable("xdotool is not installed for keyboard acceptance")
    frames = 600 if args.interactive else 12
    server, display = start_xvfb(xvfb, "1280x720x24")
    try:
        if display is None:
            server.terminate()
            _, errors = server.communicate(timeout=3)
            return unavailable(f"Xvfb failed to start: {errors.strip()}")
        command = ([args.native_test, str(output / "scene.ppm")] if args.native_test else [
            args.showcase, "--mode=interactive", "--scene=rendering", "--backend=vulkan",
            f"--frames={frames}", "--resize=960x540", "--no-reload", "--gameplay-module=static",
            f"--report={output / 'scene.json'}",
        ])
        environment = os.environ.copy()
        environment["DISPLAY"] = display
        if args.interactive:
            process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                       stderr=subprocess.PIPE, text=True)
            try:
                window = subprocess.check_output([
                    "xdotool", "search", "--sync", "--onlyvisible", "--name", "^Nexora Showcase$"
                ], env=environment, text=True, timeout=5).splitlines()[0]
                subprocess.run(["xdotool", "keydown", "--window", window, "d"],
                               env=environment, check=True, capture_output=True, timeout=3)
                time.sleep(0.03)
                subprocess.run(["xdotool", "keyup", "--window", window, "d"],
                               env=environment, check=True, capture_output=True, timeout=3)
                stdout, stderr = process.communicate(timeout=30)
                run = subprocess.CompletedProcess(command, process.returncode, stdout, stderr)
            finally:
                if process.poll() is None:
                    process.kill()
                    process.communicate(timeout=3)
        else:
            run = subprocess.run(command, env=environment, capture_output=True, text=True, timeout=30)
        (output / "stdout.log").write_text(run.stdout, encoding="utf-8")
        (output / "stderr.log").write_text(run.stderr, encoding="utf-8")
        launch = {"command": command, "exit_code": run.returncode, "acceptance_passed": False,
                  "scope": "Linux/X11/Vulkan GPU scene on virtual display",
                  "physical_display_verified": False,
                  "validation_layer_requested": "VK_LAYER_KHRONOS_validation" in
                      environment.get("VK_INSTANCE_LAYERS", "")}
        error = ""
        if run.returncode:
            error = f"native scene process failed: {run.returncode}"
        elif args.showcase:
            try:
                report = json.loads((output / "scene.json").read_text())
                native = report["windowed_evidence"]
                checks = (report["status"] == "PASS", report["scene"] == "rendering",
                          report["headless_evidence"]["executed"] is False,
                          report["lifecycle"]["module_unloaded_before_engine_shutdown"] is True,
                          native["executed"] is True, native["backend"] == "vulkan",
                          native["backend_fallback"] is False, native["scene_draws"] == frames,
                          native["native_scene_draws"] == frames,
                          native["surface_acquires"] == frames, native["surface_presents"] == frames,
                          native["composed_frames"] == 0, native["rendering_mode"] == "gpu_scene",
                          native["resize_requests"] == 1, native["resize_generations"] >= 1)
                if not all(checks):
                    error = "Rendering Room evidence mismatch"
                if args.interactive and report["interaction_overlay"]["camera_moves"] <= 0:
                    error = "normalized D input did not move the gallery camera"
            except (OSError, ValueError, KeyError, TypeError) as failure:
                error = f"invalid Rendering Room report: {failure}"
        elif not (output / "scene.ppm").is_file():
            error = "native pixel gate did not retain its capture"
        if "Validation Error" in run.stdout + run.stderr:
            error = "Khronos validation reported a Vulkan error"
        launch["acceptance_passed"] = not error
        if error:
            launch["validation_error"] = error
        (output / "launch.json").write_text(json.dumps(launch, indent=2) + "\n")
        print(run.stdout)
        print(run.stderr)
        if error:
            print(f"FAIL: {error}")
            return 1
        print(json.dumps(launch, indent=2))
        return 0
    finally:
        server.terminate()
        try:
            server.wait(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.wait(timeout=3)


if __name__ == "__main__":
    raise SystemExit(main())
