#!/usr/bin/env python3
"""Run the production Player host's actual SDK generation/pixel contract on Xvfb."""
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
    parser.add_argument("--modules", nargs=5, required=True)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    output = args.evidence_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    xvfb = shutil.which("Xvfb")
    if not xvfb:
        return unavailable("Xvfb is not installed")
    server, display = start_xvfb(xvfb, "1280x720x24")
    try:
        if display is None:
            return unavailable("Player reload Xvfb did not start")
        environment = os.environ.copy()
        environment["DISPLAY"] = display
        command = [args.app, *args.modules, str(output), "--native"]
        run = subprocess.run(command, env=environment, capture_output=True, timeout=110)
        (output / "stdout.log").write_bytes(run.stdout)
        (output / "stderr.log").write_bytes(run.stderr)
        receipt = {"command": command, "exit_code": run.returncode,
                   "physical_display_verified": False}
        (output / "launch.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
        if run.returncode:
            raise RuntimeError(run.stderr.decode("utf-8", errors="replace"))
        roots = sorted(output.glob("run-*/acceptance.json"), key=lambda p: p.stat().st_mtime_ns)
        if not roots:
            raise RuntimeError("Player reload acceptance receipt missing")
        accepted = json.loads(roots[-1].read_text(encoding="utf-8"))
        assert accepted["accepted"] == 32 and accepted["rejected"] == 128, accepted
        assert accepted["native_frames"] == 192 and accepted["allocations_balanced"], accepted
        assert len(list(roots[-1].parent.glob("frame-*.ppm"))) == 192, accepted
        print("Production Player host:32 actual SDK replacements,128 rollbacks,192 native presents")
        return 0
    finally:
        server.terminate()
        server.communicate(timeout=3)


if __name__ == "__main__":
    raise SystemExit(main())
