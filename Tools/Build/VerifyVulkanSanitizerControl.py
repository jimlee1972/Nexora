#!/usr/bin/env python3
"""Require the separate native scene fault to be detected by LeakSanitizer."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys


def verify(native: Path, output: Path, helper: Path) -> int:
    output.mkdir(parents=True, exist_ok=True)
    try:
        run = subprocess.run([sys.executable, str(helper), "--native-test", str(native),
                              "--evidence-dir", str(output)], capture_output=True, text=True,
                             timeout=43, check=False)
        text = run.stdout + run.stderr
        print(text)
        launch = json.loads((output / "launch.json").read_text(encoding="utf-8"))
        if not isinstance(launch, dict):
            raise ValueError("Missing native launch object")
        summaries = re.findall(r"SUMMARY: AddressSanitizer: ([^\r\n]+)", text)
        passed = (run.returncode == 1 and type(launch.get("exit_code")) is int
                  and launch["exit_code"] > 0
                  and summaries == ["17 byte(s) leaked in 1 allocation(s)."]
                  and "InjectVulkanSanitizerLeak" in text
                  and "PASS: 29 scene submissions" in text
                  and "ERROR: LeakSanitizer: detected memory leaks" in text
                  and "ERROR: AddressSanitizer:" not in text and "runtime error:" not in text
                  and (output / "scene.ppm").is_file())
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: native sanitizer positive control: {error}")
        return 1
    (output / "positive-control.json").write_text(json.dumps({
        "control_passed": passed, "expected_leak_bytes": 17,
        "native_exit_code": launch.get("exit_code"), "original_acceptance_replaced": False,
        "physical_display_verified": False,
    }, indent=2) + "\n", encoding="utf-8")
    if not passed:
        print("FAIL: expected completed native scene and exactly its injected 17-byte leak")
        return 1
    print("PASS: completed native scene; LeakSanitizer detected the injected 17-byte fault")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-test", type=Path, required=True)
    parser.add_argument("--evidence-dir", type=Path, required=True)
    args = parser.parse_args()
    helper = Path(__file__).resolve().parents[2] / "Tests/Showcase/LinuxVulkanSceneAcceptance.py"
    return verify(args.native_test.resolve(), args.evidence_dir.resolve(), helper)


if __name__ == "__main__":
    raise SystemExit(main())
