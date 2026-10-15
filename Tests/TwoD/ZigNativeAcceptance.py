#!/usr/bin/env python3
"""Verify physical-key input reaches Zig and drives a real native 2D frame loop."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Showcase"))
from LinuxVirtualDisplaySmoke import start_xvfb, unavailable


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", required=True)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    xvfb, xdotool = shutil.which("Xvfb"), shutil.which("xdotool")
    if not xvfb or not xdotool:
        return unavailable("Zig 2D native acceptance requires Xvfb and xdotool")
    output = args.evidence_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    report = output / "zig-2d.json"
    report.unlink(missing_ok=True)
    server, display = start_xvfb(xvfb, "800x600x24")
    process = None
    environment = os.environ.copy()
    try:
        if display is None:
            return unavailable("Xvfb did not start for Zig 2D acceptance")
        environment["DISPLAY"] = display
        command = [args.app, "--interactive", "--frames", "600", "--require-movement",
                   "--report", str(report)]
        process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE, text=True)
        window = subprocess.check_output([xdotool, "search", "--sync", "--onlyvisible",
                                          "--name", "^Nexora Zig 2D$"], env=environment,
                                         text=True, timeout=8).splitlines()[0]
        subprocess.run([xdotool, "windowfocus", "--sync", window], env=environment,
                       capture_output=True, check=True, timeout=3)
        subprocess.run([xdotool, "keydown", "--window", window, "d"], env=environment,
                       capture_output=True, check=True, timeout=3)
        stdout, stderr = process.communicate(timeout=30)
        (output / "stdout.log").write_text(stdout, encoding="utf-8")
        (output / "stderr.log").write_text(stderr, encoding="utf-8")
        if process.returncode:
            raise RuntimeError(f"Zig 2D exited {process.returncode}: {stderr}")
        evidence = json.loads(report.read_text(encoding="utf-8"))
        if (evidence["status"] != "PASS" or evidence["native_frames"] != 600 or
                evidence["sprite_instanced_draws"] < 600 or
                evidence["sprite_gpu_instances"] < 600 * 32 or
                evidence["sprite_instance_upload_bytes"] != evidence["sprite_gpu_instances"] * 44 or
                evidence["movement_frames"] == 0 or evidence["entity_x"] <= 0 or
                not evidence["reloaded"] or not evidence["allocations_balanced"] or
                evidence["physical_windows"] != "NOT RUN"):
            raise RuntimeError("Zig/native input, rendering or reload evidence mismatch")
        if "Validation Error" in stdout + stderr:
            raise RuntimeError("Vulkan validation reported an error")
        print(json.dumps(evidence, indent=2))
        return 0
    finally:
        if process is not None and process.poll() is None:
            process.kill()
            process.communicate(timeout=3)
        if display is not None:
            subprocess.run([xdotool, "keyup", "d"], env=environment,
                           capture_output=True, timeout=3)
        server.terminate()
        try:
            server.communicate(timeout=3)
        except subprocess.TimeoutExpired:
            server.kill()
            server.communicate(timeout=3)


if __name__ == "__main__":
    raise SystemExit(main())
