#!/usr/bin/env python3
"""Exercise native scene comparison, candidate validation and explicit file publication."""
import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import tempfile
import time

from NativeSceneInput import NativeSceneInput

from LinuxDisplayAcceptance import (
    launch, request_window_close, start_xvfb, wait_for_window,
)


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", required=True)
    parser.add_argument("--xvfb", required=True)
    parser.add_argument("--xdotool", required=True)
    parser.add_argument("--candidate", action="store_true")
    parser.add_argument("--publication", action="store_true")
    args = parser.parse_args()
    if args.publication:
        args.candidate = True
    root = Path(tempfile.mkdtemp(prefix="nexora-native-comparison-"))
    state = Path(tempfile.mkdtemp(prefix="nexora-native-comparison-state-"))
    (root / "Content").mkdir()
    (root / ".nexora").mkdir()
    (root / "project.nexora").write_text("schema=1\nname=Semantic Native\n")
    (root / ".nexora/workspace").write_text("schema=1\n")
    xvfb, display = start_xvfb(args.xvfb, "1280x900x24")
    process = None
    driver = None
    passed = False
    try:
        require(display is not None, "Xvfb did not become ready")
        env = os.environ.copy()
        env["DISPLAY"] = display
        env["XDG_STATE_HOME"] = str(state)
        env["NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY"] = "1"

        def send(*arguments):
            driver.send(*arguments)

        def wait_file(path):
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and not path.is_file():
                require(process.poll() is None, "Editor exited before managed save")
                time.sleep(.05)
            require(path.is_file(), "Managed source save failed")

        captured = bytearray()

        def wait_log(pattern, start=None):
            deadline = time.monotonic() + 15
            if start is None:
                start = len(captured)
            while time.monotonic() < deadline:
                require(process.poll() is None, "Editor exited during comparison")
                driver.drain(.1)
                if re.search(pattern, captured[start:].decode("utf-8", errors="replace")):
                    return
            raise RuntimeError(f"Native comparison diagnostic missing: {pattern}; {captured!r}")

        def wait_native_ready(require_scene=False):
            scene_scope = r"context_ready=1 " if require_scene else r"context_ready=[01] "
            wait_log(r"scene merge ui .*" + scene_scope + r"focused=1 selector=0", 0)

        def candidate_geometry(disk_choice=False, validated=False):
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                driver.drain(.1)
                lines = re.findall(r"scene merge ui [^\n]*", captured.decode("utf-8", errors="replace"))
                for line in lines[-1:]:
                    controls = dict((key, (float(x), float(y))) for key, x, y in re.findall(
                        r"(validate|disk|cancel)=([0-9.]+),([0-9.]+)", line))
                    if ("validate" in controls and (not disk_choice or "disk" in controls)
                            and (not validated or "validated=1" in line)):
                        return controls
                require(process.poll() is None, "Editor exited before actual candidate controls")
            raise RuntimeError(f"Candidate controls unavailable: {captured!r}")

        def validate_candidate(disk_choice=False):
            controls = candidate_geometry(disk_choice)
            start = len(captured)
            def click(point):
                driver.click(point)
            if disk_choice:
                click(controls["disk"])
            click(controls["validate"])
            wait_log(r"scene merge candidate finished: Complete captured candidate validated", start)
            require(re.search(r"scene merge candidate started choices=" + ("1" if disk_choice else "0"),
                              captured[start:].decode("utf-8", errors="replace")),
                    "Actual native candidate request lost explicit choices")
            candidate_geometry(validated=True)

        def publication_geometry(key, start=0):
            deadline = time.monotonic() + 15
            previous = None
            stable_since = time.monotonic()
            while time.monotonic() < deadline:
                driver.drain(.1)
                lines = re.findall(r"scene merge ui [^\n]*", captured[start:].decode("utf-8", errors="replace"))
                line = lines[-1] if lines else ""
                if line != previous:
                    previous = line
                    stable_since = time.monotonic()
                controls = dict((name, (float(x), float(y))) for name, x, y in re.findall(
                    r"(publication_review|publication_confirm|publication_cancel)=([0-9.]+),([0-9.]+)", line))
                expected_preview = "publication_preview=" + ("0" if key == "publication_review" else "1")
                if key in controls and expected_preview in line and time.monotonic() - stable_since >= .4:
                    return controls
                require(process.poll() is None, "Editor exited before publication confirmation")
            raise RuntimeError(f"Publication controls unavailable: {captured!r}")

        def publication_click(point):
            driver.click(point)

        def review_publication():
            controls = publication_geometry("publication_review")
            start = len(captured)
            publication_click(controls["publication_review"])
            return publication_geometry("publication_confirm", start)

        def finish():
            request_window_close(window, env)
            _, error = driver.finish(15)
            require(process.returncode == 0, f"Editor shutdown failed: {error}")

        process = launch(args.editor, root, state / "recent", env, binary=True)
        window = wait_for_window(args.xdotool, env)
        driver = NativeSceneInput(process, args.xdotool, env, window, state, 1)
        captured = driver.stderr
        send("windowfocus", "--sync", window)
        wait_native_ready()
        send("key", "--clearmodifiers", "ctrl+shift+n")
        send("key", "--clearmodifiers", "ctrl+shift+s")
        send("key", "--clearmodifiers", "ctrl+a")
        send("type", "--clearmodifiers", "--delay", "2", "Content/Main.scene")
        send("key", "--clearmodifiers", "Return")
        scene = root / "Content/Main.scene"
        wait_file(scene)
        base = scene.read_bytes()
        lines = base.decode("utf-8").splitlines(keepends=True)
        for index, line in enumerate(lines):
            if line.startswith("node "):
                fields = line.split(" ", 3)
                lines[index] = " ".join(fields[:3]) + " Disk authored name\n"
                break
        else:
            raise RuntimeError("Actual native saved source has no authoring node")
        disk = "".join(lines).encode("utf-8")
        require(disk != base, "Actual external revision did not change")
        scene.write_bytes(disk)
        send("key", "--clearmodifiers", "ctrl+shift+n")
        start = len(captured)
        send("key", "--clearmodifiers", "ctrl+alt+d")
        wait_log(r"scene comparison ready fields=[1-9][0-9]* conflicts=", start)
        if args.candidate:
            validate_candidate(disk_choice=True)
        require(scene.read_bytes() == disk, "Comparison saved unsaved authoring or replaced disk")
        if args.publication:
            controls = review_publication()
            publication_click(controls["publication_cancel"])
            require(scene.read_bytes() == disk and not (root / ".nexora/scene-merge-history").exists(),
                    "Native Cancel changed source or wrote an archive")
            controls = review_publication()
            changed_again = disk.replace(b"Disk authored name", b"Changed again by another writer")
            require(changed_again != disk, "Stale confirmation source fixture did not differ")
            scene.write_bytes(changed_again)
            start = len(captured)
            publication_click(controls["publication_confirm"])
            wait_log(r"scene merge publication outcome=0", start)
            require(scene.read_bytes() == changed_again, "Stale native confirmation replaced newer source")
            scene.write_bytes(disk)
            controls = review_publication()
            start = len(captured)
            publication_click(controls["publication_confirm"])
            wait_log(r"scene merge publication outcome=2", start)
            published = scene.read_bytes()
            require(published != disk and b"Disk authored name" in published and
                    len(re.findall(br"^node ", published, re.MULTILINE)) ==
                    len(re.findall(br"^node ", base, re.MULTILINE)) + 1,
                    "Native publication lost disk choice or local added entity")
            archives = list((root / ".nexora/scene-merge-history").iterdir())
            require(len(archives) == 1, "Native publication did not retain one exact provenance archive")
            archive = archives[0]
            require((archive / "base.scene").read_bytes() == base and
                    (archive / "disk.scene").read_bytes() == disk and
                    (archive / "candidate.scene").read_bytes() == published and
                    hashlib.sha256((archive / "manifest.bin").read_bytes()).hexdigest() == archive.name,
                    "Native archive lost raw source/candidate integrity")
            disk = published

        driver.click((100, 130))
        send("key", "--clearmodifiers", "ctrl+z")
        finish()
        require(scene.read_bytes() == disk, "Shutdown changed the external revision")
        process = None
        captured.clear()
        process = launch(args.editor, root, state / "reader-recent", env, read_only=True, binary=True)
        window = wait_for_window(args.xdotool, env)
        driver = NativeSceneInput(process, args.xdotool, env, window, state, 2)
        captured = driver.stderr
        send("windowfocus", "--sync", window)
        wait_native_ready(require_scene=True)
        start = len(captured)
        send("key", "--clearmodifiers", "ctrl+alt+d")
        wait_log(r"scene comparison ready fields=0 conflicts=0", start)
        if args.candidate:
            validate_candidate()
        if args.publication:
            before = {p: p.read_bytes() for p in (root / ".nexora/scene-merge-history").rglob("*") if p.is_file()}
            publication_click(publication_geometry("publication_review")["publication_review"])
            time.sleep(.3)
            require(scene.read_bytes() == disk and
                    all(p.read_bytes() == data for p, data in before.items()),
                    "Read-only publication control changed source or archive")
        scene.write_bytes(b"corrupt external scene")
        start = len(captured)
        send("key", "--clearmodifiers", "ctrl+alt+d")
        wait_log(r"scene comparison finished: .*corrupt", start)
        require(scene.read_bytes() == b"corrupt external scene", "Failed comparison changed corrupt source")
        scene.write_bytes(disk)
        finish()
        process = None
        require(scene.read_bytes() == disk, "Read-only inspection changed source")
        passed = True
        print("NATIVE_EDITOR_SEMANTIC_COMPARISON_PASSED source_preserved read_only corrupt_rejected")
        if args.candidate:
            print("NATIVE_EDITOR_MERGE_CANDIDATE_PASSED explicit_choice validated read_only source_preserved")
        if args.publication:
            print("NATIVE_EDITOR_MERGE_PUBLICATION_PASSED confirm cancel stale_disk archive read_only reopen")
    finally:
        if process is not None:
            if driver is not None and driver.process is process:
                driver.abort()
            else:
                process.kill()
                process.communicate(timeout=5)
        xvfb.terminate()
        xvfb.communicate(timeout=5)
        if passed:
            shutil.rmtree(root)
            shutil.rmtree(state)
        else:
            print(f"Native comparison fixtures retained: {root} {state}", flush=True)


if __name__ == "__main__":
    main()
