#!/usr/bin/env python3
"""Route Build jobs and validate changed Markdown with Git and CommonMark parsing."""

import argparse
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from urllib.parse import unquote, urlsplit

from markdown_it import MarkdownIt


ROADMAP_LANGUAGE_PAIRS = {
    "Cross-platform_3D_Engine_V1_Complete_Plan_v1_2.md": "跨平台3D_Engine_V1_完整規劃書_v1_2.md",
    "Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md": "跨平台3D_Engine_V2_完整規劃書_v1_4.md",
    "Cross-platform_3D_Engine_V3_Complete_Plan_v1_4.md": "跨平台3D_Engine_V3_完整規劃書_v1_4.md",
    "Cross-platform_3D_Engine_V1_AI_Implementation_Technology_and_System_Plan_v1_2.md": "跨平台3D_Engine_V1_AI施工技術與系統規劃_v1_2.md",
    "Cross-platform_3D_Engine_V2_AI_Implementation_Technology_and_System_Plan_v1_2.md": "跨平台3D_Engine_V2_AI施工技術與系統規劃_v1_2.md",
    "Cross-platform_3D_Engine_V3_AI_Implementation_Technology_and_System_Plan_v1_3.md": "跨平台3D_Engine_V3_AI施工技術與系統規劃_v1_3.md",
    "Engine_API_Foundation_Roadmap.md": "Engine_API_基礎_Roadmap.md",
    "Focused_Roadmaps_AI_Implementation_Plan.md": "聚焦_Roadmap_AI施工技術與系統規劃.md",
}


def git(root, *args):
    return subprocess.check_output(["git", *args], cwd=root)


def comparison(root, event_name, event):
    """Return the entire PR/push diff; unknown ranges require a full build."""
    if event_name == "pull_request":
        base = event["pull_request"]["base"]["sha"]
        head = event["pull_request"]["head"]["sha"]
        return git(root, "merge-base", base, head).decode().strip(), head
    if event_name == "push":
        head = event["after"]
        base = event["before"]
        if not base.strip("0"):
            branch = event["repository"]["default_branch"]
            if event["ref"] == f"refs/heads/{branch}":
                return None
            base = git(root, "merge-base", f"refs/remotes/origin/{branch}", head).decode().strip()
        return base, head
    return None


def changed_files(root, event_name, event):
    try:
        refs = comparison(root, event_name, event)
        if refs is None:
            return None
        # Disable rename detection so moving code to a .md path still includes its old code path.
        return [path.decode("utf-8") for path in
                git(root, "diff", "--name-only", "--no-renames", "-z", *refs, "--").split(b"\0")
                if path]
    except (KeyError, subprocess.CalledProcessError, UnicodeDecodeError):
        return None


def documentation_path(path):
    name = PurePosixPath(path)
    # Instruction files, test fixtures and runtime content retain the full gate.
    return (name.suffix.lower() == ".md"
            and name.parts[0] not in {".github", "Tests", "Content"}
            and name.name not in {"AGENTS.md", "CLAUDE.md"})


def full_build(event_name, event, paths):
    return (event_name != "push" and event_name != "pull_request"
            or event.get("ref", "").startswith("refs/tags/")
            or paths is None or not paths
            or any(not documentation_path(path) for path in paths))


def markdown_errors(root, path):
    file = root / path
    if not file.exists():
        return []  # Deleted files have no current content to lint.
    try:
        content = file.read_text(encoding="utf-8")
    except (UnicodeError, OSError) as error:
        return [f"{path}: cannot read UTF-8 Markdown: {error}"]
    errors = []
    if content and not content.endswith("\n"):
        errors.append(f"{path}: missing final newline")
    if not content.strip():
        errors.append(f"{path}: empty Markdown document")
    for token in MarkdownIt("commonmark").parse(content):
        number = token.map[0] + 1 if token.map else 1
        if token.type == "fence" and token.map[1] - token.map[0] != token.content.count("\n") + 2:
            errors.append(f"{path}:{number}: unclosed fenced code block")
        for child in token.children or []:
            target = child.attrGet("href") if child.type == "link_open" else (
                child.attrGet("src") if child.type == "image" else None)
            if target is None:
                continue
            if target.startswith(("#", "/")) or re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", target):
                continue
            local = unquote(urlsplit(target).path)
            if local and not (file.parent / local).exists():
                errors.append(f"{path}:{number}: missing local link target {target!r}")
    return errors


def roadmap_counterpart(path):
    parts = PurePosixPath(path).parts
    if len(parts) < 3 or parts[:2] not in {("Roadmap", "en"), ("Roadmap", "zh-TW")}:
        return None
    other = "zh-TW" if parts[1] == "en" else "en"
    relative = str(PurePosixPath(*parts[2:]))
    pairs = ROADMAP_LANGUAGE_PAIRS if parts[1] == "en" else {
        zh: en for en, zh in ROADMAP_LANGUAGE_PAIRS.items()}
    return str(PurePosixPath("Roadmap", other, pairs.get(relative, relative)))


def validate_documents(root, paths):
    errors = []
    changed = set(paths)
    for path in paths:
        if PurePosixPath(path).suffix.lower() != ".md":
            continue
        errors.extend(markdown_errors(root, path))
        counterpart = roadmap_counterpart(path)
        if counterpart:
            if (root / path).exists() != (root / counterpart).exists():
                errors.append(f"{path}: bilingual roadmap must exist or be deleted as a pair: {counterpart}")
            if counterpart not in changed:
                errors.append(f"{path}: update bilingual roadmap {counterpart} in the same change")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--event", type=Path, required=True)
    parser.add_argument("--event-name", required=True)
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()
    root = Path.cwd()
    event = json.loads(args.event.read_text(encoding="utf-8"))
    paths = changed_files(root, args.event_name, event)
    build = full_build(args.event_name, event, paths)
    if paths is None:
        print("Comparison unavailable: changed-document validation unavailable; full build required.")
        paths = []
    errors = validate_documents(root, paths)
    for error in errors:
        print(error)
    if errors:
        return 1
    mode = "full build" if build else "documentation only"
    print(f"PASS: checked changed Markdown and matching bilingual roadmap updates; route={mode}.")
    if args.github_output:
        with args.github_output.open("a", encoding="utf-8") as output:
            output.write(f"full_build={str(build).lower()}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
