#!/usr/bin/env python3
"""Run the bounded Linux/Vulkan Showcase acceptance under an isolated Xvfb display."""

import argparse
import json
import os
from pathlib import Path
import select
import shutil
import subprocess
import sys
import tempfile
import time


def validate_evidence(evidence: dict) -> dict:
    """Require native startup, resize, frame submission and ordered shutdown evidence."""
    expected = {
        "schema": "nexora.zig_showcase.v1",
        "status": "PASS",
        "mode": "interactive",
        "backend": "vulkan",
    }
    for key, value in expected.items():
        if evidence.get(key) != value:
            raise ValueError(f"{key}: expected {value!r}, got {evidence.get(key)!r}")
    if evidence.get("headless_evidence", {}).get("executed") is not False:
        raise ValueError("native acceptance must not be reported as headless")
    lifecycle = evidence.get("lifecycle", {})
    for key in ("engine_initialized", "module_callbacks", "module_unloaded_before_engine_shutdown"):
        if lifecycle.get(key) is not True:
            raise ValueError(f"lifecycle.{key}: required startup/shutdown evidence is missing")
    if type(lifecycle.get("frames")) is not int or lifecycle["frames"] != 4:
        raise ValueError("lifecycle.frames: expected four executed frames")
    windowed = evidence.get("windowed_evidence", {})
    for key, value in {
        "executed": True,
        "backend": "vulkan",
        "backend_fallback": False,
        "clear_color": True,
        "triangle": True,
        "diagnostics_overlay": True,
    }.items():
        if type(windowed.get(key)) is not type(value) or windowed[key] != value:
            raise ValueError(f"windowed_evidence.{key}: expected {value!r}")
    for key, value in {
        "surface_acquires": 4, "surface_presents": 4,
        "resize_requests": 1, "composed_frames": 4,
    }.items():
        if type(windowed.get(key)) is not int or windowed[key] != value:
            raise ValueError(f"windowed_evidence.{key}: expected {value}")
    generations = windowed.get("resize_generations")
    if type(generations) is not int or generations < 1:
        raise ValueError("windowed_evidence.resize_generations: resize was not observed")
    return windowed


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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("showcase")
    parser.add_argument("--evidence-dir", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="nexora-showcase-") as temporary:
        output = args.evidence_dir or Path(temporary)
        output.mkdir(parents=True, exist_ok=True)
        report = output / "windowed.json"
        # A failed run must never inherit a passing report from a previous invocation.
        for name in ("windowed.json", "stdout.log", "stderr.log", "launch.json"):
            (output / name).unlink(missing_ok=True)
        xvfb = shutil.which("Xvfb")
        if xvfb is None:
            return unavailable("Xvfb is not installed")
        server, display = start_xvfb(xvfb, "1280x720x24")
        try:
            if display is None:
                server.terminate()
                _, errors = server.communicate(timeout=3)
                return unavailable(f"Xvfb failed to start: {errors.strip()}")
            environment = os.environ.copy()
            environment["DISPLAY"] = display
            command = [
                args.showcase,
                "--mode=interactive",
                "--backend=vulkan",
                "--gameplay-module=static",
                "--frames=4",
                "--resize=960x540",
                "--no-reload",
                f"--report={report}",
            ]
            completed = subprocess.run(
                command,
                env=environment,
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
            (output / "stdout.log").write_text(completed.stdout, encoding="utf-8")
            (output / "stderr.log").write_text(completed.stderr, encoding="utf-8")
            launch = {
                "command": command, "exit_code": completed.returncode,
                "display": display, "acceptance_scope": "Linux/X11/Vulkan virtual display",
                "physical_display_verified": False, "acceptance_passed": False,
            }
            launch_path = output / "launch.json"
            launch_path.write_text(json.dumps(launch, indent=2) + "\n", encoding="utf-8")
            if completed.returncode != 0:
                print(completed.stdout)
                print(completed.stderr, file=sys.stderr)
                return completed.returncode
            assert "Validation Error" not in completed.stderr and "SYNC-HAZARD" not in completed.stderr, completed.stderr
            evidence = json.loads(report.read_text(encoding="utf-8"))
            windowed = evidence["windowed_evidence"]
            assert evidence["headless_evidence"]["executed"] is False
            assert windowed["executed"] is True
            assert windowed["backend"] == "vulkan"
            assert windowed["surface_acquires"] == 4
            assert windowed["surface_presents"] == 4
            assert windowed["resize_requests"] == 1
            assert windowed["resize_generations"] >= 1
            assert windowed["composed_frames"] == 0
            assert windowed["scene_draws"] == 4
            assert windowed["native_ui_draws"] == 4
            assert windowed["rendering_mode"] == "gpu_scene"
            assert evidence["runtime_rooms"]["healthy"] is True
            assert windowed["clear_color"] is True
            assert windowed["triangle"] is False
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
