#!/usr/bin/env python3
"""Contract tests for the Linux virtual-display evidence policy."""

import contextlib
import importlib.util
import io
import os
from pathlib import Path
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


if __name__ == "__main__":
    unittest.main()
