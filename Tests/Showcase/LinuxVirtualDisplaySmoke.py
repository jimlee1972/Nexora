#!/usr/bin/env python3
"""Run the bounded Linux/Vulkan Showcase acceptance under an isolated Xvfb display."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: LinuxVirtualDisplaySmoke.py NEXORA_SHOWCASE")
    xvfb = shutil.which("Xvfb")
    if xvfb is None:
        print("SKIP: Xvfb is not installed; virtual-display acceptance was not executed")
        return 77

    with tempfile.TemporaryDirectory(prefix="nexora-showcase-") as temporary:
        report = Path(temporary) / "windowed.json"
        display = f":{100 + os.getpid() % 500}"
        server = subprocess.Popen(
            [xvfb, display, "-screen", "0", "1280x720x24", "-nolisten", "tcp"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE,
            text=True,
        )
        try:
            time.sleep(0.5)
            if server.poll() is not None:
                print(f"SKIP: Xvfb failed to start: {server.stderr.read().strip()}")
                return 77
            environment = os.environ.copy()
            environment["DISPLAY"] = display
            completed = subprocess.run(
                [
                    sys.argv[1],
                    "--mode=interactive",
                    "--backend=vulkan",
                    "--gameplay-module=static",
                    "--frames=4",
                    "--resize=960x540",
                    "--no-reload",
                    f"--report={report}",
                ],
                env=environment,
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
            if completed.returncode != 0:
                print(completed.stdout)
                print(completed.stderr, file=sys.stderr)
                return completed.returncode
            evidence = json.loads(report.read_text(encoding="utf-8"))
            windowed = evidence["windowed_evidence"]
            assert evidence["headless_evidence"]["executed"] is False
            assert windowed["executed"] is True
            assert windowed["backend"] == "vulkan"
            assert windowed["surface_acquires"] == 4
            assert windowed["surface_presents"] == 4
            assert windowed["resize_requests"] == 1
            assert windowed["resize_generations"] >= 1
            assert windowed["composed_frames"] == 4
            assert windowed["clear_color"] is True
            assert windowed["triangle"] is True
            assert windowed["diagnostics_overlay"] is True
            print(json.dumps(windowed, indent=2))
            return 0
        finally:
            server.terminate()
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()


if __name__ == "__main__":
    raise SystemExit(main())
