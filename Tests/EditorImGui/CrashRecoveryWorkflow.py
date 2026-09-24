#!/usr/bin/env python3
"""Exercise crash/relaunch recovery through the real graphical Editor process."""

import argparse
import os
from pathlib import Path
import selectors
import shutil
import subprocess
import tempfile
import time


def wait_for_line(process: subprocess.Popen[str], expected: str, timeout: float = 10.0) -> None:
    deadline = time.monotonic() + timeout
    output: list[str] = []
    selector = selectors.DefaultSelector()
    selector.register(process.stderr, selectors.EVENT_READ)
    while time.monotonic() < deadline:
        if not selector.select(timeout=0.1):
            if process.poll() is not None:
                break
            continue
        line = process.stderr.readline()
        if line:
            output.append(line)
            if expected in line:
                return
    raise RuntimeError(f"did not observe {expected!r}; stderr was {''.join(output)!r}")


def wait_for(predicate, description: str, timeout: float = 10.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.05)
    raise RuntimeError(f"timed out waiting for {description}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", required=True)
    parser.add_argument("--xvfb", required=True)
    parser.add_argument("--xdotool", required=True)
    args = parser.parse_args()

    root = Path(tempfile.mkdtemp(prefix="nexora-crash-recovery-"))
    display = f":{100 + os.getpid() % 400}"
    environment = os.environ.copy()
    environment["DISPLAY"] = display
    xvfb = subprocess.Popen(
        [args.xvfb, display, "-screen", "0", "1280x720x24", "-nolisten", "tcp"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
    )
    editor: subprocess.Popen[str] | None = None
    try:
        wait_for(lambda: Path(f"/tmp/.X11-unix/X{display[1:]}").exists(), "X display")
        (root / "Content").mkdir()
        (root / ".nexora").mkdir()
        (root / "project.nexora").write_text("schema=1\nname=Recovery Workflow\n")
        (root / ".nexora/workspace").write_text("schema=1\ndocument=Content/Original.scene\n")

        command = [args.editor, f"--project={root}", "--graphical"]
        editor = subprocess.Popen(command, env=environment, stderr=subprocess.PIPE, text=True)
        wait_for(
            lambda: subprocess.run(
                [args.xdotool, "search", "--name", "Nexora Editor"],
                env=environment,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
            ).returncode
            == 0,
            "initial Editor window",
        )
        (root / ".nexora/workspace.recovery").write_text(
            "schema=1\ndocument=Content/Recovered.scene\n"
        )
        editor.kill()
        editor.wait(timeout=5)

        editor = subprocess.Popen(command, env=environment, stderr=subprocess.PIPE, text=True)
        wait_for_line(editor, "recovery prompt visible")
        subprocess.run(
            [args.xdotool, "search", "--name", "Nexora Editor", "windowfocus", "key", "r"],
            env=environment,
            check=True,
        )
        wait_for(lambda: not (root / ".nexora/workspace.recovery").exists(), "recovery")
        workspace = (root / ".nexora/workspace").read_text()
        if "document=Content/Recovered.scene" not in workspace:
            raise RuntimeError("Recover did not restore the journal workspace")
        editor.kill()
        editor.wait(timeout=5)

        (root / ".nexora/workspace.recovery").write_text(
            "schema=1\ndocument=Content/Discarded.scene\n"
        )
        editor = subprocess.Popen(command, env=environment, stderr=subprocess.PIPE, text=True)
        wait_for_line(editor, "recovery prompt visible")
        subprocess.run(
            [args.xdotool, "search", "--name", "Nexora Editor", "windowfocus", "key", "d"],
            env=environment,
            check=True,
        )
        wait_for(lambda: not (root / ".nexora/workspace.recovery").exists(), "discard")
        if (root / ".nexora/workspace").read_text() != workspace:
            raise RuntimeError("Discard unexpectedly changed the saved workspace")
        editor.kill()
        editor.wait(timeout=5)
        editor = None
        return 0
    finally:
        if editor is not None and editor.poll() is None:
            editor.kill()
            editor.wait(timeout=5)
        xvfb.terminate()
        xvfb.wait(timeout=5)
        shutil.rmtree(root, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
