#!/usr/bin/env python3
"""Contract tests for the Linux virtual-display evidence policy."""

import contextlib
import copy
import importlib.util
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name("LinuxVirtualDisplaySmoke.py")
SPEC = importlib.util.spec_from_file_location("linux_virtual_display_smoke", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class EvidencePolicyTests(unittest.TestCase):
    def test_local_unavailability_skips(self) -> None:
        with mock.patch.dict(os.environ, {}, clear=True), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(MODULE.unavailable("missing"), 77)

    def test_ci_unavailability_fails(self) -> None:
        output = io.StringIO()
        with mock.patch.dict(os.environ, {"CI": "true"}, clear=True), contextlib.redirect_stdout(output):
            self.assertEqual(MODULE.unavailable("missing"), 1)
        self.assertIn("CI requires Linux virtual-display acceptance", output.getvalue())

    def test_unavailable_rerun_removes_stale_evidence(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            for name in ("windowed.json", "stdout.log", "stderr.log", "launch.json"):
                (output / name).write_text("stale PASS", encoding="utf-8")
            with mock.patch.object(sys, "argv", [str(SCRIPT), "unused", "--evidence-dir", temporary]), \
                    mock.patch.object(MODULE.shutil, "which", return_value=None), \
                    mock.patch.dict(os.environ, {"CI": "true"}, clear=True), \
                    contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(MODULE.main(), 1)
            self.assertEqual(list(output.iterdir()), [])


class NativeEvidenceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.evidence = {
            "schema": "nexora.zig_showcase.v1", "status": "PASS",
            "mode": "interactive", "backend": "vulkan",
            "headless_evidence": {"executed": False},
            "lifecycle": {
                "engine_initialized": True, "module_callbacks": True,
                "module_unloaded_before_engine_shutdown": True, "frames": 4,
            },
            "windowed_evidence": {
                "executed": True, "backend": "vulkan", "backend_fallback": False,
                "surface_acquires": 4, "surface_presents": 4, "resize_requests": 1,
                "resize_generations": 2, "composed_frames": 0, "scene_draws": 4, "overlay_frames": 4,
                "clear_color": True, "triangle": False, "diagnostics_overlay": True,
            },
        }

    def test_native_run_passes(self) -> None:
        self.assertEqual(MODULE.validate_evidence(self.evidence), self.evidence["windowed_evidence"])

    def test_rejects_incomplete_or_fallback_runs(self) -> None:
        for section, key, value in (
            (None, "status", "FAIL"), (None, "schema", "unknown"),
            ("headless_evidence", "executed", True),
            ("lifecycle", "module_unloaded_before_engine_shutdown", False),
            ("lifecycle", "frames", 0),
            ("windowed_evidence", "backend_fallback", True),
            ("windowed_evidence", "surface_presents", 3),
            ("windowed_evidence", "resize_generations", 0),
            ("windowed_evidence", "resize_requests", True),
            ("windowed_evidence", "diagnostics_overlay", None),
        ):
            with self.subTest(section=section, key=key, value=value):
                evidence = copy.deepcopy(self.evidence)
                target = evidence if section is None else evidence[section]
                target[key] = value
                with self.assertRaises(ValueError):
                    MODULE.validate_evidence(evidence)

    def test_missing_report_fields_fail(self) -> None:
        with self.assertRaises(ValueError):
            MODULE.validate_evidence({})


class ResizePresentationTests(unittest.TestCase):
    def test_scaled_foreground_rejects_stale_crop_and_blank(self) -> None:
        import LinuxShowcaseInteraction as interaction
        sw, sh, width, height = 320, 180, 240, 135
        def rgb(x, y):
            if 45 < x < 275 and 30 < y < 155:
                return bytes((120 + (x // 40) % 2 * 70, 90 + (y // 23) % 2 * 70, 50))
            return bytes((10, 20, 30))
        def frame(w, h, color):
            return b''.join(b'\0' + b''.join(color(x, y) for x in range(w)) for y in range(h))
        source = frame(sw, sh, rgb)
        samples = interaction.resize_samples(source, sw, sh, width, height)
        scaled = frame(width, height, lambda x, y: rgb(
            round((x + 0.5) * sw / width - 0.5), round((y + 0.5) * sh / height - 0.5)))
        self.assertEqual(interaction.resize_match_count(scaled, width, height, samples), len(samples))
        crop = frame(width, height, rgb)
        self.assertLess(interaction.resize_match_count(crop, width, height, samples) * 10,
                        len(samples) * 9)
        blank = frame(width, height, lambda x, y: bytes((0, 0, 0)))
        self.assertEqual(interaction.resize_match_count(blank, width, height, samples), 0)
        with self.assertRaises(AssertionError):
            interaction.resize_samples(frame(sw, sh, lambda x, y: bytes((0, 0, 0))),
                                       sw, sh, width, height)
        with self.assertRaises(AssertionError):
            interaction.resize_match_count(blank[:-1], width, height, samples)


class LabExportSynchronizationTests(unittest.TestCase):
    def test_waits_for_complete_json_and_markdown(self) -> None:
        import LinuxShowcaseInteraction as interaction
        report, markdown = mock.Mock(), mock.Mock()
        report.read_text.side_effect = [FileNotFoundError(), '{', '{"status":"PASS"}', '{"status":"PASS"}']
        markdown.is_file.side_effect = [False, True]
        with mock.patch.object(interaction.time, 'sleep'):
            self.assertEqual(interaction.wait_lab_export(report, markdown), {'status': 'PASS'})
        self.assertEqual(report.read_text.call_count, 4)

    def test_missing_export_still_fails(self) -> None:
        import LinuxShowcaseInteraction as interaction
        report, markdown = mock.Mock(), mock.Mock()
        report.read_text.side_effect = FileNotFoundError()
        with mock.patch.object(interaction.time, 'monotonic', side_effect=[0, 6]):
            with self.assertRaises(AssertionError):
                interaction.wait_lab_export(report, markdown)


if __name__ == "__main__":
    unittest.main()
