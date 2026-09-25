#!/usr/bin/env python3
import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("NexoraTool.py")
SPEC = importlib.util.spec_from_file_location("nexora_tool", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    header = root / "Types.hpp"
    header.write_text('struct [[clang::annotate("nexora:reflect")]] Camera { float zoom; int priority; };\n')
    first = MODULE.generate_reflection(header, "clang++", [])
    second = MODULE.generate_reflection(header, "clang++", [])
    assert first == second
    assert first["types"] == [{"name": "Camera", "fields": [
        {"name": "priority", "type": "int"}, {"name": "zoom", "type": "float"}]}]

    source = root / "asset.json"
    source.write_text('{"value": 7}\n')
    cache = MODULE.DerivedDataCache(root / "ddc")
    imported_a = MODULE.isolated_import(source, cache, {"quality": "high"})
    imported_b = MODULE.isolated_import(source, cache, {"quality": "high"})
    assert imported_a == imported_b and imported_a["ok"]
    assert len(imported_a["key"]) == 64 and len(imported_a["artifact_sha256"]) == 64
    renamed_source = root / "renamed-asset.json"
    renamed_source.write_bytes(source.read_bytes())
    imported_renamed = MODULE.isolated_import(renamed_source, cache, {"quality": "high"})
    assert imported_renamed["ok"]
    assert imported_renamed["key"] == imported_a["key"]
    assert imported_renamed["artifact"] == imported_a["artifact"]
    assert imported_renamed["artifact_sha256"] == imported_a["artifact_sha256"]
    crashed = MODULE.isolated_import(source, cache, {"simulate_crash": True})
    assert not crashed["ok"] and "code 70" in crashed["error"]

    scene = {"entities": [{"id": "b", "name": "Second"}, {"id": "a", "name": "First"}]}
    manifest = MODULE.externalize_scene(scene, root / "world")
    assert [item["id"] for item in manifest["entities"]] == ["a", "b"]
    assert (root / "world/entities/a.json").is_file()
    try:
        MODULE.externalize_scene({"entities": [{"id": "../escape"}]}, root / "invalid-world")
        raise AssertionError("unsafe entity id was accepted")
    except ValueError:
        pass
    assert MODULE.structural_diff({"x": 1}, {"x": 2, "y": 3}) == [
        {"op": "add", "path": "/y", "value": 3},
        {"op": "replace", "path": "/x", "value": 2},
    ]

    world = {"regions": [
        {"id": "west", "items": [{"id": "b", "position": [101, 2, 3]},
                                     {"id": "a", "position": [1, 2, 3]}]},
        {"id": "east", "items": [{"id": "c", "position": [201, 2, 3]}]},
    ]}
    partition_a = MODULE.build_world_partition(world, 100, 8)
    partition_b = MODULE.build_world_partition({"regions": list(reversed(world["regions"]))}, 100, 8)
    assert partition_a == partition_b
    changed_world = json.loads(json.dumps(world))
    changed_world["regions"][1]["items"][0]["position"][0] = 301
    incremental = MODULE.build_world_partition(changed_world, 100, 8, partition_a, {"east"})
    assert incremental["regions"][1] == partition_a["regions"][1]
    assert incremental["regions"][0] != partition_a["regions"][0]

    report = root / "report.json"
    completed = subprocess.run([sys.executable, str(MODULE_PATH), "cook", str(source),
                                "--ddc", str(root / "command-ddc"), "--output", str(report)], check=False)
    assert completed.returncode == 0
    assert json.loads(report.read_text())["ok"]
