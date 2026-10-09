"""Read an actual native GPU export with an independent strict JSON implementation."""
import json
import math
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
    raise RuntimeError(f"nonfinite JSON: {value}")


with tempfile.TemporaryDirectory(prefix="nexora-gpu-json-") as temporary:
    output = Path(temporary) / "capture.json"
    subprocess.run([sys.argv[1], str(output)], check=True, timeout=30)
    capture = json.loads(output.read_text(encoding="utf-8"), object_pairs_hook=unique_object,
                         parse_constant=reject_constant)
require(set(capture) == {"schema", "source", "metric", "scope", "unit", "export_project_uuid",
                         "sample_count", "older_samples_dropped", "timing_source",
                         "software_rasterizer", "sequence_axis", "samples"}, "unexpected fields")
require(type(capture["schema"]) is int and capture["schema"] == 1, "wrong schema")
require(capture["source"] == "NexoraEditor" and capture["metric"] == "completed_gpu_timing", "wrong source/metric")
require(capture["scope"] == "native_command_buffer_interval" and capture["unit"] == "milliseconds", "wrong scope/unit")
require(capture["timing_source"] == "vulkan_timestamps" and capture["software_rasterizer"] is True, "wrong native provenance")
require(capture["sequence_axis"] == "native_completed_submission_id", "fake time axis")
require(uuid.UUID(capture["export_project_uuid"]).int != 0, "missing export destination")
require(capture["sample_count"] == len(capture["samples"]) == 3, "wrong count")
require(capture["older_samples_dropped"] == "18446744073709551615", "lost drop precision")
for sample, submission, measured in zip(capture["samples"],
        ["9007199254740993", "9007199254740994", "18446744073709551615"], [0, None, 250.12345678901234]):
    require(set(sample) == {"submission", "milliseconds"}, "wrong sample fields")
    require(sample["submission"] == submission, "lost uint64 precision")
    require(sample["milliseconds"] == measured, "lost measured/null distinction or precision")
    require(measured is None or math.isfinite(measured), "nonfinite native timing")
print("Native GPU schema-1 interchange passed")
