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

    report = root / "report.json"
    completed = subprocess.run([sys.executable, str(MODULE_PATH), "cook", str(source),
                                "--ddc", str(root / "command-ddc"), "--output", str(report)], check=False)
    assert completed.returncode == 0
    assert json.loads(report.read_text())["ok"]
