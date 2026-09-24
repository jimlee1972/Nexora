#!/usr/bin/env python3
"""Produce a deterministic V1-to-V2 migration audit for a Nexora project."""

import argparse
import hashlib
import json
from pathlib import Path


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def scan(root: Path) -> dict:
    required = {
        "module_graph": root / "Config/Modules/modules.json",
        "abi_manifest": root / "Engine/API/abi_manifest.json",
        "cmake_presets": root / "CMakePresets.json",
    }
    findings = []
    artifacts = {}
    for name, path in required.items():
        relative = path.relative_to(root).as_posix()
        if not path.is_file():
            findings.append({"code": "missing-required-file", "path": relative,
                             "severity": "error"})
            continue
        try:
            value = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            findings.append({"code": "invalid-json", "path": relative,
                             "severity": "error", "detail": str(error)})
            continue
        artifacts[name] = {"path": relative, "sha256": digest(path)}
        if name == "abi_manifest":
            artifacts[name]["abi_version"] = value.get("abi", {}).get("major")
        elif name == "module_graph":
            artifacts[name]["schema_version"] = value.get("schema_version")
        else:
            names = {preset.get("name") for preset in value.get("configurePresets", [])}
            for profile in ("linux-development", "linux-shipping"):
                if profile not in names:
                    findings.append({"code": "missing-build-profile", "profile": profile,
                                     "severity": "error"})

    plugins = []
    for manifest in sorted(root.glob("Plugins/*/plugin.json")):
        relative = manifest.relative_to(root).as_posix()
        try:
            value = json.loads(manifest.read_text(encoding="utf-8"))
            plugins.append({"path": relative, "sha256": digest(manifest),
                            "schema_version": value.get("schema_version"),
                            "engine_abi": value.get("engine_abi")})
        except (OSError, json.JSONDecodeError) as error:
            findings.append({"code": "invalid-plugin-manifest", "path": relative,
                             "severity": "error", "detail": str(error)})

    fingerprint_input = json.dumps({"artifacts": artifacts, "plugins": plugins},
                                   sort_keys=True, separators=(",", ":")).encode()
    return {
        "report_version": 1,
        "source_generation": "V1",
        "target_generation": "V2",
        "compatible": not any(item["severity"] == "error" for item in findings),
        "rebuild_required": True,
        "artifacts": artifacts,
        "plugins": plugins,
        "findings": findings,
        "baseline_fingerprint": hashlib.sha256(fingerprint_input).hexdigest(),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("project", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    report = scan(args.project.resolve())
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0 if report["compatible"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
