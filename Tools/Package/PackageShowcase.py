#!/usr/bin/env python3
"""Create a deterministic, self-describing Nexora Showcase directory."""

import argparse
import hashlib
import json
import shutil
from pathlib import Path


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def copy(source: Path, destination: Path) -> dict:
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    return {"path": destination.name, "bytes": destination.stat().st_size,
            "sha256": digest(destination)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", required=True, choices=("Development", "Shipping"))
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--gameplay-module", type=Path)
    parser.add_argument("--api-manifest", required=True, type=Path)
    parser.add_argument("--license", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    if args.profile == "Development" and args.gameplay_module is None:
        parser.error("Development packages require --gameplay-module")
    for path in (args.binary, args.api_manifest, args.license, args.gameplay_module):
        if path is not None and not path.is_file():
            parser.error(f"input does not exist: {path}")

    if args.output.exists():
        shutil.rmtree(args.output)
    bin_dir = args.output / "bin"
    manifest_dir = args.output / "manifests"
    artifacts = [copy(args.binary, bin_dir / args.binary.name)]
    if args.gameplay_module:
        artifacts.append(copy(args.gameplay_module, bin_dir / args.gameplay_module.name))
    copy(args.license, args.output / "LICENSE")
    copy(args.api_manifest, manifest_dir / "api.json")

    artifacts.sort(key=lambda item: item["path"])
    module_argument = ""
    if args.gameplay_module:
        module_argument = (f" --gameplay-module=dynamic"
                           f" --gameplay-library=bin/{args.gameplay_module.name}")
    else:
        module_argument = " --gameplay-module=static"
    build = {
        "schema_version": 1,
        "application": "NexoraShowcase",
        "profile": args.profile,
        "gameplay_linkage": "dynamic" if args.gameplay_module else "static",
        "launch": (f"bin/{args.binary.name} --headless --scene=tour --frames=1 --no-reload"
                   f"{module_argument} --report=launch-report.json"),
    }
    content = {"schema_version": 1, "artifacts": artifacts}
    manifest_dir.mkdir(parents=True, exist_ok=True)
    (manifest_dir / "build.json").write_text(json.dumps(build, indent=2) + "\n", encoding="utf-8")
    (manifest_dir / "content.json").write_text(json.dumps(content, indent=2) + "\n", encoding="utf-8")

    checksums = []
    for path in sorted(p for p in args.output.rglob("*") if p.is_file()):
        checksums.append(f"{digest(path)}  {path.relative_to(args.output).as_posix()}")
    (manifest_dir / "SHA256SUMS").write_text("\n".join(checksums) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
