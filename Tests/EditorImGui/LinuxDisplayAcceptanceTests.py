#!/usr/bin/env python3
"""Process-level regression tests for false-success native validation output."""

import contextlib
import io
import json
import os
import subprocess
import sys
import time
import unittest

from LinuxDisplayAcceptance import collect_output
from LinuxPrefabIsolation import send_input


class PrefabInputTransportTests(unittest.TestCase):
    def test_ack_bound_input_preserves_trace_without_blind_settle(self):
        actions = io.StringIO()
        send_input(sys.executable, ['-c', 'pass'], os.environ.copy(), actions,
                   lambda _: self.fail('ACK-bound input added a fixed settle'),
                   deadline=time.monotonic() + 8, awaiting_ack=True)
        records = [json.loads(line) for line in actions.getvalue().splitlines()]
        self.assertTrue(records[0]['awaiting_gui_ack'])
        self.assertGreater(records[0]['timeout_seconds'], 0)
        self.assertLessEqual(records[0]['timeout_seconds'], 8)
        self.assertEqual(records[1]['returncode'], 0)
        self.assertFalse(records[1]['timed_out'])

    def test_unobserved_input_retains_native_frame_settle(self):
        drains = []
        def drain(timeout):
            drains.append(timeout)
            time.sleep(timeout)
        send_input(sys.executable, ['-c', 'pass'], os.environ.copy(), io.StringIO(), drain)
        self.assertTrue(drains)
        self.assertTrue(all(0 <= timeout <= .2 for timeout in drains))

    def test_failed_command_retains_error_and_stays_failed(self):
        actions = io.StringIO()
        with self.assertRaises(subprocess.CalledProcessError) as caught:
            send_input(sys.executable, ['-c', 'import sys; sys.stderr.write("input failed"); sys.exit(7)'],
                       os.environ.copy(), actions, lambda _: self.fail('Settled after failure'))
        self.assertEqual(caught.exception.returncode, 7)
        result = json.loads(actions.getvalue().splitlines()[-1])
        self.assertEqual(result['returncode'], 7)
        self.assertEqual(result['stderr'], 'input failed')

    def test_transport_timeout_uses_remaining_phase_and_stays_failed(self):
        actions = io.StringIO()
        with self.assertRaises(subprocess.TimeoutExpired):
            send_input(sys.executable, ['-c', 'import time; time.sleep(10)'], os.environ.copy(),
                       actions, lambda _: self.fail('Settled after timeout'),
                       deadline=time.monotonic() + .2, awaiting_ack=True)
        records = [json.loads(line) for line in actions.getvalue().splitlines()]
        self.assertLessEqual(records[0]['timeout_seconds'], .2)
        self.assertTrue(records[1]['timed_out'])
        self.assertIsNone(records[1]['returncode'])


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

    def test_already_drained_validation_error_fails_on_either_stream(self):
        for stream in ['stdout', 'stderr']:
            for message in ['Validation Error: invalid native operation\n',
                            'VUID-vkCmdDrawIndexed-None-08114 invalid descriptor\n',
                            'SYNC-HAZARD-WRITE-AFTER-READ missing dependency\n']:
                with self.subTest(stream=stream, message=message):
                    with self.launch(message if stream == 'stdout' else '',
                                     message if stream == 'stderr' else '') as editor:
                        prefix = getattr(editor, stream).read(len(message))
                        self.assertEqual(prefix, message)
                        with contextlib.redirect_stdout(io.StringIO()):
                            with self.assertRaisesRegex(RuntimeError, 'Vulkan validation failed'):
                                collect_output(editor, 5, **{stream + '_prefix': prefix})

    def test_warning_does_not_fail(self):
        with self.launch("", "WARNING: optional native capability unavailable\n") as editor:
            with contextlib.redirect_stdout(io.StringIO()):
                collect_output(editor, 5)
            self.assertEqual(editor.returncode, 0)

    def test_binary_prefix_reassembles_split_unicode_and_error_marker(self):
        for stream in ['stdout', 'stderr']:
            for message, prefix_size, fails in [
                ('原生畫面\n', 1, False),
                ('Validation Error: native operation\n', 4, True),
            ]:
                with self.subTest(stream=stream, message=message):
                    command = [sys.executable, '-c',
                               'import sys; getattr(sys, sys.argv[1]).buffer.write('
                               'sys.argv[2].encode("utf-8"))', stream, message]
                    with subprocess.Popen(command, stdout=subprocess.PIPE,
                                          stderr=subprocess.PIPE) as editor:
                        # Match the live selector's raw reads; BufferedReader.read may prefetch
                        # the tail into a buffer that subprocess.communicate does not consume.
                        prefix = os.read(getattr(editor, stream).fileno(), prefix_size)
                        with contextlib.redirect_stdout(io.StringIO()):
                            if fails:
                                with self.assertRaisesRegex(RuntimeError, 'Vulkan validation failed'):
                                    collect_output(editor, 5, **{stream + '_prefix': prefix})
                            else:
                                output = collect_output(editor, 5, **{stream + '_prefix': prefix})
                                self.assertEqual(output[0 if stream == 'stdout' else 1], message)


if __name__ == "__main__":
    unittest.main()
