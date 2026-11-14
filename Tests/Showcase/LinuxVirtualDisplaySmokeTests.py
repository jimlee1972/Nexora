#!/usr/bin/env python3
"""Contract tests for the Linux virtual-display evidence policy."""

import contextlib
import copy
import importlib.util
import io
import json
import os
from pathlib import Path
import sys
import subprocess
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
            for name in ("windowed.json", "stdout.log", "stderr.log", "launch.json", "xvfb.stderr.log"):
                (output / name).write_text("stale PASS", encoding="utf-8")
            with mock.patch.object(sys, "argv", [str(SCRIPT), "unused", "--evidence-dir", temporary]), \
                    mock.patch.object(MODULE.shutil, "which", return_value=None), \
                    mock.patch.dict(os.environ, {"CI": "true"}, clear=True), \
                    contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(MODULE.main(), 1)
            self.assertEqual(list(output.iterdir()), [])

    def test_timeout_retains_partial_streams_and_never_accepts_stale_pass(self) -> None:
        for stdout, stderr in ((b"startup\n", b"driver \xff\n"), ("startup\n", None)):
            with self.subTest(stdout=stdout), tempfile.TemporaryDirectory() as temporary:
                output = Path(temporary)
                (output / "launch.json").write_text('{"acceptance_passed":true}', encoding="utf-8")
                server = mock.Mock()
                server.poll.return_value = None
                server.communicate.return_value = (None, "Xvfb diagnostics\n")
                timeout = subprocess.TimeoutExpired(["showcase"], 30, output=stdout, stderr=stderr)
                with mock.patch.object(sys, "argv", [str(SCRIPT), "showcase", "--evidence-dir", temporary]), \
                        mock.patch.object(MODULE.shutil, "which", return_value="Xvfb"), \
                        mock.patch.object(MODULE, "start_xvfb", return_value=(server, ":7")), \
                        mock.patch.object(MODULE.subprocess, "run", side_effect=timeout) as run, \
                        contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(MODULE.main(), 1)
                receipt = json.loads((output / "launch.json").read_text(encoding="utf-8"))
                self.assertFalse(receipt["acceptance_passed"])
                self.assertTrue(receipt["timed_out"])
                self.assertIsNone(receipt["exit_code"])
                self.assertEqual(receipt["timeout_seconds"], 30)
                self.assertEqual(run.call_args.kwargs["timeout"], 30)
                self.assertFalse((output / "windowed.json").exists())
                self.assertEqual((output / "stdout.log").read_text(encoding="utf-8"), "startup\n")
                self.assertEqual((output / "stderr.log").read_text(encoding="utf-8"),
                                 "driver \ufffd\n" if stderr is not None else "")
                self.assertEqual((output / "xvfb.stderr.log").read_text(encoding="utf-8"), "Xvfb diagnostics\n")
                server.terminate.assert_called_once_with()
                server.communicate.assert_called_once_with(timeout=3)
                server.kill.assert_not_called()

    def test_unresponsive_xvfb_is_killed_drained_and_reaped(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            server = mock.Mock()
            server.poll.return_value = None
            server.communicate.side_effect = [subprocess.TimeoutExpired(["Xvfb"], 3), (None, "stopped\n")]
            with mock.patch.object(sys, "argv", [str(SCRIPT), "showcase", "--evidence-dir", temporary]), \
                    mock.patch.object(MODULE.shutil, "which", return_value="Xvfb"), \
                    mock.patch.object(MODULE, "start_xvfb", return_value=(server, ":7")), \
                    mock.patch.object(MODULE.subprocess, "run", return_value=subprocess.CompletedProcess(
                        ["showcase"], 2, "startup\n", "failed\n")), \
                    contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(MODULE.main(), 2)
            server.terminate.assert_called_once_with()
            server.kill.assert_called_once_with()
            self.assertEqual(server.communicate.call_args_list, [mock.call(timeout=3), mock.call(timeout=3)])
            self.assertFalse(json.loads((Path(temporary) / "launch.json").read_text())["acceptance_passed"])
            self.assertEqual((Path(temporary) / "xvfb.stderr.log").read_text(), "stopped\n")


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


class InteractionEvidenceTests(unittest.TestCase):
    def test_failed_workspace_retains_frames_and_child_output(self) -> None:
        import LinuxShowcaseInteraction as interaction
        with tempfile.TemporaryDirectory() as parent:
            retained = Path(parent) / 'interaction'
            retained.mkdir()
            with mock.patch.object(interaction.tempfile, 'mkdtemp', return_value=str(retained)), \
                    contextlib.redirect_stderr(io.StringIO()) as errors:
                with self.assertRaisesRegex(AssertionError, 'original comparison failed'):
                    with interaction.interaction_workspace() as temporary:
                        (Path(temporary) / 'stderr.log').write_text('driver output')
                        (Path(temporary) / 'frame.png').write_bytes(b'frame')
                        raise AssertionError('original comparison failed')
            self.assertEqual((retained / 'stderr.log').read_text(), 'driver output')
            self.assertEqual((retained / 'frame.png').read_bytes(), b'frame')
            self.assertIn(f'fixtures retained: {retained}', errors.getvalue())

    def test_successful_workspace_is_removed(self) -> None:
        import LinuxShowcaseInteraction as interaction
        with interaction.interaction_workspace() as temporary:
            workspace = Path(temporary)
            self.assertTrue(workspace.is_dir())
        self.assertFalse(workspace.exists())


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
