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

    shader_inputs = [
        MODULE.artifact_address(b"shader-source"),
        MODULE.artifact_address(b"common-include"),
    ]
    shader_key = MODULE.derivation_key(
        "shader", "slang", "2026.9", shader_inputs,
        {"profile": "shipping", "target": "spirv"})
    assert shader_key == "420394c73fa1e3ec71f88ac3c7d62be76a7bd4710a688a15f985bad7bcab4dfe"
    assert MODULE.distributed_work_unit(
        "shader", "slang", "2026.9", list(reversed(shader_inputs)),
        {"target": "spirv", "profile": "shipping"})["work_id"] == shader_key
    try:
        MODULE.distributed_work_unit("unknown", "tool", "1", shader_inputs, {})
        raise AssertionError("unsupported distributed work kind was accepted")
    except ValueError:
        pass

    remote_store = {}
    def remote_put(key, address, payload):
        remote_store[key] = (address, payload)
    def remote_get(key):
        return remote_store.get(key)
    shared_local = MODULE.DerivedDataCache(root / "shared-local")
    shared = MODULE.SharedDerivedDataCache(shared_local, remote_get, remote_put)
    shared_payload = b"shared-ddc-artifact"
    shared.store(shader_key, shared_payload)
    assert remote_store[shader_key] == (MODULE.artifact_address(shared_payload), shared_payload)
    cold = MODULE.SharedDerivedDataCache(MODULE.DerivedDataCache(root / "cold-local"), remote_get)
    assert cold.load(shader_key) == shared_payload
    assert cold.local.load(shader_key) == shared_payload

    def unavailable(_key):
        raise OSError("remote DDC unavailable")
    offline = MODULE.SharedDerivedDataCache(MODULE.DerivedDataCache(root / "offline-local"),
                                            unavailable)
    offline.local.store(shader_key, shared_payload)
    assert offline.load(shader_key) == shared_payload
    missing_offline = MODULE.SharedDerivedDataCache(
        MODULE.DerivedDataCache(root / "missing-offline"), unavailable)
    assert missing_offline.load(shader_key) is None

    corrupt_remote = MODULE.SharedDerivedDataCache(
        MODULE.DerivedDataCache(root / "corrupt-local"),
        lambda _key: ("sha256:" + "0" * 64, b"tampered"))
    try:
        corrupt_remote.load(shader_key)
        raise AssertionError("corrupt shared DDC payload was accepted")
    except RuntimeError:
        pass

    patch_payloads = {
        "data/balance.json": b'{"damage":7}\n',
        "loc/zh-TW.pack": "開始\n".encode(),
    }
    patch_manifest = {
        "schema_version": MODULE.ARTIFACT_PROTOCOL_VERSION,
        "generation": 2,
        "entries": [
            {"path": path, "kind": kind, "size": len(patch_payloads[path]),
             "sha256": MODULE.sha256(patch_payloads[path])}
            for path, kind in [
                ("data/balance.json", "data-overlay"),
                ("loc/zh-TW.pack", "localization-pack"),
            ]
        ],
    }
    generations = MODULE.GenerationRegistry(1)
    pinned = generations.pin()
    transaction = MODULE.PatchVerificationTransaction(patch_manifest, patch_payloads)
    assert not transaction.activate(generations)
    assert transaction.verify() and transaction.activate(generations)
    assert generations.active_generation == 2 and generations.available(1)
    assert not generations.drain(1)
    assert generations.release(pinned) and generations.drain(1)
    assert not generations.available(1)

    native_payload = b"\x7fELF" + b"\0" * 32
    native_manifest = {
        "schema_version": MODULE.ARTIFACT_PROTOCOL_VERSION,
        "generation": 3,
        "entries": [{
            "path": "content/plugin.bin",
            "kind": "asset",
            "size": len(native_payload),
            "sha256": MODULE.sha256(native_payload),
        }],
    }
    native_transaction = MODULE.PatchVerificationTransaction(
        native_manifest, {"content/plugin.bin": native_payload})
    assert not native_transaction.verify()
    assert "native executable" in native_transaction.error

    traversal_manifest = json.loads(json.dumps(patch_manifest))
    traversal_manifest["generation"] = 3
    traversal_manifest["entries"][0]["path"] = "../escape.json"
    traversal_transaction = MODULE.PatchVerificationTransaction(traversal_manifest, patch_payloads)
    assert not traversal_transaction.verify()

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
