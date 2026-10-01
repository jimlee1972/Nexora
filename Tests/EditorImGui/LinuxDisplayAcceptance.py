#!/usr/bin/env python3
"""Exercise rendering and crash-relaunch recovery in a real X11 server."""

import argparse
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time


def start_xvfb(xvfb: str, screen: str, timeout: float = 10.0):
    """Start Xvfb on a display it picks itself; return (server, ":N") once it accepts clients.

    `-displayfd` makes Xvfb choose a free display and write its number only when the server is
    ready, so neither a fixed sleep nor a guessed display number (which parallel tests can share)
    is involved. Returns (server, None) when it does not become ready within `timeout`.
    """
    read_fd, write_fd = os.pipe()
    server = subprocess.Popen(
        [xvfb, "-displayfd", str(write_fd), "-screen", "0", screen, "-nolisten", "tcp"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
        pass_fds=(write_fd,),
    )
    os.close(write_fd)
    data = b""
    deadline = time.monotonic() + timeout
    try:
        while not data.endswith(b"\n"):
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([read_fd], [], [], remaining)[0]:
                break
            chunk = os.read(read_fd, 64)
            if not chunk:  # Xvfb exited before becoming ready.
                break
            data += chunk
    finally:
        os.close(read_fd)
    if not data.endswith(b"\n"):
        return server, None
    return server, f":{int(data)}"


def wait_for_window(xdotool: str, environment: dict[str, str]) -> str:
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        found = subprocess.run(
            [xdotool, "search", "--name", "Nexora Editor"],
            env=environment,
            capture_output=True,
            text=True,
            check=False,
        )
        if found.returncode == 0 and found.stdout.strip():
            return found.stdout.splitlines()[0]
        time.sleep(0.1)
    raise RuntimeError("Nexora Editor window did not appear")


def launch(editor: str, root: Path, environment: dict[str, str], frames: int = 0):
    command = [editor, f"--project={root}", "--graphical"]
    if frames:
        command.append(f"--frames={frames}")
    return subprocess.Popen(
        command,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def finish_recovery_choice(
    editor: subprocess.Popen[str],
    xdotool: str,
    environment: dict[str, str],
    key_sequence: list[str],
    expected_choice: str,
) -> str:
    window = wait_for_window(xdotool, environment)
    subprocess.run([xdotool, "windowfocus", window], env=environment, check=True)
    time.sleep(0.5)
    subprocess.run([xdotool, *key_sequence], env=environment, check=True)
    _, stderr = editor.communicate(timeout=30)
    if "graphical evidence:" not in stderr:
        raise RuntimeError(f"missing graphical diagnostics: {stderr}")
    match = re.search(
        r"presented=(\d+) ui_draws=(\d+) ui_uploads=(\d+) ui_rejected=(\d+)", stderr
    )
    if not match or min(map(int, match.groups()[:3])) <= 0 or int(match.group(4)) != 0:
        raise RuntimeError(f"native UI evidence was incomplete: {stderr}")
    if f"recovery={expected_choice}" not in stderr:
        raise RuntimeError(f"{expected_choice} choice was not observed after relaunch: {stderr}")
    return stderr


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", required=True)
    parser.add_argument("--xvfb", required=True)
    parser.add_argument("--xdotool", required=True)
    args = parser.parse_args()
    root = Path(tempfile.mkdtemp(prefix="nexora-display-acceptance-"))
    xvfb, display = start_xvfb(args.xvfb, "1600x900x24")
    environment = os.environ.copy()
    editor = None
    try:
        if display is None:
            raise RuntimeError("Xvfb did not become ready")
        environment["DISPLAY"] = display
        (root / "Content").mkdir()
        (root / ".nexora").mkdir()
        (root / "project.nexora").write_text("schema=1\nname=Display Acceptance\n")
        (root / ".nexora/workspace").write_text("schema=1\n")
        editor = launch(args.editor, root, environment)
        window = wait_for_window(args.xdotool, environment)
        subprocess.run([args.xdotool, "windowfocus", window], env=environment, check=True)
        subprocess.run([args.xdotool, "windowsize", window, "1024", "640"], env=environment,
                       check=True)
        subprocess.run([args.xdotool, "mousemove", "200", "160", "click", "1"],
                       env=environment, check=True)
        subprocess.run([args.xdotool, "key", "ctrl+s"], env=environment, check=True)

        # A real close event must stop the unbounded loop and still drain/persist cleanly.
        subprocess.run([args.xdotool, "windowclose", window], env=environment, check=True)
        _, stderr = editor.communicate(timeout=30)
        if editor.returncode != 0 or "graphical evidence:" not in stderr:
            raise RuntimeError(f"close-event shutdown failed: {stderr}")
        editor = None

        # A corrupt project-owned layout is rejected without preventing startup. The bounded
        # run replaces it with the current schema after the default dock layout is rebuilt.
        (root / ".nexora/editor-layout.ini").write_text("schema=999\ncorrupt\n")
        editor = launch(args.editor, root, environment, frames=8)
        _, stderr = editor.communicate(timeout=30)
        if editor.returncode != 0 or "invalid or unsupported editor layout" not in stderr:
            raise RuntimeError(f"corrupt-layout recovery failed: {stderr}")
        editor = None
        if not (root / ".nexora/editor-layout.ini").read_text().startswith("schema=1\n"):
            raise RuntimeError("corrupt layout was not replaced with the current schema")

        # The legacy schema remains readable and is migrated by the normal shutdown save.
        current_layout = (root / ".nexora/editor-layout.ini").read_text().split("\n", 1)[1]
        (root / ".nexora/editor-layout.ini").write_text("schema=0\n" + current_layout)
        editor = launch(args.editor, root, environment, frames=8)
        _, stderr = editor.communicate(timeout=30)
        if editor.returncode != 0 or "graphical evidence:" not in stderr:
            raise RuntimeError(f"layout migration run failed: {stderr}")
        editor = None
        if not (root / ".nexora/editor-layout.ini").read_text().startswith("schema=1\n"):
            raise RuntimeError("legacy layout was not migrated to the current schema")

        # Leave behind a valid recovery journal, as a crashed session would. The relaunched
        # process must discover it before normal editing and accept the keyboard-only choice.
        (root / ".nexora/workspace.recovery").write_text(
            "schema=1\ndocument=Recovered.scene\n"
        )
        editor = launch(args.editor, root, environment, frames=600)
        finish_recovery_choice(
            editor, args.xdotool, environment, ["key", "Tab", "key", "Return"], "recover"
        )
        editor = None
        if (root / ".nexora/workspace.recovery").exists():
            raise RuntimeError("successful recovery did not remove the journal")
        if "document=Recovered.scene" not in (root / ".nexora/workspace").read_text():
            raise RuntimeError("recovered workspace contents were not committed")

        # Exercise the destructive branch separately. Discard must remove only the journal and
        # leave the last committed workspace untouched.
        committed_workspace = (root / ".nexora/workspace").read_text()
        (root / ".nexora/workspace.recovery").write_text(
            "schema=1\ndocument=Discarded.scene\n"
        )
        editor = launch(args.editor, root, environment, frames=600)
        finish_recovery_choice(
            editor,
            args.xdotool,
            environment,
            ["key", "Tab", "key", "Tab", "key", "Return"],
            "discard",
        )
        editor = None
        if (root / ".nexora/workspace.recovery").exists():
            raise RuntimeError("successful discard did not remove the journal")
        if (root / ".nexora/workspace").read_text() != committed_workspace:
            raise RuntimeError("discard changed the last committed workspace")
        if not (root / ".nexora/editor-layout.ini").is_file():
            raise RuntimeError("the Editor did not persist its layout")
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
