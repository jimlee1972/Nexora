#!/usr/bin/env python3
"""Route Build jobs and validate changed Markdown using only Python and Git."""

import argparse
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from urllib.parse import unquote, urlsplit


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
    fence = None
    for number, line in enumerate(content.splitlines(), 1):
        line = re.sub(r"^(?: {0,3}> ?)+", "", line)
        marker = re.match(r"^\s{0,3}(`{3,}|~{3,})(.*)$", line)
        if marker:
            run, rest = marker.groups()
            if fence is None:
                fence = run
            elif run[0] == fence[0] and len(run) >= len(fence) and not rest.strip():
                fence = None
            continue
        if fence is not None:
            continue
        if line.startswith(("    ", "\t")):
            continue
        # Ignore code examples. Check local inline links/images, not remote URLs or heading anchors.
        line = re.sub(r"(`+).*?\1", "", line)
        for match in re.finditer(r"!?\[[^\]\n]*\]\((<[^>]+>|[^\s)]+)(?:\s+[^)]*)?\)", line):
            target = match.group(1).strip("<>")
            if target.startswith(("#", "/")) or re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", target):
                continue
            local = unquote(urlsplit(target).path)
            if local and not (file.parent / local).exists():
                errors.append(f"{path}:{number}: missing local link target {target!r}")
    if fence is not None:
        errors.append(f"{path}: unclosed fenced code block")
    return errors


def validate_documents(root, paths):
    errors = []
    changed = set(paths)
    for path in paths:
        if PurePosixPath(path).suffix.lower() != ".md":
            continue
        errors.extend(markdown_errors(root, path))
        parts = PurePosixPath(path).parts
        if len(parts) >= 3 and parts[:2] in {("Roadmap", "en"), ("Roadmap", "zh-TW")}:
            other = "zh-TW" if parts[1] == "en" else "en"
            counterpart = str(PurePosixPath("Roadmap", other, *parts[2:]))
            if (root / counterpart).exists() and counterpart not in changed:
                errors.append(f"{path}: update matching bilingual roadmap {counterpart}")
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
