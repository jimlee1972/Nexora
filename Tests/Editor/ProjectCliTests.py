"""Exercise real Editor creation/open commands, including Unicode Windows arguments."""
import argparse
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import uuid


class ProjectCliTests(unittest.TestCase):
    editor = None
    graphical = False

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="nexora-project-cli-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.project = self.root / "陰陽 project with spaces"
        self.recents = self.root / "使用者 recent projects"

    def run_editor(self, *arguments, expected=0):
        result = subprocess.run(
            [self.editor, "--recent-projects=" + str(self.recents), *arguments],
            capture_output=True, text=True, encoding="utf-8", timeout=30)
        self.assertEqual(result.returncode, expected,
                         f"{arguments!r}: exit={result.returncode}; "
                         f"stdout={result.stdout[:2048]!r}; stderr={result.stderr[:2048]!r}")
        return result

    def create(self, name="Onmyoji 陰陽"):
        return self.run_editor("--create-project=" + str(self.project), "--project-name=" + name)

    def test_help(self):
        result = self.run_editor("--help")
        self.assertIn("--create-project=PATH", result.stdout)
        self.assertIn("--project-name=NAME", result.stdout)
        self.assertFalse(self.recents.exists())

    def test_create_then_open_and_read_only_preserve_identity(self):
        # Quotes, backslashes and tabs are valid names, and must remain valid JSON too.
        name = '陰陽 "Survivors" \\ chapter\t1'
        created = json.loads(self.create(name).stdout)
        self.assertEqual(created["project"], name)
        self.assertEqual(created["schema"], 2)
        self.assertEqual(created["access"], "read-write")
        descriptor = self.project / "project.nexora"
        original = descriptor.read_bytes()
        lines = original.decode("utf-8").splitlines()
        self.assertEqual(lines[0], "schema=2")
        self.assertNotEqual(uuid.UUID(lines[1].removeprefix("uuid=")), uuid.UUID(int=0))
        self.assertTrue((self.project / "Content").is_dir())
        self.assertTrue((self.project / ".nexora/workspace").is_file())
        opened = json.loads(self.run_editor("--project=" + str(self.project)).stdout)
        self.assertEqual(opened, created)
        before = {p.relative_to(self.project): p.read_bytes()
                  for p in self.project.rglob("*") if p.is_file()}
        observed = json.loads(self.run_editor("--project=" + str(self.project), "--read-only").stdout)
        self.assertEqual(observed["access"], "read-only")
        self.assertEqual(observed["project"], name)
        self.assertEqual(descriptor.read_bytes(), original)
        self.assertEqual(before, {p.relative_to(self.project): p.read_bytes()
                                 for p in self.project.rglob("*") if p.is_file()})

    def test_existing_documentation_directory_is_preserved_and_repeat_rejected(self):
        self.project.mkdir()
        document = self.project / "README.md"
        document.write_bytes(b"existing design documentation\n")
        self.create()
        descriptor = (self.project / "project.nexora").read_bytes()
        workspace = (self.project / ".nexora/workspace").read_bytes()
        result = self.run_editor("--create-project=" + str(self.project),
                                 "--project-name=Replacement", expected=1)
        self.assertIn("already exists", result.stderr)
        self.assertEqual((self.project / "project.nexora").read_bytes(), descriptor)
        self.assertEqual((self.project / ".nexora/workspace").read_bytes(), workspace)
        self.assertEqual(document.read_bytes(), b"existing design documentation\n")

    def test_report_and_recent_paths_support_unicode(self):
        report = self.root / "建立 report.json"
        result = self.run_editor("--create-project=" + str(self.project),
                                 "--project-name=陰陽", "--report=" + str(report))
        self.assertEqual(result.stdout, "")
        self.assertEqual(json.loads(report.read_text(encoding="utf-8"))["project"], "陰陽")
        self.assertTrue(self.recents.is_file())

    def test_invalid_option_combinations_never_create_project(self):
        create = "--create-project=" + str(self.project)
        cases = [
            [create], [create, "--project-name="], ["--create-project=", "--project-name=Game"],
            [create, "--project-name=Game", "--read-only"],
            ["--read-only", create, "--project-name=Game"],
            [create, "--project-name=Game", "--project=" + str(self.project)],
            ["--project=", create, "--project-name=Game"],
            [create, create, "--project-name=Game"],
            [create, "--project-name=A", "--project-name=B"],
            ["--project-name=Game"], ["--project=", "--graphical"],
            [create, "--project-name=Game", "--unknown"],
            [create, "--project-name=Game", "--frames=oops"],
            [create, "--project-name=Game", "--gameplay-library=game.dll"],
            [create, "--project-name=Game", "--native-scene-preview"],
        ]
        for arguments in cases:
            with self.subTest(arguments=arguments):
                self.run_editor(*arguments, expected=2)
                self.assertFalse(self.project.exists())
                self.assertFalse(self.recents.exists())

    def test_invalid_names_and_unusable_root_fail_without_replacement(self):
        for name in ["line\nbreak", "x" * 1025]:
            with self.subTest(name=name[:20]):
                self.run_editor("--create-project=" + str(self.project),
                                "--project-name=" + name, expected=1)
                self.assertFalse(self.project.exists())
        self.project.write_bytes(b"existing regular file")
        self.run_editor("--create-project=" + str(self.project), "--project-name=Game", expected=1)
        self.assertEqual(self.project.read_bytes(), b"existing regular file")

    def test_open_missing_project_does_not_create_it(self):
        self.run_editor("--project=" + str(self.project), expected=1)
        self.assertFalse(self.project.exists())

    def test_unsupported_graphical_build_rejects_before_creation(self):
        if self.graphical:
            self.skipTest("graphical creation is covered by the native display gate")
        result = self.run_editor("--create-project=" + str(self.project),
                                 "--project-name=Game", "--graphical", expected=2)
        self.assertIn("not enabled at build time", result.stderr)
        self.assertFalse(self.project.exists())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", required=True)
    parser.add_argument("--graphical-build", choices=["ON", "OFF"], required=True)
    options, unittest_args = parser.parse_known_args()
    ProjectCliTests.editor = options.editor
    ProjectCliTests.graphical = options.graphical_build == "ON"
    unittest.main(argv=[__file__, *unittest_args])
