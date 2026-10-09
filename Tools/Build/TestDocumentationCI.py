#!/usr/bin/env python3
"""Exercise event ranges, conservative routing, and documentation failures."""

import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

import DocumentationCI as ci


class DocumentationCITests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git("init", "-q", "-b", "main")
        # Detached maintenance can outlive a commit and race TemporaryDirectory cleanup.
        # These short-lived fixtures need no automatic housekeeping; keep the settings local.
        self.git("config", "maintenance.auto", "false")
        self.git("config", "gc.auto", "0")
        self.git("config", "user.email", "ci@example.test")
        self.git("config", "user.name", "CI Test")
        self.write("README.md", "# Documentation\n")
        self.write("Engine/main.cpp", "int main() {}\n")
        self.base = self.commit()
        self.git("update-ref", "refs/remotes/origin/main", self.base)

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.root).decode().strip()

    def write(self, path, text):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text(text, encoding="utf-8")

    def commit(self):
        self.git("add", "-A")
        self.git("commit", "-qm", "test change")
        return self.git("rev-parse", "HEAD")

    def push(self, head, before=None, ref="refs/heads/topic"):
        return {"before": before or self.base, "after": head, "ref": ref,
                "repository": {"default_branch": "main"}}

    def test_docs_push_and_new_branch(self):
        self.write("README.md", "# Updated documentation\n")
        head = self.commit()
        for before in (self.base, "0" * 40):
            event = self.push(head, before)
            paths = ci.changed_files(self.root, "push", event)
            self.assertEqual(paths, ["README.md"])
            self.assertFalse(ci.full_build("push", event, paths))

    def test_pr_uses_merge_base_not_new_base_changes(self):
        self.git("switch", "-qc", "topic")
        self.write("README.md", "# Topic documentation\n")
        head = self.commit()
        self.git("switch", "-q", "main")
        self.write("Engine/main.cpp", "int main() { return 0; }\n")
        base = self.commit()
        event = {"pull_request": {"base": {"sha": base}, "head": {"sha": head}}}
        paths = ci.changed_files(self.root, "pull_request", event)
        self.assertEqual(paths, ["README.md"])
        self.assertFalse(ci.full_build("pull_request", event, paths))

    def test_entire_diff_includes_more_than_300_files(self):
        for i in range(305):
            self.write(f"Roadmap/notes-{i}.md", "# Notes\n")
        self.write("Shaders/Z.slang", "void main() {}\n")
        event = self.push(self.commit())
        paths = ci.changed_files(self.root, "push", event)
        self.assertEqual(len(paths), 306)
        self.assertTrue(ci.full_build("push", event, paths))

    def test_code_rename_to_markdown_still_requires_full_build(self):
        self.git("mv", "Engine/main.cpp", "Engine/main.md")
        event = self.push(self.commit())
        paths = ci.changed_files(self.root, "push", event)
        self.assertIn("Engine/main.cpp", paths)
        self.assertTrue(ci.full_build("push", event, paths))

    def test_tags_unknown_events_and_empty_diffs_require_full_build(self):
        self.assertTrue(ci.full_build("push", {"ref": "refs/tags/v1"}, ["README.md"]))
        self.assertTrue(ci.full_build("workflow_dispatch", {}, ["README.md"]))
        self.assertTrue(ci.full_build("push", {}, []))
        self.assertTrue(ci.full_build("push", {}, None))
        self.assertIsNone(ci.changed_files(self.root, "push", {}))

    def test_sensitive_and_non_documentation_paths(self):
        for path in (".github/workflows/build.yml", ".github/instructions.md", "AGENTS.md",
                     "CLAUDE.md", "Tests/fixture.md", "Content/level.md", "CMakeLists.txt",
                     "Shaders/Pbr.slang", "Engine/code.cpp", "README.png"):
            self.assertTrue(ci.full_build("push", {}, [path]), path)
        self.assertTrue(ci.documentation_path("Engine/Renderer/README.md"))

    def test_markdown_links_and_fences(self):
        self.write("Roadmap/資料 notes.md", "# Notes\n")
        self.write("README.md", '# Title\n\n[notes](<Roadmap/資料 notes.md>)\n'
                   '[encoded](Roadmap/%E8%B3%87%E6%96%99%20notes.md)\n'
                   '[remote](https://example.test/missing) [anchor](#title)\n'
                   '`[inline example](missing)`\n```md\n[example](missing)\n```\n'
                   '> ```md\n> [quoted example](missing)\n> ```\n'
                   '    [indented example](missing)\n')
        self.assertEqual(ci.markdown_errors(self.root, "README.md"), [])
        self.write("README.md", "# Title\n[broken](missing.md)\n```python\n")
        errors = ci.markdown_errors(self.root, "README.md")
        self.assertTrue(any("missing local link" in error for error in errors))
        self.assertTrue(any("unclosed fenced" in error for error in errors))

    def test_utf8_newline_and_deleted_markdown(self):
        (self.root / "README.md").write_bytes(b"\xff")
        self.assertTrue(ci.markdown_errors(self.root, "README.md"))
        self.write("README.md", "# Title")
        self.assertTrue(ci.markdown_errors(self.root, "README.md"))
        self.assertEqual(ci.markdown_errors(self.root, "deleted.md"), [])

    def test_matching_bilingual_roadmaps(self):
        paths = ["Roadmap/en/Plan.md", "Roadmap/zh-TW/Plan.md"]
        for path in paths:
            self.write(path, "# Plan\n")
        self.assertTrue(ci.validate_documents(self.root, paths[:1]))
        self.assertEqual(ci.validate_documents(self.root, paths), [])
        (self.root / paths[0]).unlink()
        self.assertTrue(ci.validate_documents(self.root, paths[:1]))
        self.assertTrue(ci.validate_documents(self.root, paths))
        (self.root / paths[1]).unlink()
        self.assertEqual(ci.validate_documents(self.root, paths), [])

    def test_new_single_language_roadmap_is_rejected(self):
        path = "Roadmap/en/NewPlan.md"
        self.write(path, "# Plan\n")
        self.assertTrue(ci.validate_documents(self.root, [path]))

    def test_differently_named_bilingual_pairs(self):
        paths = ["Roadmap/en/Engine_API_Foundation_Roadmap.md",
                 "Roadmap/zh-TW/Engine_API_基礎_Roadmap.md"]
        for path in paths:
            self.write(path, "# Plan\n")
        self.assertTrue(ci.validate_documents(self.root, paths[:1]))
        self.assertTrue(ci.validate_documents(self.root, paths[1:]))
        self.assertEqual(ci.validate_documents(self.root, paths), [])

    def test_repository_roadmaps_all_have_unique_counterparts(self):
        root = Path(ci.__file__).parents[2]
        self.assertEqual(len(ci.ROADMAP_LANGUAGE_PAIRS), len(set(ci.ROADMAP_LANGUAGE_PAIRS.values())))
        for language in ("en", "zh-TW"):
            for path in (root / "Roadmap" / language).rglob("*.md"):
                relative = path.relative_to(root).as_posix()
                counterpart = ci.roadmap_counterpart(relative)
                self.assertTrue((root / counterpart).exists(), relative)
                self.assertEqual(ci.roadmap_counterpart(counterpart), relative)

    def test_commonmark_links_with_parentheses_references_and_list_continuations(self):
        self.write("doc(v1).md", "# Document\n")
        self.write("README.md", '# Title\n\n[target](doc(v1).md)\n'
                   '[escaped](doc\\(v1\\).md)\n[reference][version]\n\n'
                   '[version]: doc(v1).md "Version"\n\n'
                   '- List item\n\n    [continuation](doc(v1).md)\n')
        self.assertEqual(ci.markdown_errors(self.root, "README.md"), [])
        self.write("README.md", "# Title\n\n- List item\n\n    [broken](missing.md)\n")
        self.assertTrue(ci.markdown_errors(self.root, "README.md"))

    def test_command_outputs_docs_route_and_fails_on_broken_links(self):
        self.write("README.md", "# Updated\n")
        event = self.push(self.commit())
        event_path = self.root / "event.json"
        event_path.write_text(json.dumps(event))
        output = self.root / "output"
        script = Path(ci.__file__)
        args = ["python3", str(script), "--event", str(event_path), "--event-name", "push",
                "--github-output", str(output)]
        subprocess.run(args, cwd=self.root, check=True, capture_output=True)
        self.assertEqual(output.read_text(), "full_build=false\n")
        self.write("README.md", "# Updated\n[broken](missing.md)\n")
        output.unlink()
        self.assertNotEqual(subprocess.run(args, cwd=self.root, capture_output=True).returncode, 0)
        self.assertFalse(output.exists())

    def test_aggregate_gate_rejects_failures_and_unexpected_skips(self):
        workflow = (Path(ci.__file__).parents[2] / ".github/workflows/build.yml").read_text()
        script = workflow.split("python3 - <<'PYTHON'\n", 1)[1].split("          PYTHON", 1)[0]
        script = "\n".join(line[10:] for line in script.splitlines())
        cases = [
            ("success", "false", "skipped", True),
            ("success", "true", "success", True),
            ("failure", "false", "skipped", False),
            ("cancelled", "true", "success", False),
            ("success", "true", "failure", False),
            ("success", "true", "skipped", False),
            ("success", "false", "success", False),
            ("success", "", "skipped", False),
        ]
        for result, full, build_result, passed in cases:
            jobs = {"changes": {"result": result, "outputs": {"full_build": full}},
                    "desktop": {"result": build_result}}
            env = dict(os.environ, JOB_RESULTS=json.dumps(jobs))
            run = subprocess.run(["python3", "-c", script], env=env, capture_output=True)
            self.assertEqual(run.returncode == 0, passed, (result, full, build_result))

    def test_every_build_job_is_gated_and_aggregated(self):
        workflow = (Path(ci.__file__).parents[2] / ".github/workflows/build.yml").read_text()
        blocks = re.split(r"^  ([a-z][a-z0-9-]+):\n", workflow.split("\njobs:\n", 1)[1], flags=re.M)
        jobs = dict(zip(blocks[1::2], blocks[2::2]))
        for name, block in jobs.items():
            if name not in {"changes", "ci-result"}:
                self.assertIn("    needs: changes\n", block, name)
                self.assertIn("    if: needs.changes.outputs.full_build == 'true'\n", block, name)
        aggregate = jobs["ci-result"]
        self.assertIn("    if: always()\n", aggregate)
        dependencies = set(re.findall(r"^      - ([a-z][a-z0-9-]+)$", aggregate, re.M))
        self.assertEqual(dependencies, set(jobs) - {"ci-result"})


if __name__ == "__main__":
    unittest.main()
