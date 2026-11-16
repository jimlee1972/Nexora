"""Portable checks of pre-drain evidence and the native text-input protocol."""
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import WindowsBuildEvidence as evidence
from WindowsBuildWorkflow import Editor, Native


class Process:
    pid = 123

    def __init__(self, settings):
        self.settings, self.exited, self.terminated = settings, False, False

    def poll(self):
        return 0 if self.exited else None

    def terminate(self):
        self.terminated = True
        self.settings.write_bytes(b"changed during process drain")
        self.exited = True

    def wait(self, timeout):
        if not self.exited:
            raise RuntimeError("Process was not drained")
        return 0


class EvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.project, self.output = self.root / "開發 project", self.root / "evidence"
        self.project.mkdir()
        self.output.mkdir()
        self.settings = self.project / "project.build.json"
        self.raw = '{"output":"Build/預覽 🙂", "escaped":"\\t\\\""}\n'.encode("utf-8")
        self.settings.write_bytes(self.raw)
        self.log = self.output / "fixture.stderr.log"
        self.log.write_text('game build ui busy=0 p5=10,20 frame=3\n'
                            'game build draft {"frame":3,"value":{"output":"預覽 🙂"}}\n'
                            'game build request {"save":true}\n'
                            'game build settings result {"save":true,"applied":false,"error":"rejected"}\n',
                            encoding="utf-8")

    def editor(self):
        editor = Editor.__new__(Editor)
        editor.project, editor.evidence, editor.log_path = self.project, self.output, self.log
        editor.hwnd, editor.inputs = 42, [{"control": 5, "point": [10, 20]}]
        editor.stdout, editor.stderr = io.BytesIO(), io.BytesIO()
        editor.process = Process(self.settings)
        editor.native = Native.__new__(Native)
        editor.native.events = [{"kind": "post", "message": 0x102, "wparam": 0x9810}]
        editor.native.observe = lambda hwnd: {"hwnd": hwnd, "foreground_pid": 123}
        def close(hwnd, message):
            self.assertEqual((hwnd, message), (42, 0x10))
            editor.process.terminate()
        editor.native.post = close
        return editor

    def assert_pre_drain(self, phase):
        result = json.loads((self.output / "fixture.stderr.diagnostics.json").read_text(encoding="utf-8"))
        self.assertEqual(result["phase"], phase)
        saved = result["saved_settings"]
        self.assertEqual((self.output / saved["evidence"]).read_bytes(), self.raw)
        self.assertEqual(saved["captured_sha256"], hashlib.sha256(self.raw).hexdigest())
        self.assertEqual(self.settings.read_bytes(), b"changed during process drain")
        self.assertEqual(result["inputs"][0]["control"], 5)
        self.assertEqual(result["native_events"][0]["wparam"], 0x9810)
        self.assertEqual(result["latest_ui"][0]["frame"], "3")
        self.assertEqual(result["observations"]["results"][0]["error"], "rejected")
        self.assertEqual(result["observations"]["drafts"][0]["value"]["output"], "預覽 🙂")

    def test_abort_copies_bytes_and_input_before_termination(self):
        editor = self.editor()
        editor.abort()
        self.assert_pre_drain("before-abort")
        self.assertTrue(editor.stdout.closed and editor.stderr.closed and editor.process.terminated)

    def test_close_copies_before_wm_close(self):
        editor = self.editor()
        editor.close()
        self.assert_pre_drain("before-close")
        self.assertTrue(editor.stdout.closed and editor.stderr.closed)

    def test_repeated_abort_keeps_first_pre_termination_capture(self):
        for finish, phase in [("abort", "before-abort"), ("close", "before-close")]:
            with self.subTest(finish=finish):
                self.settings.write_bytes(self.raw)
                editor = self.editor()
                getattr(editor, finish)()
                before = (self.output / "fixture.stderr.diagnostics.json").read_bytes()
                editor.abort()
                self.assertEqual((self.output / "fixture.stderr.diagnostics.json").read_bytes(), before)
                self.assert_pre_drain(phase)

    def test_capture_error_cannot_skip_failure_drain(self):
        editor = self.editor()
        with patch("WindowsBuildWorkflow.capture_build_evidence", side_effect=OSError("disk unavailable")), \
                patch("sys.stderr", new_callable=io.StringIO) as errors:
            editor.abort()
        self.assertIn("disk unavailable", errors.getvalue())
        self.assertTrue(editor.process.terminated and editor.stdout.closed and editor.stderr.closed)

    def test_startup_failure_captures_and_drains_owned_process(self):
        native = self.editor().native
        process = Process(self.settings)
        failure = RuntimeError("window missing")
        with patch("WindowsBuildWorkflow.subprocess.Popen", return_value=process), \
                patch.object(Editor, "wait", side_effect=failure):
            with self.assertRaises(RuntimeError) as raised:
                Editor(native, "unused.exe", self.project, self.output, "startup", {})
        self.assertIs(raised.exception, failure)
        self.assertTrue(process.terminated)
        result = json.loads((self.output / "startup.stderr.diagnostics.json").read_text(encoding="utf-8"))
        self.assertEqual(result["phase"], "before-abort")
        self.assertEqual((self.output / result["saved_settings"]["evidence"]).read_bytes(), self.raw)

    def test_bounded_and_missing_sources_are_explicit(self):
        self.settings.write_bytes(b"x" * (evidence.MAX_SETTINGS_BYTES + 1))
        self.log.write_bytes(b"ignored prefix\n" + b"x" * evidence.MAX_LOG_BYTES +
                             b'\ngame build request {broken\ngame build ui frame=8\n')
        rows = list(range(max(evidence.MAX_INPUTS, evidence.MAX_NATIVE_EVENTS) + 1))
        result = evidence.capture(self.output, "bounded", self.project, self.log,
                                  rows, rows, {}, "before-abort", 123)
        self.assertTrue(result["saved_settings"]["truncated"])
        self.assertEqual(len((self.output / "bounded.project.build.json").read_bytes()), evidence.MAX_SETTINGS_BYTES)
        self.assertGreater(result["log"]["offset"], 0)
        self.assertEqual(result["log"]["bytes"], evidence.MAX_LOG_BYTES)
        self.assertEqual(result["latest_ui"], [{"frame": "8"}])
        self.assertEqual(len(result["malformed_observations"]), 1)
        self.assertTrue(result["inputs_truncated"] and result["native_events_truncated"])
        self.assertEqual(len(result["inputs"]), evidence.MAX_INPUTS)
        self.assertEqual(len(result["native_events"]), evidence.MAX_NATIVE_EVENTS)
        self.settings.unlink()
        self.log.unlink()
        result = evidence.capture(self.output, "missing", self.project, self.log, [], [], {}, "before-abort", 123)
        self.assertEqual(result["saved_settings"]["status"], "missing")
        self.assertEqual(result["log"]["status"], "unreadable")
        self.assertTrue((self.output / "missing.diagnostics.json").is_file())

    def test_native_text_protocol_keeps_three_clicks_and_utf16_units(self):
        native = Native.__new__(Native)
        native.events, native.last_click = [], (42, 10, 20)
        native.observe = lambda hwnd: {"hwnd": hwnd}
        clicks, messages = [], []
        native.click = lambda *args: clicks.append(args)
        native.post = lambda *args: messages.append(args)
        text = "Scripts/預覽 🙂.json"
        with patch("WindowsBuildWorkflow.time.sleep"):
            native.replace_text(42, text)
        self.assertEqual(clicks, [(42, 10, 20)] * 3)
        encoded = text.encode("utf-16-le")
        self.assertEqual(messages, [(42, 0x102, int.from_bytes(encoded[i:i + 2], "little"))
                                    for i in range(0, len(encoded), 2)])
        self.assertEqual([row["kind"] for row in native.events], ["replace_text_begin", "replace_text_end"])
        self.assertEqual(native.events[0]["text"], text)


if __name__ == "__main__":
    unittest.main()
