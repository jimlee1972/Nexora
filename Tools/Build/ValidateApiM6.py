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
# The canonical header includes separately documented service declarations.
header += "\n" + "\n".join(p.read_text(encoding="utf-8") for p in sorted((root / "Engine/API/include/nexora").glob("gameplay_*.h")))
services = json.loads((root / "Engine/API/abi_baseline_services_v1.json").read_text(encoding="utf-8"))

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

extension = json.loads((root / "Engine/API/abi_baseline_twod_v1.json").read_text(encoding="utf-8"))
structures = dict(baseline["structures"])
structures.update(extension["structures"])
pointer_extension = json.loads((root / "Engine/API/abi_baseline_pointer_v1.json").read_text(encoding="utf-8"))
structures.update(pointer_extension["structures"])
structures["NexoraGameplayHostV3"] = structures["NexoraGameplayHostV3"] + pointer_extension["host_suffix"]
structures.update(services["structures"])
structures["NexoraGameplayHostV3"] += services["host_suffix"]
display = json.loads((root / "Engine/API/abi_baseline_display_v1.json").read_text(encoding="utf-8"))
structures.update(display["structures"])
structures["NexoraGameplayServicesApiV1"] = structures["NexoraGameplayServicesApiV1"] + display["services_suffix"]
services["zig_names"].update(display["zig_names"])
ui_draw = json.loads((root / "Engine/API/abi_baseline_ui_draw_v1.json").read_text(encoding="utf-8"))
structures.update(ui_draw["structures"])
structures["NexoraGameplayServicesApiV1"] += ui_draw["services_suffix"]
services["zig_names"].update(ui_draw["zig_names"])
assets = json.loads((root / "Engine/API/abi_baseline_assets_v1.json").read_text(encoding="utf-8"))
structures.update(assets["structures"])
structures["NexoraGameplayServicesApiV1"] += assets["services_suffix"]
services["zig_names"].update(assets["zig_names"])
audio_state = json.loads((root / "Engine/API/abi_baseline_audio_state_v1.json").read_text(encoding="utf-8"))
structures.update(audio_state["structures"])
structures["NexoraGameplayServicesApiV1"] += audio_state["services_suffix"]
services["zig_names"].update(audio_state["zig_names"])
for structure, expected_fields in structures.items():
    match = re.search(rf"typedef struct {structure} \{{(.*?)\}} {structure};", header, re.DOTALL)
    if match is None:
        raise SystemExit(f"baseline structure is absent: {structure}")
    body = match.group(1)
    for callback in re.findall(r"\(\*(\w+)\)", body):
        if f"{structure}.{callback}" not in names:
            raise SystemExit(f"baseline callback lacks API-M6 metadata: {structure}.{callback}")
    positions = [(m.start() if (m := re.search(rf"\b{re.escape(field)}\b", body)) else -1) for field in expected_fields]
    if any(position < 0 for position in positions) or positions != sorted(positions):
        raise SystemExit(f"breaking field removal/reorder in {structure}; increment ABI major")
    zig_name = services["zig_names"].get(structure, structure.removeprefix("Nexora"))
    zig_source = zig
    if "." in zig_name:
        module, zig_name = zig_name.split(".")
        zig_source = (root / f"Bindings/Zig/{module}.zig").read_text(encoding="utf-8")
    zig_match = re.search(rf"pub const {zig_name} = extern struct \{{(.*?)\}};", zig_source, re.DOTALL)
    if zig_match is None:
        raise SystemExit(f"Zig wrapper is missing {zig_name}")
    zig_body = zig_match.group(1)
    zig_positions = [(m.start() if (m := re.search(rf"\b{re.escape(field)}\b", zig_body)) else -1) for field in expected_fields]
    if any(position < 0 for position in zig_positions) or zig_positions != sorted(zig_positions):
        raise SystemExit(f"Zig wrapper field order differs for {zig_name}")

print(f"API-M6 ABI {baseline['major']} validated: {len(exports)} documented exports")
