"""Bounded owning diagnostics for the private Windows Build workflow fixture."""
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import time

MAX_SETTINGS_BYTES = 256 * 1024
MAX_LOG_BYTES = 4 * 1024 * 1024
MAX_INPUTS = 2048
MAX_NATIVE_EVENTS = 8192


def _stat(path):
    value = path.stat()
    return {"size": value.st_size, "mtime_ns": value.st_mtime_ns, "inode": value.st_ino}


def _write_json(path, value):
    pending = path.with_name(path.name + ".pending")
    pending.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")
    os.replace(pending, path)


def capture(evidence, label, project, log_path, inputs, native_events, window, phase, pid):
    """Copy fixture data before its process or temporary project can disappear."""
    if not re.fullmatch(r"[A-Za-z0-9_.-]+", label):
        raise ValueError("Unsafe fixture evidence label")
    evidence, project, log_path = Path(evidence), Path(project), Path(log_path)
    evidence.mkdir(parents=True, exist_ok=True)
    saved = {"path": str(project / "project.build.json"), "limit": MAX_SETTINGS_BYTES}
    try:
        before = _stat(project / "project.build.json")
        with (project / "project.build.json").open("rb") as source:
            data = source.read(MAX_SETTINGS_BYTES + 1)
        truncated = len(data) > MAX_SETTINGS_BYTES
        data = data[:MAX_SETTINGS_BYTES]
        after = _stat(project / "project.build.json")
        name = label + ".project.build.json"
        (evidence / name).write_bytes(data)
        saved.update(status="copied", evidence=name, bytes=len(data), truncated=truncated,
                     captured_sha256=hashlib.sha256(data).hexdigest(),
                     stat_before=before, stat_after=after, stable_metadata=before == after)
    except FileNotFoundError:
        saved.update(status="missing")
    except OSError as error:
        saved.update(status="unreadable", error=type(error).__name__, detail=str(error))

    log = {"path": str(log_path), "limit": MAX_LOG_BYTES}
    raw = b""
    try:
        with log_path.open("rb") as source:
            source.seek(0, os.SEEK_END)
            total = source.tell()
            offset = max(0, total - MAX_LOG_BYTES)
            source.seek(offset)
            raw = source.read(MAX_LOG_BYTES)
        log.update(status="read", offset=offset, bytes=len(raw),
                   captured_sha256=hashlib.sha256(raw).hexdigest())
    except OSError as error:
        log.update(status="unreadable", error=type(error).__name__, detail=str(error))

    records = {"drafts": [], "requests": [], "results": []}
    prefixes = {"game build draft ": "drafts", "game build request ": "requests",
                "game build settings result ": "results"}
    malformed, ui = [], []
    for line in raw.decode("utf-8", errors="replace").splitlines():
        if line.startswith("game build ui "):
            ui.append(dict(item.split("=", 1) for item in line[len("game build ui "):].split()
                           if "=" in item))
        for prefix, key in prefixes.items():
            if line.startswith(prefix):
                try:
                    records[key].append(json.loads(line[len(prefix):]))
                except json.JSONDecodeError:
                    malformed.append({"prefix": prefix, "text": line[:2048]})

    value = {"schema": "nexora.windows.build.diagnostics.v1", "phase": phase,
             "recorded_utc": datetime.now(timezone.utc).isoformat(), "monotonic": time.monotonic(),
             "editor_pid": pid, "window": window, "project": str(project),
             "saved_settings": saved, "log": log, "latest_ui": ui[-1:] or [],
             "inputs": inputs[-MAX_INPUTS:], "input_count": len(inputs),
             "inputs_truncated": len(inputs) > MAX_INPUTS,
             "native_events": native_events[-MAX_NATIVE_EVENTS:],
             "native_event_count": len(native_events),
             "native_events_truncated": len(native_events) > MAX_NATIVE_EVENTS,
             "observations": {key: rows[-128:] for key, rows in records.items()},
             "malformed_observations": malformed[-32:]}
    _write_json(evidence / (label + ".inputs.json"), inputs[-MAX_INPUTS:])
    _write_json(evidence / (label + ".diagnostics.json"), value)
    return value
