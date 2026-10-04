#!/usr/bin/env python3
"""Assemble portable release provenance and acceptance archives after native verification."""
import argparse
import json
import os
import platform
from pathlib import Path
import tarfile

from PackageShowcase import digest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--platform", choices=("linux-x64", "macos-arm64"), required=True)
    parser.add_argument("--preset", required=True)
    args = parser.parse_args()
    expected = ("Linux", "x86_64") if args.platform == "linux-x64" else ("Darwin", "arm64")
    if (platform.system(), platform.machine()) != expected:
        parser.error(f"runner architecture does not match release platform: {platform.system()}/{platform.machine()}")
    archive = args.bundle / f"NexoraShowcase-Shipping-{args.platform}.zip"
    archive.with_suffix(".zip.sha256").write_text(f"{digest(archive)}  {archive.name}\n")
    evidence = Path("build") / args.preset / "artifacts/release-native"
    with tarfile.open(args.bundle / f"{args.platform}-acceptance.tar.gz", "w:gz") as target:
        target.add(evidence, arcname=f"{args.platform}-acceptance")
    info = {"git_commit": os.environ["GITHUB_SHA"], "git_ref": os.environ["GITHUB_REF"],
            "workflow_run": f"{os.environ['GITHUB_SERVER_URL']}/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}",
            "platform": args.platform, "preset": args.preset,
            "native_acceptance": json.loads((evidence / "native-acceptance.json").read_text()),
            "note": "Hosted-runner evidence. Native UNSUPPORTED does not certify graphics; physical and clean-host acceptance are separate."}
    (args.bundle / f"release-info-{args.platform}.json").write_text(json.dumps(info, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
