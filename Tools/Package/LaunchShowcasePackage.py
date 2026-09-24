#!/usr/bin/env python3
"""Verify and launch a Showcase package from an isolated copy."""

import argparse
import json
import shlex
import shutil
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path

from PackageShowcase import digest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--evidence", required=True, type=Path)
    args = parser.parse_args()

    source = args.package.resolve()
    checksums = source / "manifests/SHA256SUMS"
    build_path = source / "manifests/build.json"
    if not checksums.is_file() or not build_path.is_file():
        parser.error("package manifests are missing")

    verified = []
    for line in checksums.read_text(encoding="utf-8").splitlines():
        expected, relative = line.split("  ", 1)
        artifact = source / relative
        if not artifact.is_file() or digest(artifact) != expected:
            raise RuntimeError(f"package checksum mismatch: {relative}")
        verified.append(relative)

    with tempfile.TemporaryDirectory(prefix="nexora-clean-package-") as temporary:
        staged = Path(temporary) / "NexoraShowcase"
        shutil.copytree(source, staged)
        build = json.loads((staged / "manifests/build.json").read_text(encoding="utf-8"))
        command = shlex.split(build["launch"])
        executable = staged / command[0]
        executable.chmod(executable.stat().st_mode | 0o100)
        completed = subprocess.run([str(executable), *command[1:]], cwd=staged,
                                   text=True, capture_output=True, check=False)
        report = staged / "launch-report.json"
        if completed.returncode != 0 or not report.is_file():
            raise RuntimeError(f"package launch failed ({completed.returncode}):\n"
                               f"{completed.stdout}\n{completed.stderr}")
        launch_report = json.loads(report.read_text(encoding="utf-8"))

    evidence = {
        "schema_version": 1,
        "platform": "linux",
        "profile": build["profile"],
        "isolated_copy": True,
        "checksums_verified": len(verified),
        "command": build["launch"],
        "exit_code": completed.returncode,
        "launch_report": launch_report,
        "recorded_utc": datetime.now(timezone.utc).isoformat(),
    }
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(evidence, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
