#!/usr/bin/env python3
"""Verify bounded real Editor frames observe native process RSS through the app owner."""

import argparse
import math
import os
from pathlib import Path
import re
import subprocess
import tempfile

from LinuxDisplayAcceptance import start_xvfb


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", required=True)
    parser.add_argument("--xvfb", required=True)
    args = parser.parse_args()
    server, display = start_xvfb(args.xvfb, "1600x900x24")
    try:
        if display is None:
            raise RuntimeError("Xvfb did not become ready")
        environment = os.environ.copy()
        environment["DISPLAY"] = display
        with tempfile.TemporaryDirectory(prefix="nexora-process-memory-") as temporary:
            root = Path(temporary)
            (root / "Content").mkdir()
            (root / ".nexora").mkdir()
            (root / "project.nexora").write_text("schema=1\nname=Memory Observation\n")
            (root / ".nexora/workspace").write_text("schema=1\n")
            completed = subprocess.run(
                [args.editor, "--graphical", f"--project={root}", "--frames=24",
                 f"--recent-projects={root / 'recent-projects'}"],
                env=environment, text=True, capture_output=True, timeout=30, check=True)
            evidence = completed.stdout + completed.stderr
            print(evidence, end="" if evidence.endswith("\n") else "\n")
            if re.search(r"Validation Error\b|VUID-|SYNC-HAZARD-", evidence):
                raise RuntimeError("native graphical run reported Vulkan validation failure")
            memory = re.search(
                r"process memory evidence: scope=current_process_resident_set unit=bytes "
                r"attempts=(\d+) successful=(\d+) latest=(\d+) observed_peak=(\d+)", evidence)
            if not memory:
                raise RuntimeError("production owner did not publish actual native RSS evidence")
            attempts, successful, latest, peak = map(int, memory.groups())
            if not (1 <= successful <= attempts <= 24 and 0 < latest <= peak):
                raise RuntimeError("invalid native observation count, resident bytes or observed peak")
            gpu = re.search(
                r"GPU timing evidence: scope=native_command_buffer_interval unit=milliseconds "
                r"source=(\d+) software=(\d+) completed=(\d+) retained=(\d+) dropped=(\d+) latest=(\S+)", evidence)
            if not gpu:
                raise RuntimeError("native owner did not publish GPU timing availability/source")
            source, software, completed_gpu, retained, dropped = map(int, gpu.groups()[:5])
            latest = gpu.group(6)
            if source == 1:
                if not (software in (0, 1) and 1 <= retained <= completed_gpu <= 24 and dropped == 0 and
                        latest != "unavailable" and math.isfinite(float(latest)) and float(latest) >= 0):
                    raise RuntimeError("real Vulkan timestamp result is unavailable or invalid")
            elif source != 0 or retained != 0 or latest != "unavailable":
                raise RuntimeError("unsupported native source advertised GPU timing")
            render = re.search(r"graphical evidence: acquired=(\d+) presented=(\d+) ui_draws=(\d+)",
                               evidence)
            if not render or any(value <= 0 for value in map(int, render.groups())):
                raise RuntimeError("real process did not present graphical Editor frames")
            if (root / ".nexora/frame-processing.json").exists() or \
                    (root / ".nexora/frame-processing.csv").exists() or \
                    (root / ".nexora/process-memory.json").exists() or \
                    (root / ".nexora/gpu-timing.json").exists():
                raise RuntimeError("live profiling implicitly wrote a capture")
    finally:
        if server.poll() is None:
            server.terminate()
        server.communicate(timeout=5)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
