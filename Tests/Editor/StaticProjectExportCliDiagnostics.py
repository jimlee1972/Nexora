"""Retain actionable fixture-process failures under normal and optimized Python."""
import json
import subprocess
import sys
import unittest

from StaticProjectExportCli import output_diagnostic, require_success, run, verification_report


class DiagnosticsTests(unittest.TestCase):
    def child(self, code):
        return run([sys.executable, "-c", code])

    def test_complete_successful_report_is_preserved(self):
        expected = {"status": "VERIFIED_STATIC_VIEW", "entity_count": 3}
        process = self.child(f"print({json.dumps(expected)!r})")
        self.assertEqual(verification_report(process), expected)

    def test_zero_exit_empty_output_remains_failure(self):
        with self.assertRaisesRegex(RuntimeError, "exit=0; stdout=''; stderr=''"):
            verification_report(self.child("pass"))

    def test_non_json_output_and_stderr_are_retained(self):
        process = self.child("import sys; print('not JSON'); print('stream failure', file=sys.stderr)")
        with self.assertRaises(RuntimeError) as raised:
            verification_report(process)
        self.assertIn("Invalid verification JSON", str(raised.exception))
        self.assertIn("not JSON", str(raised.exception))
        self.assertIn("stream failure", str(raised.exception))

    def test_nonzero_exit_is_never_admitted_even_with_valid_json(self):
        process = self.child("import sys; print('{}'); print('failed', file=sys.stderr); sys.exit(7)")
        with self.assertRaisesRegex(RuntimeError, "verify-package: exit=7"):
            verification_report(process)
        with self.assertRaisesRegex(RuntimeError, "repeat-write-package: exit=7"):
            require_success(process, "repeat-write-package")

    def test_uncaptured_stdout_is_distinct_from_empty_captured_output(self):
        process = subprocess.run(
            [sys.executable, "-c", "import sys; sys.exit(7)"], stdout=subprocess.DEVNULL,
            stderr=subprocess.PIPE, text=True, timeout=120)
        diagnostic = output_diagnostic(process, "verify-package")
        self.assertIn("exit=7; stdout=<not captured>; stderr=''", diagnostic)

    def test_streams_are_bounded_and_control_characters_are_escaped(self):
        text = "\n\r\x00" * 10000 + "UNBOUNDED_TAIL"
        process = subprocess.CompletedProcess([], 0, text, text)
        diagnostic = output_diagnostic(process, "verify-package")
        self.assertNotIn("\n", diagnostic)
        self.assertNotIn("\r", diagnostic)
        self.assertNotIn("\x00", diagnostic)
        self.assertNotIn("UNBOUNDED_TAIL", diagnostic)
        self.assertEqual(diagnostic.count(f"({len(text)} characters total)"), 2)
        self.assertLess(len(diagnostic), 17000)


if __name__ == "__main__":
    unittest.main()
