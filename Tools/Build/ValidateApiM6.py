#!/usr/bin/env python3
"""Validate API-M6 metadata, ABI baseline, canonical declarations, and Zig mirror."""

import json
import pathlib
import re
import sys


root = pathlib.Path(sys.argv[1])
manifest = json.loads((root / "Engine/API/abi_manifest.json").read_text(encoding="utf-8"))
baseline = json.loads((root / "Engine/API/abi_baseline_v3.json").read_text(encoding="utf-8"))
header = (root / manifest["canonical_header"]).read_text(encoding="utf-8")
zig = (root / "Bindings/Zig/nexora.zig").read_text(encoding="utf-8")

required_metadata = {"name", "since", "ownership", "nullability", "threading", "errors", "determinism"}
exports = manifest.get("exports", [])
names = [entry.get("name") for entry in exports]
if not exports or len(names) != len(set(names)):
    raise SystemExit("API-M6 manifest exports must be present and unique")
for entry in exports:
    missing = required_metadata - entry.keys()
    if missing:
        raise SystemExit(f"{entry.get('name', '<unnamed>')} lacks metadata: {sorted(missing)}")
    symbol = entry["name"].split(".")[-1]
    if symbol not in header:
        raise SystemExit(f"manifest export is absent from canonical header: {entry['name']}")

if manifest["abi"]["major"] != baseline["major"]:
    raise SystemExit("ABI manifest major differs from compatibility baseline")

for structure, expected_fields in baseline["structures"].items():
    match = re.search(rf"typedef struct {structure} \{{(.*?)\}} {structure};", header, re.DOTALL)
    if match is None:
        raise SystemExit(f"baseline structure is absent: {structure}")
    body = match.group(1)
    positions = [body.find(field) for field in expected_fields]
    if any(position < 0 for position in positions) or positions != sorted(positions):
        raise SystemExit(f"breaking field removal/reorder in {structure}; increment ABI major")
    zig_name = structure.removeprefix("Nexora")
    zig_match = re.search(rf"pub const {zig_name} = extern struct \{{(.*?)\n\}};", zig, re.DOTALL)
    if zig_match is None:
        raise SystemExit(f"Zig wrapper is missing {zig_name}")
    zig_body = zig_match.group(1)
    zig_positions = [zig_body.find(field) for field in expected_fields]
    if any(position < 0 for position in zig_positions) or zig_positions != sorted(zig_positions):
        raise SystemExit(f"Zig wrapper field order differs for {zig_name}")

print(f"API-M6 ABI {baseline['major']} validated: {len(exports)} documented exports")
