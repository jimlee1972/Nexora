#!/usr/bin/env python3
"""Retain headless and native evidence from checksum-verified isolated POSIX release copies."""
import argparse
import ctypes
import json
import os
from pathlib import Path
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile

from LaunchShowcasePackage import safe_join, verify_runtime_closure
from PackageShowcase import digest


def metal_available() -> bool:
    metal = ctypes.CDLL("/System/Library/Frameworks/Metal.framework/Metal")
    metal.MTLCreateSystemDefaultDevice.restype = ctypes.c_void_p
    graphics = ctypes.CDLL("/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics")
    graphics.CGMainDisplayID.restype = ctypes.c_uint32
    graphics.CGDisplayIsActive.argtypes = [ctypes.c_uint32]
    graphics.CGDisplayIsActive.restype = ctypes.c_bool
    return bool(metal.MTLCreateSystemDefaultDevice() and
                graphics.CGDisplayIsActive(graphics.CGMainDisplayID()))


def validate_native(report: dict, backend: str, room: str) -> None:
    native = report["windowed_evidence"]
    checks = [report["status"] == "PASS", report["scene"] == room,
              report["headless_evidence"]["executed"] is False,
              report["lifecycle"]["module_unloaded_before_engine_shutdown"] is True,
              native["executed"] is True, native["backend"] == backend,
              native["backend_fallback"] is False, native["composed_frames"] == 0,
              native["rendering_mode"] == "gpu_scene", native["native_graph_frames"] == 12,
              native["native_graph_order"] == ["Offscreen", "Main", "UI", "Present"],
              native["surface_acquires"] == 12, native["surface_presents"] == 12,
              native["native_scene_draws"] == 12, native["native_offscreen_draws"] == 12,
              native["native_scene_composites"] == 12,
              native["resize_requests"] == 1, native["resize_generations"] >= 1]
    if not all(checks):
        raise ValueError(f"native {backend}/{room} evidence mismatch")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--evidence-directory", required=True, type=Path)
    parser.add_argument("--allow-unavailable-display", action="store_true")
    args = parser.parse_args()
    system = platform.system()
    if system not in ("Linux", "Darwin"):
        parser.error("POSIX release verification requires Linux or macOS")
    source, output = args.package.resolve(), args.evidence_directory.resolve()
    if output == source or source in output.parents:
        parser.error("evidence must be outside the package")
    output.mkdir(parents=True, exist_ok=True)
    status_path = output / "native-acceptance.json"
    status_path.unlink(missing_ok=True)
    (output / "headless.json").unlink(missing_ok=True)
    # Each headless attempt writes a fresh isolated-copy report, separate from native acceptance.
    headless = subprocess.run([sys.executable, str(Path(__file__).with_name("LaunchShowcasePackage.py")),
                               "--package", str(source), "--evidence", str(output / "headless.json")],
                              text=True, capture_output=True)
    (output / "headless.stdout.log").write_text(headless.stdout)
    (output / "headless.stderr.log").write_text(headless.stderr)
    if headless.returncode:
        print(headless.stderr[-8192:], file=sys.stderr)
        return headless.returncode
    result = {"status": "FAIL", "platform": system, "isolated_copy": True,
              "physical_display_verified": False, "issues": []}
    if system == "Darwin" and not metal_available():
        result.update(status="UNSUPPORTED", issues=["Metal device or active WindowServer display unavailable"])
        status_path.write_text(json.dumps(result, indent=2) + "\n")
        return 77 if args.allow_unavailable_display else 1
    with tempfile.TemporaryDirectory(prefix="nexora-native-release-") as temporary:
        staged = Path(temporary) / "NexoraShowcase"
        shutil.copytree(source, staged)
        for line in (source / "manifests/SHA256SUMS").read_text().splitlines():
            expected, relative = line.split("  ", 1)
            if digest(safe_join(staged, relative)) != expected:
                raise RuntimeError(f"isolated-copy checksum mismatch: {relative}")
        build = json.loads((staged / "manifests/build.json").read_text())
        if build["profile"] != "Shipping" or build["shipping_profile"] != "Full":
            parser.error("native release acceptance requires Shipping/Full")
        binary = safe_join(staged, shlex.split(build["launch"])[0])
        environment = os.environ.copy()
        if system == "Linux":
            environment["LD_LIBRARY_PATH"] = str(staged / "bin") + os.pathsep + environment.get("LD_LIBRARY_PATH", "")
        verify_runtime_closure(binary, staged, environment)
        try:
            if system == "Linux":
                environment["NEXORA_SHOWCASE_EVIDENCE_DIR"] = str(output / "native")
                # Uses the same real keyboard/mouse, room, Lab and screenshot gate as CTest.
                gate = Path(__file__).resolve().parents[2] / "Tests/Showcase/LinuxShowcaseInteraction.py"
                native = subprocess.run([sys.executable, str(gate), str(binary)], cwd=staged,
                                        env=environment, text=True, capture_output=True, timeout=90)
                (output / "native.stdout.log").write_text(native.stdout)
                (output / "native.stderr.log").write_text(native.stderr)
                if native.returncode:
                    print(native.stdout[-8192:], file=sys.stderr)
                    print(native.stderr[-8192:], file=sys.stderr)
                    raise RuntimeError(f"Linux native interaction failed ({native.returncode})")
                result["scope"] = "Linux Xvfb native interaction and screenshots; driver identity is recorded by the application"
            else:
                # Counter/report smoke does not claim screen capture, physical display or human review.
                for room in ("hub", "rendering", "scene", "input", "gameplay", "presentation", "streaming", "shipping"):
                    report = output / f"{room}.json"
                    report.unlink(missing_ok=True)
                    command = [str(binary), "--mode=interactive", "--backend=metal", f"--scene={room}",
                               "--frames=12", "--resize=960x540", "--no-reload", "--gameplay-module=static",
                               f"--report={report}"]
                    native = subprocess.run(command, cwd=staged, env=environment,
                                            text=True, capture_output=True, timeout=45)
                    (output / f"{room}.stdout.log").write_text(native.stdout)
                    (output / f"{room}.stderr.log").write_text(native.stderr)
                    if native.returncode:
                        print(native.stdout[-8192:], file=sys.stderr)
                        print(native.stderr[-8192:], file=sys.stderr)
                        raise RuntimeError(f"Metal {room} failed ({native.returncode})")
                    validate_native(json.loads(report.read_text()), "metal", room)
                result["scope"] = "macOS Metal eight-room native graph/report smoke; no screenshots or physical-display attestation"
            result["status"] = "PASS"
        except (RuntimeError, ValueError, KeyError, OSError, subprocess.TimeoutExpired) as error:
            result["issues"].append(str(error))
            if isinstance(error, subprocess.TimeoutExpired) and error.stderr:
                detail = error.stderr.decode(errors="replace") if isinstance(error.stderr, bytes) else error.stderr
                print(detail[-8192:], file=sys.stderr)
            print(json.dumps(result, indent=2), file=sys.stderr)
        finally:
            status_path.write_text(json.dumps(result, indent=2) + "\n")
    return 0 if result["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
