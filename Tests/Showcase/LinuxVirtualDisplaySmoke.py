#!/usr/bin/env python3
"""Run the bounded Linux/Vulkan Showcase acceptance under an isolated Xvfb display."""

import json
import os
from pathlib import Path
import select
import shutil
import subprocess
import sys
import tempfile
import time


def unavailable(message: str) -> int:
    """Treat missing native evidence as a CI failure and a local skip."""
    if os.environ.get("CI", "").lower() == "true":
        print(f"FAIL: {message}; CI requires Linux virtual-display acceptance")
        return 1
    print(f"SKIP: {message}; virtual-display acceptance was not executed")
    return 77


def start_xvfb(xvfb: str, screen: str, timeout: float = 10.0):
    """Start Xvfb on a display it picks itself; return (server, ":N") once it accepts clients.

    `-displayfd` makes Xvfb choose a free display and write its number only when the server is
    ready, so neither a fixed sleep nor a guessed display number (which parallel tests can share)
    is involved. Returns (server, None) when it does not become ready within `timeout`.
    """
    read_fd, write_fd = os.pipe()
    server = subprocess.Popen(
        [xvfb, "-displayfd", str(write_fd), "-screen", "0", screen, "-nolisten", "tcp"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
        pass_fds=(write_fd,),
    )
    os.close(write_fd)
    data = b""
    deadline = time.monotonic() + timeout
    try:
        while not data.endswith(b"\n"):
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([read_fd], [], [], remaining)[0]:
                break
            chunk = os.read(read_fd, 64)
            if not chunk:  # Xvfb exited before becoming ready.
                break
            data += chunk
    finally:
        os.close(read_fd)
    if not data.endswith(b"\n"):
        return server, None
    return server, f":{int(data)}"


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: LinuxVirtualDisplaySmoke.py NEXORA_SHOWCASE")
    xvfb = shutil.which("Xvfb")
    if xvfb is None:
        return unavailable("Xvfb is not installed")

    with tempfile.TemporaryDirectory(prefix="nexora-showcase-") as temporary:
        report = Path(temporary) / "windowed.json"
        server, display = start_xvfb(xvfb, "1280x720x24")
        try:
            if display is None:
                server.terminate()
                _, errors = server.communicate(timeout=3)
                return unavailable(f"Xvfb failed to start: {errors.strip()}")
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
