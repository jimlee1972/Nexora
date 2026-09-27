#!/usr/bin/env python3
"""Deterministic V2 production metadata, import, and cook commandlet."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import secrets
import subprocess
import sys
from pathlib import Path, PurePosixPath
from typing import Any


SCHEMA_VERSION = 1


def canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode()


def write_atomic(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    # A random suffix (not just the PID) keeps two threads in the same
    # process from racing on the same temp filename before the atomic
    # rename below.
    temporary = path.with_name(f".{path.name}.{os.getpid()}.{secrets.token_hex(4)}.tmp")
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

    def path(self, key: str) -> Path:
        return self.root / key[:2] / key[2:]

    def load(self, key: str) -> bytes | None:
        path = self.path(key)
        return path.read_bytes() if path.is_file() else None

    def store(self, key: str, artifact: bytes) -> Path:
        path = self.path(key)
        if path.exists() and path.read_bytes() != artifact:
            raise RuntimeError(f"DDC collision for {key}")
        if not path.exists():
            write_atomic(path, artifact)
        return path



ARTIFACT_PROTOCOL_VERSION = 1
DISTRIBUTED_WORK_KINDS = {"shader", "hlod", "cook"}
REMOTE_CONTENT_KINDS = {"asset", "data-overlay", "localization-pack", "media"}
NATIVE_CONTENT_EXTENSIONS = {".exe", ".dll", ".so", ".dylib", ".app", ".apk", ".ipa"}
NATIVE_MAGICS = (
    b"MZ",
    b"\x7fELF",
    bytes.fromhex("feedface"),
    bytes.fromhex("feedfacf"),
    bytes.fromhex("cefaedfe"),
    bytes.fromhex("cffaedfe"),
    bytes.fromhex("cafebabe"),
    bytes.fromhex("bebafeca"),
)


def artifact_address(payload: bytes) -> str:
    return f"sha256:{sha256(payload)}"


def derivation_key(kind: str, tool: str, tool_version: str,
                   input_addresses: list[str], settings: dict[str, Any]) -> str:
    if not kind or not tool or not tool_version:
        raise ValueError("artifact derivation identity must be non-empty")
    if any(not address.startswith("sha256:") or len(address) != 71 for address in input_addresses):
        raise ValueError("artifact inputs must use sha256 content addresses")
    descriptor = {
        "protocol_version": ARTIFACT_PROTOCOL_VERSION,
        "kind": kind,
        "tool": tool,
        "tool_version": tool_version,
        "inputs": sorted(input_addresses),
        "settings": settings,
    }
    return sha256(canonical_bytes(descriptor))


def distributed_work_unit(kind: str, tool: str, tool_version: str,
                          input_addresses: list[str], settings: dict[str, Any]) -> dict[str, Any]:
    if kind not in DISTRIBUTED_WORK_KINDS:
        raise ValueError(f"unsupported distributed work kind: {kind}")
    return {
        "schema_version": ARTIFACT_PROTOCOL_VERSION,
        "kind": kind,
        "work_id": derivation_key(kind, tool, tool_version, input_addresses, settings),
        "inputs": sorted(input_addresses),
        "settings": settings,
    }


class SharedDerivedDataCache:
    """Local-first DDC with an optional shared backend and verified fill."""

    def __init__(self, local: DerivedDataCache, remote_get=None, remote_put=None):
        self.local = local
        self.remote_get = remote_get
        self.remote_put = remote_put

    def load(self, key: str) -> bytes | None:
        local = self.local.load(key)
        if local is not None:
            return local
        if self.remote_get is None:
            return None
        try:
            remote = self.remote_get(key)
        except (OSError, RuntimeError):
            return None
        if remote is None:
            return None
        address, payload = remote
        if address != artifact_address(payload):
            raise RuntimeError(f"remote DDC integrity failure for {key}")
        self.local.store(key, payload)
        return payload

    def store(self, key: str, artifact: bytes) -> Path:
        path = self.local.store(key, artifact)
        if self.remote_put is not None:
            try:
                self.remote_put(key, artifact_address(artifact), artifact)
            except (OSError, RuntimeError):
                pass
        return path


def contains_native_code(path: str, payload: bytes) -> bool:
    suffix = PurePosixPath(path).suffix.lower()
    return suffix in NATIVE_CONTENT_EXTENSIONS or any(payload.startswith(magic) for magic in NATIVE_MAGICS)


def _safe_patch_path(value: str) -> bool:
    if not value or "\\" in value or re.match(r"^[A-Za-z]:", value):
        return False
    path = PurePosixPath(value)
    return not path.is_absolute() and ".." not in path.parts and "." not in path.parts


class GenerationRegistry:
    def __init__(self, active_generation: int):
        if active_generation <= 0:
            raise ValueError("active generation must be positive")
        self.active_generation = active_generation
        self._retired: set[int] = set()
        self._pins: dict[int, int] = {active_generation: 0}

    def pin(self, generation: int | None = None) -> int:
        generation = self.active_generation if generation is None else generation
        if generation != self.active_generation and generation not in self._retired:
            raise ValueError("generation is not available")
        self._pins[generation] = self._pins.get(generation, 0) + 1
        return generation

    def release(self, generation: int) -> bool:
        count = self._pins.get(generation, 0)
        if count <= 0:
            return False
        self._pins[generation] = count - 1
        return True

    def activate(self, generation: int) -> bool:
        if generation <= self.active_generation:
            return False
        previous = self.active_generation
        self._retired.add(previous)
        self._pins.setdefault(previous, 0)
        self.active_generation = generation
        self._pins.setdefault(generation, 0)
        return True

    def drain(self, generation: int) -> bool:
        if generation not in self._retired or self._pins.get(generation, 0) != 0:
            return False
        self._retired.remove(generation)
        self._pins.pop(generation, None)
        return True

    def available(self, generation: int) -> bool:
        return generation == self.active_generation or generation in self._retired


class PatchVerificationTransaction:
    """Verifies a complete patch before allowing generation activation."""

    def __init__(self, manifest: dict[str, Any], payloads: dict[str, bytes]):
        self.manifest = manifest
        self.payloads = payloads
        self.verified = False
        self.activated = False
        self.error = ""
        self._verified_generation: int | None = None

    def verify(self) -> bool:
        try:
            if self.manifest.get("schema_version") != ARTIFACT_PROTOCOL_VERSION:
                raise ValueError("unsupported patch manifest version")
            generation = self.manifest.get("generation")
            if not isinstance(generation, int) or generation <= 0:
                raise ValueError("invalid patch generation")
            entries = self.manifest.get("entries")
            if not isinstance(entries, list) or not entries:
                raise ValueError("patch manifest requires entries")
            seen: set[str] = set()
            for entry in entries:
                path = entry.get("path")
                kind = entry.get("kind")
                if not isinstance(path, str) or not _safe_patch_path(path) or path in seen:
                    raise ValueError(f"unsafe or duplicate patch path: {path!r}")
                seen.add(path)
                if kind not in REMOTE_CONTENT_KINDS:
                    raise ValueError(f"remote content kind is not allowed: {kind!r}")
                payload = self.payloads.get(path)
                if payload is None:
                    raise ValueError(f"missing patch payload: {path}")
                if contains_native_code(path, payload):
                    raise ValueError(f"native executable content rejected: {path}")
                if entry.get("size") != len(payload) or entry.get("sha256") != sha256(payload):
                    raise ValueError(f"patch payload verification failed: {path}")
            self.verified = True
            self._verified_generation = generation
            self.error = ""
            return True
        except (TypeError, ValueError) as error:
            self.verified = False
            self._verified_generation = None
            self.error = str(error)
            return False

    def activate(self, generations: GenerationRegistry) -> bool:
        if not self.verified or self.activated or self._verified_generation is None:
            return False
        if not generations.activate(self._verified_generation):
            return False
        self.activated = True
        return True


def worker_request(request: dict[str, Any]) -> dict[str, Any]:
    source_bytes = bytes.fromhex(request["source_hex"])
    if request.get("settings", {}).get("simulate_crash"):
        os._exit(70)
    artifact = canonical_bytes({
        "schema_version": SCHEMA_VERSION,
        "source_sha256": sha256(source_bytes),
        "settings": request.get("settings", {}),
    })
    return {"artifact_hex": artifact.hex()}


def isolated_import(source: Path, ddc: DerivedDataCache, settings: dict[str, Any]) -> dict[str, Any]:
    # Read the source exactly once and hand the worker those same bytes
    # (rather than a path it re-reads independently), so the artifact's
    # embedded source_sha256 and the DDC cache key it is stored under always
    # describe the identical byte snapshot even if the file on disk is
    # concurrently modified between the two -- the tool's own contract is
    # that independent commandlets may run concurrently.
    try:
        source_bytes = source.read_bytes()
    except OSError as error:
        return {"ok": False, "error": str(error)}
    request = {"source_hex": source_bytes.hex(), "settings": settings}
    process = subprocess.run(
        [sys.executable, str(Path(__file__).resolve()), "_worker"],
        input=json.dumps(request), capture_output=True, text=True, check=False,
    )
    if process.returncode:
        return {"ok": False, "error": f"import worker exited with code {process.returncode}"}
    response = json.loads(process.stdout)
    artifact = bytes.fromhex(response["artifact_hex"])
    key = ddc.key(source_bytes, "raw", "1", settings)
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


def build_world_partition(world: dict[str, Any], leaf_size: float, split_threshold: int,
                          previous: dict[str, Any] | None = None,
                          changed_regions: set[str] | None = None) -> dict[str, Any]:
    """Build deterministic, region-addressed partition metadata without loading a whole world."""
    if leaf_size <= 0 or split_threshold <= 0:
        raise ValueError("leaf size and split threshold must be positive")
    regions = world.get("regions", [])
    seen: set[str] = set()
    prior = {item["id"]: item for item in (previous or {}).get("regions", [])}
    output = []
    for region in sorted(regions, key=lambda item: item["id"]):
        region_id = region.get("id")
        if not isinstance(region_id, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", region_id):
            raise ValueError(f"invalid region id: {region_id!r}")
        if region_id in seen:
            raise ValueError(f"duplicate region id: {region_id}")
        seen.add(region_id)
        if changed_regions is not None and region_id not in changed_regions and region_id in prior:
            output.append(prior[region_id])
            continue
        cells: dict[tuple[int, int, int], list[str]] = {}
        for item in sorted(region.get("items", []), key=lambda entry: entry["id"]):
            position = item.get("position", [])
            if len(position) != 3:
                raise ValueError(f"item {item.get('id')!r} requires a 3D position")
            coordinate = tuple(int(value // leaf_size) for value in position)
            cells.setdefault(coordinate, []).append(str(item["id"]))
        cell_records = [{"coordinate": list(coordinate), "items": items}
                        for coordinate, items in sorted(cells.items())]
        descriptor = {"id": region_id, "cells": cell_records,
                      "split_threshold": split_threshold}
        descriptor["hash"] = sha256(canonical_bytes(descriptor))
        output.append(descriptor)
    result = {"schema_version": SCHEMA_VERSION, "kind": "world-partition", "regions": output}
    result["build_hash"] = sha256(canonical_bytes(result))
    return result


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
    partition = commands.add_parser("world-partition")
    partition.add_argument("world", type=Path)
    partition.add_argument("--output", type=Path, required=True)
    partition.add_argument("--leaf-size", type=float, default=100.0)
    partition.add_argument("--split-threshold", type=int, default=8)
    partition.add_argument("--previous", type=Path)
    partition.add_argument("--changed-region", action="append")
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
        if args.command == "world-partition":
            previous = json.loads(args.previous.read_text()) if args.previous else None
            changed = set(args.changed_region) if args.changed_region is not None else None
            result = build_world_partition(json.loads(args.world.read_text()), args.leaf_size,
                                           args.split_threshold, previous, changed)
            write_atomic(args.output, canonical_bytes(result))
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
