#!/usr/bin/env python3
import importlib.util
import json
import tempfile
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("ScanV1Project.py")
SPEC = importlib.util.spec_from_file_location("scan_v1_project", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def write_json(root, relative, value):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    write_json(root, "Config/Modules/modules.json", {"schema_version": 1})
    write_json(root, "Engine/API/abi_manifest.json", {"abi": {"major": 3}})
    write_json(root, "CMakePresets.json", {"configurePresets": [
        {"name": "linux-development"}, {"name": "linux-shipping"}]})
    write_json(root, "Plugins/Example/plugin.json", {"schema_version": 1, "engine_abi": 3})
    first = MODULE.scan(root)
    second = MODULE.scan(root)
    assert first == second
    assert first["compatible"]
    assert len(first["baseline_fingerprint"]) == 64
    (root / "Engine/API/abi_manifest.json").unlink()
    broken = MODULE.scan(root)
    assert not broken["compatible"]
    assert broken["findings"][0]["code"] == "missing-required-file"
