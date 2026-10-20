"""Process-level checks for failure evidence and native-only tracing."""
import importlib.util
from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


spec = importlib.util.spec_from_file_location(
    "diagnose", Path(__file__).resolve().parents[1] / "DiagnoseVulkanSanitizers.py")
diagnose = importlib.util.module_from_spec(spec)
spec.loader.exec_module(diagnose)


class DiagnosticsTests(unittest.TestCase):
    def run_case(self, native_exit):
        with tempfile.TemporaryDirectory(prefix="nexora diagnostic ") as directory:
            root = Path(directory)
            build = root / "build with spaces"
            output = root / "output with spaces"
            original = build / "Testing/Temporary/LastTest.log"
            original.parent.mkdir(parents=True)
            original.write_bytes(b"original mandatory gate failed\n")
            native = build / "Tests/WindowPresentation/NexoraVulkanSceneTests"
            native.parent.mkdir(parents=True)
            native.write_text(
                "#!/usr/bin/env python3\nimport json,os,sys\n"
                "print(json.dumps({key:os.environ.get(key) for key in "
                "['VK_DRIVER_FILES','ASAN_OPTIONS','LD_DEBUG','VK_LOADER_DEBUG']}))\n"
                f"sys.exit({native_exit})\n", encoding="utf-8")
            native.chmod(0o700)
            helper = root / "Tests/Showcase/LinuxVulkanSceneAcceptance.py"
            helper.parent.mkdir(parents=True)
            helper.write_text(
                "import json,os,subprocess,sys\n"
                "assert os.environ.get('LD_DEBUG') is None\n"
                "assert os.environ.get('VK_LOADER_DEBUG') is None\n"
                "run=subprocess.run([sys.argv[2]],check=False,timeout=30)\n"
                "sys.exit(run.returncode)\n", encoding="utf-8")
            environment = os.environ.copy()
            environment.pop("LD_DEBUG", None)
            environment.pop("VK_LOADER_DEBUG", None)
            environment.update({"VK_DRIVER_FILES": "preserve-existing-selection",
                                "ASAN_OPTIONS": "detect_leaks=1",
                                "NEXORA_PRIVATE_SENTINEL": "must-never-be-captured"})
            console = io.StringIO()
            with patch.dict(os.environ, environment, clear=True), redirect_stdout(console):
                result = diagnose.diagnose(root, build, output)
            self.assertIn("original gate remains failed", console.getvalue())
            self.assertNotIn("must-never-be-captured", console.getvalue())
            self.assertEqual(result, native_exit)
            self.assertEqual(original.read_bytes(), b"original mandatory gate failed\n")
            self.assertEqual((output / "original-ctest.log").read_bytes(), original.read_bytes())
            report = json.loads((output / "result.json").read_text())
            self.assertFalse(report["original_gate_replaced"])
            self.assertEqual(report["diagnostic_exit_code"], native_exit)
            log = (output / "diagnostic.log").read_text()
            self.assertIn('"VK_DRIVER_FILES": "preserve-existing-selection"', log)
            self.assertIn('"ASAN_OPTIONS": "detect_leaks=1"', log)
            self.assertIn('"LD_DEBUG": "files"', log)
            self.assertTrue(list(output.glob("loader.*")), "missing actual loader mapping trace")
            for file in output.rglob("*"):
                if file.is_file():
                    self.assertNotIn(b"must-never-be-captured", file.read_bytes())

    def test_successful_probe_does_not_rewrite_failed_gate(self):
        self.run_case(0)

    def test_native_failure_is_propagated(self):
        self.run_case(1)

    def test_unavailable_native_gate_is_not_reported_as_pass(self):
        self.run_case(77)


if __name__ == "__main__":
    unittest.main()
