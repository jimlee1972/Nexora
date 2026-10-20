#!/usr/bin/env python3
"""Contract tests for evidence admission, not physical-host acceptance."""
import argparse
import contextlib
import copy
import io
import json
import subprocess
import sys
from unittest.mock import patch
from pathlib import Path
import tempfile
import unittest

import EditorTargetEvidence as evidence

LOG = "graphical evidence: acquired=240 presented=240 ui_draws=717 ui_uploads=1 ui_rejected=0 recovery=none\n"


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        log = self.directory / "native.log"
        log.write_text(LOG, encoding="utf-8")
        self.artifact = evidence.retain(log, self.directory)

    def record(self, host="windows"):
        rows = [dict(scenario=name, result="pass", notes="Observed on recorded host", artifacts=[self.artifact])
                for name in ["smoke.bounded_frames", *evidence.scenarios(host)]]
        rows[0].update(frames=240, exit_code=0)
        if host == "windows":
            for row in rows:
                if row["scenario"] in {f"dpi.{scale}" for scale in (100, 125, 150, 200)}:
                    row["scale_percent"] = int(row["scenario"].split(".")[1])
                elif row["scenario"].startswith("ime.candidate_dpi_"):
                    row["scale_percent"] = 100 if row["scenario"].endswith("a") else 150
        return dict(schema=evidence.SCHEMA, platform=host, host_os="test-host", commit="a" * 40,
                    window_backend="win32" if host == "windows" else "x11",
                    executable_sha256="b" * 64, configuration="Development", backend="dx12" if host == "windows" else "vulkan",
                    gpu_driver="device/driver", monitors="monitor A 100%; monitor B 150%",
                    recorded_utc="2026-10-20T00:00:00+00:00", display_kind="physical", operator_attested=True,
                    command=["Editor", "--graphical", "--project=/disposable"], rows=rows)

    def status(self, record):
        return evidence.evaluate(record, self.directory)[0]

    def test_complete_operator_records(self):
        for host in ("linux", "windows"):
            self.assertEqual(self.status(self.record(host)), "COMPLETE_OPERATOR_RECORD")

    def test_each_required_scenario_is_mandatory(self):
        for host in ("linux", "windows"):
            record = self.record(host)
            for index in range(len(record["rows"])):
                with self.subTest(host=host, scenario=record["rows"][index]["scenario"]):
                    missing = copy.deepcopy(record)
                    del missing["rows"][index]
                    missing["status"] = "PASS"
                    self.assertEqual(self.status(missing), "INCOMPLETE")

    def test_incomplete_and_failed_operator_observations(self):
        for result, expected in (("blocked", "INCOMPLETE"), ("fail", "FAIL"), ("assumed", "INCOMPLETE")):
            record = self.record()
            record["rows"][1]["result"] = result
            self.assertEqual(self.status(record), expected)

    def test_artifact_missing_empty_changed_or_outside_bundle(self):
        for artifact in ({"path": "missing", "sha256": "a" * 64},
                         {"path": "../native.log", "sha256": "a" * 64},
                         {"path": str(self.directory / "native.log"), "sha256": "a" * 64},
                         {**self.artifact, "sha256": "a" * 64}):
            record = self.record()
            record["rows"][1]["artifacts"] = [artifact]
            self.assertEqual(self.status(record), "INCOMPLETE")
        path = self.directory / self.artifact["path"]
        path.write_text("", encoding="utf-8")
        self.assertEqual(self.status(self.record()), "INCOMPLETE")

    def test_symlink_cannot_escape_bundle(self):
        with tempfile.TemporaryDirectory() as outside:
            external = Path(outside) / "log"
            external.write_text(LOG, encoding="utf-8")
            link = self.directory / "link"
            try:
                link.symlink_to(external)
            except OSError:
                self.skipTest("Host does not permit symbolic links")
            record = self.record()
            record["rows"][1]["artifacts"] = [{"path": "link", "sha256": evidence.digest(external)}]
            self.assertEqual(self.status(record), "INCOMPLETE")

    def test_virtual_unattested_or_unidentified_records_stay_open(self):
        for key, value in (("display_kind", "virtual"), ("operator_attested", False), ("commit", "unknown"),
                           ("configuration", "Shipping"), ("gpu_driver", ""), ("monitors", "unknown"),
                           ("command", []), ("backend", "assumed"), ("window_backend", "unknown"),
                           ("recorded_utc", "not-a-timestamp")):
            record = self.record()
            record[key] = value
            self.assertEqual(self.status(record), "INCOMPLETE")

    def test_actual_dpi_must_match_and_candidate_scales_must_differ(self):
        for scenario, value in (("dpi.125", 100), ("dpi.150", None),
                                ("ime.candidate_dpi_b", 100), ("ime.candidate_dpi_b", "150")):
            record = self.record()
            row = next(row for row in record["rows"] if row["scenario"] == scenario)
            row["scale_percent"] = value
            self.assertEqual(self.status(record), "INCOMPLETE")

    def test_duplicate_and_malformed_rows(self):
        for bad in (None, [], {"schema": "legacy", "platform": "windows"}):
            self.assertEqual(self.status(bad), "INCOMPLETE")
        for bad in (None, {}, {"scenario": ["bad"]}, self.record()["rows"][1]):
            record = self.record()
            record["rows"].append(bad)
            self.assertEqual(self.status(record), "INCOMPLETE")

    def test_native_smoke_requires_draws_uploads_and_clean_diagnostics(self):
        for output in ("", LOG.replace("ui_draws=717", "ui_draws=0"), LOG.replace("ui_uploads=1", "ui_uploads=0"),
                       LOG.replace("presented=240", "presented=239"), LOG.replace("ui_rejected=0", "ui_rejected=1"),
                       LOG + "VUID-test\n", LOG + "D3D12 ERROR\n"):
            self.assertEqual(evidence.smoke_result(0, output, 240), "fail")
        self.assertEqual(evidence.smoke_result(1, LOG, 240), "fail")
        self.assertEqual(evidence.smoke_result(0, LOG, 240), "pass")

    def test_interrupted_interview_preserves_blocked_rows_and_artifacts(self):
        output = self.directory / "bundle"
        project = self.directory / "project"
        project.mkdir()
        args = argparse.Namespace(output=output, editor=Path(sys.executable), project=project,
                                  commit="a" * 40, backend="vulkan", gpu_driver="fixture", monitors="fixture",
                                  display_kind="physical", frames=240)

        class FakeEditor:
            def __init__(self, command, stdout, stderr):
                stderr.write(LOG.encode())

            def wait(self, timeout=None):
                return 0

            def poll(self):
                return 0

        with patch.object(evidence.platform, "system", return_value="Linux"), \
                patch.object(evidence.platform, "platform", return_value="fixture-linux"), \
                patch.object(evidence.subprocess, "Popen", FakeEditor), \
                patch("builtins.input", side_effect=EOFError), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(evidence.record_session(args), 1)
            with self.assertRaises(FileExistsError):
                evidence.record_session(args)
        record = json.loads((output / "evidence.json").read_text(encoding="utf-8"))
        self.assertEqual(record["status"], "INCOMPLETE")
        self.assertFalse(record["operator_attested"])
        self.assertEqual(record["rows"][0]["result"], "pass")
        self.assertTrue(all(row["result"] == "blocked" for row in record["rows"][1:]))
        self.assertTrue((output / record["rows"][0]["artifacts"][0]["path"]).is_file())
        self.assertEqual(list(project.iterdir()), [])

    def test_unsupported_host_does_not_create_a_bundle(self):
        output = self.directory / "unsupported-bundle"
        args = argparse.Namespace(output=output)
        with patch.object(evidence.platform, "system", return_value="Darwin"):
            with self.assertRaisesRegex(ValueError, "Only Linux/X11 and Windows"):
                evidence.record_session(args)
        self.assertFalse(output.exists())

    def test_cli_rejects_forged_status_without_rewriting_input(self):
        record = self.record()
        record["status"] = "PASS"
        record["rows"] = []
        path = self.directory / "evidence.json"
        text = json.dumps(record)
        path.write_text(text, encoding="utf-8")
        result = subprocess.run([sys.executable, str(Path(evidence.__file__)), "validate", str(path)],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 1)
        self.assertIn("INCOMPLETE", result.stdout)
        self.assertEqual(path.read_text(encoding="utf-8"), text)

    def test_validator_rechecks_smoke_logs_instead_of_trusting_pass(self):
        record = self.record()
        log = self.directory / "bad.log"
        log.write_text(LOG + "SYNC-HAZARD-WRITE\n", encoding="utf-8")
        record["rows"][0]["artifacts"] = [evidence.retain(log, self.directory)]
        self.assertEqual(self.status(record), "INCOMPLETE")


if __name__ == "__main__":
    unittest.main()
