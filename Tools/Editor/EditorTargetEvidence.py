#!/usr/bin/env python3
"""Record operator observations and validate ED-M0 target-host evidence completeness.

A complete record is an operator attestation, not an automated visual/IME certification.
This tool never injects crashes or modifies project data itself. Use a disposable project.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import platform
import re
import subprocess
import sys

SCHEMA = "nexora.editor.target_host.evidence.v1"
COMMON = {
    "host.native_backend": "Record the actual selected backend, GPU/driver and native validation diagnostics.",
    "shell.glyphs": "Verify readable Latin, CJK and supplementary text; record any missing glyphs.",
    "shell.selection": "Select Hierarchy nodes and verify the selected document node.",
    "shell.docking_restart": "Drag dock panels, restart this project, and verify restored positions.",
    "shell.typing_shortcuts": "Verify real typing and command shortcuts without duplicate text.",
    "shell.wheel_axes": "Verify horizontal and vertical wheel directions independently.",
    "shell.focus_loss": "Hold a key/button, change focus, return; verify no stuck input.",
    "shell.resize_minimize_close": "Resize, minimize, restore and close; verify readable output and no stale frame.",
    "recovery.recover": "On a disposable copy, fsync a valid journal, SIGKILL/terminate, relaunch and Recover using keyboard only. Compare committed data/journal and visible focus.",
    "recovery.discard": "Repeat on a separate disposable copy; explicitly Discard using keyboard only. Verify destructive wording and only the selected recovery data is discarded.",
    "recovery.failure_retry": "Retain a corrupt journal, observe an actionable modal error, and verify retry or safe exit preserves data.",
}
WINDOWS = {
    **{f"dpi.{scale}": f"At {scale}% verify crisp text, pointer hit targets, resize/minimize/restore and no cumulative style scaling." for scale in (100, 125, 150, 200)},
    "dpi.cross_monitor": "Move repeatedly between monitors with different DPI; verify same-frame extent/hit targets and font rebuild.",
    "ime.start_update_commit": "Use an installed IME in Hierarchy Rename: start/update composition and commit exactly once.",
    "ime.cancel": "Observe composition first, then cancel; verify no text was committed.",
    "ime.caret_move": "Move the input caret during composition; verify the candidate follows it.",
    "ime.candidate_dpi_a": "Record installed-IME candidate placement at the caret, including the actual DPI.",
    "ime.candidate_dpi_b": "Repeat at a different DPI/monitor and record the actual DPI.",
}


def scenarios(host):
    return {**COMMON, **(WINDOWS if host == "windows" else {})}


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(chunk)
    return value.hexdigest()


def retain(source, directory):
    source = source.expanduser().resolve(strict=True)
    if not source.is_file() or source.stat().st_size == 0:
        raise ValueError("Artifact must be a nonempty regular file")
    sha = digest(source)
    suffix = source.suffix.lower()
    relative = Path("artifacts") / (sha + (suffix if re.fullmatch(r"\.[a-z0-9]{1,10}", suffix) else ""))
    target = directory / relative
    target.parent.mkdir(exist_ok=True)
    if not target.exists():
        # Stream recordings without retaining them in process memory.
        import shutil
        shutil.copyfile(source, target)
    if digest(target) != sha:
        raise ValueError("Artifact changed while being retained; record it again")
    return {"path": relative.as_posix(), "sha256": sha}


def evaluate(record, directory):
    """Ignore caller-supplied status. Require exact scenario coverage and retained artifacts."""
    errors = []
    if not isinstance(record, dict):
        return "INCOMPLETE", ["Record must be an object"]
    host = record.get("platform")
    if record.get("schema") != SCHEMA or host not in ("linux", "windows"):
        return "INCOMPLETE", ["Unsupported schema/platform; legacy smoke records are not target acceptance"]
    for key in ("host_os", "gpu_driver", "monitors", "configuration", "recorded_utc"):
        if not isinstance(record.get(key), str) or record[key].strip().lower() in ("", "unknown", "unverified"):
            errors.append(f"Missing metadata: {key}")
    for key in ("commit", "executable_sha256"):
        if not isinstance(record.get(key), str) or not re.fullmatch(r"[0-9a-f]{40}" if key == "commit" else r"[0-9a-f]{64}", record[key]):
            errors.append(f"Invalid {key}")
    if record.get("window_backend") != ("x11" if host == "linux" else "win32"):
        errors.append("Missing or unsupported native window backend")
    try:
        stamp = datetime.fromisoformat(record.get("recorded_utc", ""))
        if stamp.utcoffset() is None:
            raise ValueError("Timestamp requires a timezone")
    except (TypeError, ValueError):
        errors.append("Invalid observation timestamp")
    if record.get("configuration") != "Development":
        errors.append("Graphical target acceptance requires Development configuration")
    if record.get("backend") not in (("vulkan",) if host == "linux" else ("dx12", "vulkan")):
        errors.append("Unsupported selected backend")
    if record.get("display_kind") != "physical" or record.get("operator_attested") is not True:
        errors.append("Physical display and explicit operator attestation required")
    command = record.get("command")
    if not isinstance(command, list) or not command or not all(isinstance(arg, str) and arg for arg in command):
        errors.append("Missing exact Editor argv")
    elif "--graphical" not in command or not any(arg.startswith("--project=") for arg in command):
        errors.append("Command must identify graphical mode and disposable project")
    rows = record.get("rows")
    if not isinstance(rows, list):
        return "INCOMPLETE", errors + ["Missing scenario rows"]
    expected = {"smoke.bounded_frames", *scenarios(host)}
    seen = set()
    candidate_scales = []
    failed = False
    for row in rows:
        if not isinstance(row, dict):
            errors.append("Invalid row")
            continue
        scenario = row.get("scenario")
        if not isinstance(scenario, str) or scenario not in expected or scenario in seen:
            errors.append("Unknown or duplicate scenario")
            continue
        seen.add(scenario)
        if host == "windows" and (scenario in {f"dpi.{scale}" for scale in (100, 125, 150, 200)}
                                   or scenario.startswith("ime.candidate_dpi_")):
            scale = row.get("scale_percent")
            if type(scale) is not int or not 50 <= scale <= 800:
                errors.append(f"{scenario}: missing actual DPI percentage")
            elif scenario.startswith("dpi.") and scale != int(scenario.split(".")[1]):
                errors.append(f"{scenario}: observed DPI does not match required scale")
            elif scenario.startswith("ime."):
                candidate_scales.append(scale)
        result = row.get("result")
        failed |= result == "fail"
        if result != "pass":
            errors.append(f"{scenario}: {result}")
        if not isinstance(row.get("notes"), str) or not row["notes"].strip():
            errors.append(f"{scenario}: missing observation")
        artifacts = row.get("artifacts")
        if not isinstance(artifacts, list) or not artifacts:
            errors.append(f"{scenario}: missing artifact")
            continue
        for item in artifacts:
            try:
                relative = Path(item["path"])
                if relative.is_absolute() or ".." in relative.parts:
                    raise ValueError("Artifact must be retained inside record directory")
                path = (directory / relative).resolve(strict=True)
                path.relative_to(directory.resolve())
                if not path.is_file() or not path.stat().st_size or digest(path) != item["sha256"]:
                    raise ValueError("Missing, empty or changed artifact")
            except (OSError, ValueError, KeyError, TypeError):
                errors.append(f"{scenario}: invalid artifact")
    if host == "windows" and len(set(candidate_scales)) != 2:
        errors.append("IME candidate observations require two different DPI percentages")
    smoke = next((row for row in rows if isinstance(row, dict) and row.get("scenario") == "smoke.bounded_frames"), None)
    if smoke:
        try:
            frames, code = smoke["frames"], smoke["exit_code"]
            if type(frames) is not int or not 1 <= frames <= 10000 or type(code) is not int:
                raise ValueError("Invalid smoke metadata")
            output = "\n".join((directory / item["path"]).read_text(encoding="utf-8", errors="replace")
                               for item in smoke["artifacts"]
                               if not Path(item["path"]).is_absolute() and ".." not in Path(item["path"]).parts
                               and (directory / item["path"]).resolve().is_relative_to(directory.resolve()))
            if smoke_result(code, output, frames) != "pass":
                errors.append("Smoke logs do not prove successful native frames")
        except (OSError, KeyError, TypeError, ValueError):
            errors.append("Missing or invalid smoke metadata/logs")
    errors.extend(f"Missing scenario: {name}" for name in sorted(expected - seen))
    return ("FAIL" if failed else "INCOMPLETE" if errors else "COMPLETE_OPERATOR_RECORD"), errors


def smoke_result(code, output, frames):
    lines = re.findall(r"^graphical evidence: (.+)$", output, re.MULTILINE)
    if code or not lines or re.search(r"Validation Error|VUID-|SYNC-HAZARD-|D3D12 ERROR", output):
        return "fail"
    fields = dict(re.findall(r"(\w+)=([^\s]+)", lines[-1]))
    try:
        ok = (int(fields["acquired"]) == frames == int(fields["presented"])
              and int(fields["ui_draws"]) > 0 and int(fields["ui_uploads"]) > 0
              and int(fields["ui_rejected"]) == 0)
    except (KeyError, ValueError):
        ok = False
    return "pass" if ok else "fail"


def save(record, directory):
    status, errors = evaluate(record, directory)
    record["status"] = status
    record["incomplete_reasons"] = errors
    staged = directory / "evidence.json.tmp"
    staged.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    staged.replace(directory / "evidence.json")
    print(f"{status}: {directory / 'evidence.json'}", flush=True)
    return status


def record_session(args):
    host = {"Linux": "linux", "Windows": "windows"}.get(platform.system())
    if not host:
        raise ValueError("Only Linux/X11 and Windows target-host recording are supported")
    directory = args.output.resolve()
    # Refuse to replace an existing evidence bundle, including after an interrupted session.
    directory.mkdir(parents=True, exist_ok=False)
    editor = args.editor.resolve(strict=True)
    project = args.project.resolve(strict=True)
    command = [str(editor), "--graphical", f"--project={project}",
               f"--recent-projects={directory / 'recent-projects'}"]
    host_os = platform.platform()
    if host == "linux":
        try:
            host_os = platform.freedesktop_os_release().get("PRETTY_NAME", "Linux") + "; " + host_os
        except OSError:
            pass
    record = dict(schema=SCHEMA, platform=host, host_os=host_os, commit=args.commit,
                  window_backend="x11" if host == "linux" else "win32",
                  executable_sha256=digest(editor), configuration="Development", backend=args.backend,
                  gpu_driver=args.gpu_driver, monitors=args.monitors, command=command,
                  recorded_utc=datetime.now(timezone.utc).isoformat(), display_kind=args.display_kind,
                  operator_attested=False, rows=[])
    # Persist blocked rows before launching; interruption cannot leave a misleading success.
    record["rows"] = [dict(scenario=name, result="blocked", notes="Not performed", artifacts=[])
                      for name in ["smoke.bounded_frames", *scenarios(host)]]
    save(record, directory)
    stdout, stderr = directory / "smoke-stdout.log", directory / "smoke-stderr.log"
    smoke_command = [*command, f"--frames={args.frames}"]
    record["smoke_command"] = smoke_command
    with stdout.open("wb") as out, stderr.open("wb") as err:
        proc = subprocess.Popen(smoke_command, stdout=out, stderr=err)
        try:
            code = proc.wait(timeout=60)
        except subprocess.TimeoutExpired:
            code = -1
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
    output = stdout.read_text(encoding="utf-8", errors="replace") + "\n" + stderr.read_text(encoding="utf-8", errors="replace")
    record["rows"][0] = dict(scenario="smoke.bounded_frames", result=smoke_result(code, output, args.frames),
                              notes=f"exit={code}; requested_frames={args.frames}", frames=args.frames, exit_code=code,
                              artifacts=[retain(path, directory) for path in (stdout, stderr) if path.stat().st_size])
    save(record, directory)
    print("Use only a disposable project. Launch this exact binary/configuration for manual observations:")
    print(json.dumps(command))
    print("This recorder does not perform the manual scenarios or certify your observations.")
    try:
        for row in record["rows"][1:]:
            print(f"\n[{row['scenario']}] {scenarios(host)[row['scenario']]}")
            answer = input("p=pass, f=fail, Enter=blocked/not performed: ").strip().lower()
            row["result"] = {"p": "pass", "f": "fail"}.get(answer, "blocked")
            row["notes"] = input("Actual observation, DPI/IME used, limitations: ").strip()
            if host == "windows" and (row["scenario"] in {f"dpi.{scale}" for scale in (100, 125, 150, 200)}
                                       or row["scenario"].startswith("ime.candidate_dpi_")):
                actual_scale = input("Actual DPI percentage observed (e.g. 125; blank if unperformed): ").strip()
                if actual_scale.isascii() and actual_scale.isdigit():
                    row["scale_percent"] = int(actual_scale)
            artifact = input("Screenshot/recording/log file (blank if unavailable): ").strip()
            if artifact:
                try:
                    row["artifacts"] = [retain(Path(artifact), directory)]
                except (OSError, ValueError) as error:
                    row["notes"] += f"; artifact unavailable: {error}"
            save(record, directory)
        record["operator_attested"] = input("Did you personally perform all passed scenarios on the recorded host/binary? Type yes: ").strip().lower() == "yes"
    except (EOFError, KeyboardInterrupt):
        print("\nRecording interrupted; unperformed rows remain blocked.")
    return 0 if save(record, directory) == "COMPLETE_OPERATOR_RECORD" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    check = sub.add_parser("validate")
    check.add_argument("record", type=Path)
    collect = sub.add_parser("record")
    collect.add_argument("--editor", type=Path, required=True)
    collect.add_argument("--project", type=Path, required=True)
    collect.add_argument("--output", type=Path, required=True)
    collect.add_argument("--commit", required=True, help="Full source SHA used to build this binary")
    collect.add_argument("--backend", choices=("vulkan", "dx12"), required=True)
    collect.add_argument("--gpu-driver", required=True)
    collect.add_argument("--monitors", required=True, help="Monitor models and observed scales")
    collect.add_argument("--display-kind", choices=("physical", "virtual"), default="virtual")
    collect.add_argument("--frames", type=int, default=240)
    args = parser.parse_args()
    try:
        if args.mode == "record":
            if not re.fullmatch(r"[0-9a-f]{40}", args.commit) or not 1 <= args.frames <= 10000:
                parser.error("Require a full lowercase source SHA and 1..10000 smoke frames")
            return record_session(args)
        record = json.loads(args.record.read_text(encoding="utf-8"))
        status, errors = evaluate(record, args.record.resolve().parent)
        print(status)
        for error in errors:
            print(error)
        return 0 if status == "COMPLETE_OPERATOR_RECORD" else 1
    except (OSError, ValueError) as error:
        print(f"Evidence unavailable: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
