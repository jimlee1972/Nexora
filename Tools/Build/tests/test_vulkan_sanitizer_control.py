"""A successful renderer or an unrelated crash cannot satisfy the injected-leak control."""
from contextlib import redirect_stdout
import importlib.util
import io
import json
from pathlib import Path
import tempfile
import unittest


spec = importlib.util.spec_from_file_location(
    "control", Path(__file__).resolve().parents[1] / "VerifyVulkanSanitizerControl.py")
control = importlib.util.module_from_spec(spec)
spec.loader.exec_module(control)


class ControlTests(unittest.TestCase):
    def run_case(self, *, helper_exit=1, native_exit=1, leak=17, function=True, pixels=True,
                 scene=True, extra=""):
        with tempfile.TemporaryDirectory(prefix="native control ") as directory:
            root = Path(directory)
            helper = root / "helper with spaces.py"
            text = (("PASS: 29 scene submissions\n" if scene else "")
                    + "ERROR: LeakSanitizer: detected memory leaks\n"
                    + ("InjectVulkanSanitizerLeak\n" if function else "OtherFault\n")
                    + f"SUMMARY: AddressSanitizer: {leak} byte(s) leaked in 1 allocation(s).\n"
                    + extra)
            helper.write_text(
                "import json,sys\nfrom pathlib import Path\n"
                "p=Path(sys.argv[sys.argv.index('--evidence-dir')+1]);p.mkdir(exist_ok=True)\n"
                f"(p/'launch.json').write_text(json.dumps({{'exit_code':{native_exit!r}}}))\n"
                + ("(p/'scene.ppm').write_bytes(b'P6 fixture')\n" if pixels else "")
                + f"print({text!r})\nsys.exit({helper_exit})\n")
            with redirect_stdout(io.StringIO()):
                return control.verify(root / "native", root / "evidence", helper)

    def test_accepts_only_the_completed_native_injected_leak(self):
        self.assertEqual(self.run_case(), 0)
        self.assertEqual(self.run_case(native_exit=23), 0)

    def test_rejects_disabled_detection_or_unavailable_native_execution(self):
        for changes in ({"helper_exit": 0, "native_exit": 0},
                        {"helper_exit": 77, "native_exit": 77}, {"native_exit": False}):
            with self.subTest(changes=changes):
                self.assertEqual(self.run_case(**changes), 1)

    def test_rejects_unrelated_faults_missing_scene_and_additional_errors(self):
        for changes in ({"leak": 34}, {"function": False}, {"pixels": False}, {"scene": False},
                        {"extra": "runtime error: invalid access\n"},
                        {"extra": "ERROR: AddressSanitizer: heap-use-after-free\n"}):
            with self.subTest(changes=changes):
                self.assertEqual(self.run_case(**changes), 1)


if __name__ == "__main__":
    unittest.main()
