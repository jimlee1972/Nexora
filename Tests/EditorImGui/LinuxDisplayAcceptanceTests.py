#!/usr/bin/env python3
"""Process-level regression tests for false-success native validation output."""

import contextlib
import io
import subprocess
import sys
import unittest

from LinuxDisplayAcceptance import collect_output


class ValidationOutputTests(unittest.TestCase):
    def launch(self, stdout: str, stderr: str, code: int = 0):
        return subprocess.Popen(
            [sys.executable, "-c",
             "import sys; sys.stdout.write(sys.argv[1]); sys.stderr.write(sys.argv[2]); "
             "sys.exit(int(sys.argv[3]))", stdout, stderr, str(code)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        )

    def test_success_retains_both_streams(self):
        stdout = "ordinary output\n"
        stderr = "graphical evidence: presented=240 ui_rejected=0\n"
        evidence = io.StringIO()
        with self.launch(stdout, stderr) as editor, contextlib.redirect_stdout(evidence):
            self.assertEqual(collect_output(editor, 5), (stdout, stderr))
            self.assertEqual(editor.returncode, 0)
        self.assertIn(stdout, evidence.getvalue())
        self.assertIn(stderr, evidence.getvalue())

    def test_exit_zero_validation_error_fails_on_either_stream(self):
        for message in [
            "Validation Error: [ VUID-vkDestroyBuffer-buffer-00922 ] Object still in use\n",
            "ERROR: VUID-vkCmdDrawIndexed-None-08114 invalid descriptor\n",
            "SYNC-HAZARD-WRITE-AFTER-READ missing dependency\n",
        ]:
            for stream in ["stdout", "stderr"]:
                with self.subTest(message=message, stream=stream):
                    evidence = io.StringIO()
                    stdout = message if stream == "stdout" else ""
                    stderr = message if stream == "stderr" else ""
                    with self.launch(stdout, stderr) as editor, contextlib.redirect_stdout(evidence):
                        with self.assertRaisesRegex(RuntimeError, "Vulkan validation failed"):
                            collect_output(editor, 5)
                        self.assertEqual(editor.returncode, 0)
                    self.assertIn(message, evidence.getvalue())

    def test_nonzero_exit_remains_available_to_scenario_policy(self):
        with self.launch("", "project is already open for writing\n", 1) as editor:
            with contextlib.redirect_stdout(io.StringIO()):
                collect_output(editor, 5)
            self.assertEqual(editor.returncode, 1)

    def test_warning_does_not_fail(self):
        with self.launch("", "WARNING: optional native capability unavailable\n") as editor:
            with contextlib.redirect_stdout(io.StringIO()):
                collect_output(editor, 5)
            self.assertEqual(editor.returncode, 0)


if __name__ == "__main__":
    unittest.main()
