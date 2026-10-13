"""Validate an actual C++ mesh-memory export using an independent JSON reader."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import uuid


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, f"duplicate key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise RuntimeError(f"nonfinite number: {value}")


with tempfile.TemporaryDirectory(prefix="nexora-memory-json-") as temporary:
    output = Path(temporary) / "capture.json"
    subprocess.run([sys.argv[1], str(output)], check=True, timeout=30)
    capture = json.loads(output.read_text(encoding="utf-8"),
                         object_pairs_hook=unique_object, parse_constant=reject_constant)

require(set(capture) == {"schema", "source", "metric", "scope", "unit", "export_project_uuid",
                        "sample_count", "older_samples_dropped", "time_unit", "time_origin",
                        "samples", "cpu_scope"}, "unsupported fields")
require(type(capture["schema"]) is int and capture["schema"] == 1, "unsupported version")
require(capture["source"] == "NexoraEditor", "wrong source")
require(capture["metric"] == "retained_mesh_memory", "wrong metric")
require(capture["scope"] == "current_native_surface_retained_geometry", "wrong scope")
require(capture["cpu_scope"] == "owned_comparison_key_payload_excluding_allocator_overhead", "wrong CPU scope")
require(capture["unit"] == "bytes" and capture["time_unit"] == "milliseconds", "wrong units")
require(capture["time_origin"] == "first_observation_since_clear_or_domain_change", "wrong origin")
require(uuid.UUID(capture["export_project_uuid"]).int != 0, "missing export destination")
require(capture["sample_count"] == 3 and len(capture["samples"]) == 3, "wrong sample count")
require(capture["older_samples_dropped"] == "18446744073709551615", "lost drop precision")
for sample, sequence, elapsed, resident, cpu in zip(
        capture["samples"], ["9007199254740993", "9007199254740994", "18446744073709551615"],
        [0, 250.12345678901234, 1000], ["0", None, "18446744073709551615"], ["0", None, "9007199254740993"]):
    require(set(sample) == {"sequence", "elapsed_ms", "native_allocation_bytes", "cpu_key_payload_bytes"}, "wrong sample fields")
    require(sample["sequence"] == sequence, "lost sequence precision")
    require(sample["elapsed_ms"] == elapsed, "lost elapsed precision")
    require(sample["native_allocation_bytes"] == resident, "lost bytes/unavailable distinction")
    require(sample["cpu_key_payload_bytes"] == cpu, "lost CPU bytes/unavailable distinction")
print("Mesh-memory schema-1 interchange passed")
