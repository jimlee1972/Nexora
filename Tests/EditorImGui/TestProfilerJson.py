"""Parse an actual C++ Profiler export with an independent, strict JSON consumer."""

import decimal
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
        require(key not in result, f"duplicate JSON key: {key}")
        result[key] = value
    return result


def reject_constant(value):
    raise RuntimeError(f"non-finite JSON number: {value}")


with tempfile.TemporaryDirectory(prefix="nexora-profiler-json-") as temporary:
    output = Path(temporary) / "capture.json"
    subprocess.run([sys.argv[1], str(output)], check=True, timeout=30)
    capture = json.loads(output.read_text(encoding="utf-8"),
                         parse_float=decimal.Decimal, parse_constant=reject_constant,
                         object_pairs_hook=unique_object)

require(set(capture) == {"schema", "source", "metric", "scope", "unit", "project_uuid",
                        "sample_count", "older_frames_dropped", "gpu_timing_available",
                        "memory_measurement_available", "samples"}, "unexpected schema fields")
require(type(capture["schema"]) is int and capture["schema"] == 1, "unsupported schema")
require(capture["source"] == "NexoraEditor", "incorrect measurement source")
require(capture["metric"] == "editor_frame_processing_wall_ms", "incorrect metric")
require(capture["scope"] == "after_begin_frame_before_present", "incorrect timing scope")
require(capture["unit"] == "milliseconds", "incorrect timing unit")
require(uuid.UUID(capture["project_uuid"]).int != 0, "missing project identity")
require(capture["sample_count"] == 3 and len(capture["samples"]) == 3, "incorrect sample count")
require(capture["older_frames_dropped"] == "18446744073709551615", "drop count lost precision")
require(capture["gpu_timing_available"] is False and
        capture["memory_measurement_available"] is False, "unobserved metrics advertised")
expected_frames = ["9007199254740993", "9007199254740994", "18446744073709551615"]
expected_times = [decimal.Decimal("1.2345678901234567"),
                  decimal.Decimal("2.2250738585072014e-308"),
                  decimal.Decimal("1.7976931348623157e+308")]
for sample, frame, wall_time in zip(capture["samples"], expected_frames, expected_times):
    require(set(sample) == {"frame", "frame_processing_wall_ms", "gpu_ms", "memory_bytes"},
            "unexpected sample fields")
    require(sample["frame"] == frame, "64-bit frame ID lost precision")
    require(sample["frame_processing_wall_ms"] == wall_time, "double timing precision lost")
    require(sample["gpu_ms"] is None and sample["memory_bytes"] is None,
            "unavailable metric was serialized as measured data")
print("Profiler schema-1 JSON interchange contracts passed")
