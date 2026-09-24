#!/usr/bin/env python3
"""Deterministic V2 production metadata, import, and cook commandlet."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1


def canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode()


def write_atomic(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    temporary.write_bytes(data)
    temporary.replace(path)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _walk_ast(node: Any):
    if isinstance(node, dict):
        yield node
        for child in node.get("inner", []):
            yield from _walk_ast(child)


def generate_reflection(source: Path, clang: str, clang_args: list[str]) -> dict[str, Any]:
    command = [clang, "-std=c++20", "-fsyntax-only", "-Xclang", "-ast-dump=json", *clang_args, str(source)]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or "clang AST generation failed")
    ast = json.loads(result.stdout)
    source_resolved = source.resolve()
    records = []
    for node in _walk_ast(ast):
        if node.get("kind") not in ("CXXRecordDecl", "RecordDecl") or not node.get("completeDefinition"):
            continue
        location = node.get("loc", {})
        file_name = location.get("file")
        if file_name and Path(file_name).resolve() != source_resolved:
            continue
        annotations = [child for child in node.get("inner", []) if child.get("kind") == "AnnotateAttr"]
        if not annotations:
            continue
        fields = []
        for field in node.get("inner", []):
            if field.get("kind") == "FieldDecl":
                fields.append({"name": field["name"], "type": field.get("type", {}).get("qualType", "")})
        records.append({"name": node.get("qualifiedName", node.get("name", "")), "fields": sorted(fields, key=lambda f: f["name"])})
    records.sort(key=lambda record: record["name"])
    return {"schema_version": SCHEMA_VERSION, "generator": "nexora-clang-ast-v1", "types": records}


class DerivedDataCache:
    def __init__(self, root: Path):
        self.root = root

    @staticmethod
    def key(source: bytes, importer: str, version: str, settings: dict[str, Any]) -> str:
        descriptor = {"importer": importer, "version": version, "settings": settings, "source_sha256": sha256(source)}
        return sha256(canonical_bytes(descriptor))

    def store(self, key: str, artifact: bytes) -> Path:
        path = self.root / key[:2] / key[2:]
        if path.exists() and path.read_bytes() != artifact:
            raise RuntimeError(f"DDC collision for {key}")
        if not path.exists():
            write_atomic(path, artifact)
        return path


def worker_request(request: dict[str, Any]) -> dict[str, Any]:
    source = Path(request["source"])
    source_bytes = source.read_bytes()
    if request.get("settings", {}).get("simulate_crash"):
        os._exit(70)
    artifact = canonical_bytes({
        "schema_version": SCHEMA_VERSION,
        "source_sha256": sha256(source_bytes),
        "settings": request.get("settings", {}),
    })
    return {"artifact_hex": artifact.hex()}


def isolated_import(source: Path, ddc: DerivedDataCache, settings: dict[str, Any]) -> dict[str, Any]:
    request = {"source": str(source.resolve()), "settings": settings}
    process = subprocess.run(
        [sys.executable, str(Path(__file__).resolve()), "_worker"],
        input=json.dumps(request), capture_output=True, text=True, check=False,
    )
    if process.returncode:
        return {"ok": False, "error": f"import worker exited with code {process.returncode}"}
    response = json.loads(process.stdout)
    artifact = bytes.fromhex(response["artifact_hex"])
    key = ddc.key(source.read_bytes(), "raw", "1", settings)
    path = ddc.store(key, artifact)
    return {"ok": True, "key": key, "artifact": str(path), "artifact_sha256": sha256(artifact)}


def externalize_scene(scene: dict[str, Any], output: Path) -> dict[str, Any]:
    entities = scene.get("entities", [])
    manifest = {"schema_version": SCHEMA_VERSION, "entities": []}
    seen = set()
    for entity in sorted(entities, key=lambda item: item["id"]):
        entity_id = entity["id"]
        if not isinstance(entity_id, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", entity_id):
            raise ValueError(f"invalid entity id: {entity_id!r}")
        if entity_id in seen:
            raise ValueError(f"duplicate entity id: {entity_id}")
        seen.add(entity_id)
        entity_path = Path("entities") / f"{entity_id}.json"
        write_atomic(output / entity_path, canonical_bytes(entity))
        manifest["entities"].append({"id": entity_id, "file": entity_path.as_posix(), "sha256": sha256(canonical_bytes(entity))})
    write_atomic(output / "scene.json", canonical_bytes(manifest))
    return manifest


def structural_diff(before: Any, after: Any, path: str = "") -> list[dict[str, Any]]:
    changes = []
    if type(before) is not type(after):
        return [{"op": "replace", "path": path or "/", "value": after}]
    if isinstance(before, dict):
        for key in sorted(before.keys() - after.keys()):
            escaped = key.replace("~", "~0").replace("/", "~1")
            changes.append({"op": "remove", "path": f"{path}/{escaped}"})
        for key in sorted(after.keys() - before.keys()):
            escaped = key.replace("~", "~0").replace("/", "~1")
            changes.append({"op": "add", "path": f"{path}/{escaped}", "value": after[key]})
        for key in sorted(before.keys() & after.keys()):
            escaped = key.replace("~", "~0").replace("/", "~1")
            changes.extend(structural_diff(before[key], after[key], f"{path}/{escaped}"))
    elif isinstance(before, list):
        if before != after:
            changes.append({"op": "replace", "path": path or "/", "value": after})
    elif before != after:
        changes.append({"op": "replace", "path": path or "/", "value": after})
    return changes


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__)
    commands = root.add_subparsers(dest="command", required=True)
    reflect = commands.add_parser("reflect")
    reflect.add_argument("source", type=Path)
    reflect.add_argument("--output", type=Path, required=True)
    reflect.add_argument("--clang", default=os.environ.get("CXX", "clang++"))
    reflect.add_argument("--clang-arg", action="append", default=[])
    for name in ("validate", "import", "cook"):
        command = commands.add_parser(name)
        command.add_argument("inputs", nargs="+", type=Path)
        command.add_argument("--ddc", type=Path, required=True)
        command.add_argument("--output", type=Path)
    diff = commands.add_parser("diff")
    diff.add_argument("before", type=Path)
    diff.add_argument("after", type=Path)
    diff.add_argument("--output", type=Path, required=True)
    externalize = commands.add_parser("externalize")
    externalize.add_argument("scene", type=Path)
    externalize.add_argument("--output", type=Path, required=True)
    commands.add_parser("_worker")
    return root


def main() -> int:
    args = parser().parse_args()
    try:
        if args.command == "_worker":
            print(json.dumps(worker_request(json.load(sys.stdin))))
            return 0
        if args.command == "reflect":
            write_atomic(args.output, canonical_bytes(generate_reflection(args.source, args.clang, args.clang_arg)))
            return 0
        if args.command == "diff":
            changes = structural_diff(json.loads(args.before.read_text()), json.loads(args.after.read_text()))
            write_atomic(args.output, canonical_bytes({"schema_version": SCHEMA_VERSION, "changes": changes}))
            return 0
        if args.command == "externalize":
            externalize_scene(json.loads(args.scene.read_text()), args.output)
            return 0
        failures = []
        results = []
        cache = DerivedDataCache(args.ddc)
        for source in sorted(args.inputs, key=lambda path: path.as_posix()):
            if not source.is_file():
                failures.append({"source": str(source), "error": "not a file"})
                continue
            if args.command == "validate":
                try:
                    json.loads(source.read_text())
                    results.append({"source": str(source), "valid": True})
                except (UnicodeDecodeError, json.JSONDecodeError) as error:
                    failures.append({"source": str(source), "error": str(error)})
            else:
                result = isolated_import(source, cache, {})
                (results if result["ok"] else failures).append({"source": str(source), **result})
        report = {"schema_version": SCHEMA_VERSION, "command": args.command, "ok": not failures, "results": results, "failures": failures}
        if args.output:
            write_atomic(args.output, canonical_bytes(report))
        else:
            sys.stdout.buffer.write(canonical_bytes(report))
        return 0 if not failures else 1
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
