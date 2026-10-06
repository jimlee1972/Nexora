#!/usr/bin/env python3
"""Exercise rendering and crash-relaunch recovery in a real X11 server."""

import argparse
import ctypes
import os
from pathlib import Path
import re
import select
import shutil
import signal
import subprocess
import tempfile
import time


def collect_output(editor: subprocess.Popen[str], timeout: float) -> tuple[str, str]:
    """Retain both streams in CTest evidence and reject Vulkan errors even after exit zero.

    Validation layers may write to either stream without changing the application's exit code.
    This applies to every completed launch, including the intentionally SIGKILLed recovery drill.
    Cleanup-only waits remain outside this check so they cannot mask an earlier failure.
    """
    stdout, stderr = editor.communicate(timeout=timeout)
    if stdout:
        print(stdout, end="" if stdout.endswith("\n") else "\n", flush=True)
    if stderr:
        print(stderr, end="" if stderr.endswith("\n") else "\n", flush=True)
    error = re.search(r"Validation Error\b|(?:VUID-|SYNC-HAZARD-)[A-Za-z0-9_-]+",
                      stdout + "\n" + stderr)
    if error:
        raise RuntimeError(f"Vulkan validation failed: {error.group(0)}")
    return stdout, stderr


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


def request_window_close(window: str, environment: dict[str, str]) -> None:
    """Send WM_DELETE_WINDOW directly; xdotool windowclose may destroy an unowned Xvfb window."""
    x11 = ctypes.CDLL("libX11.so.6")

    class ClientMessage(ctypes.Structure):
        _fields_ = [
            ("type", ctypes.c_int),
            ("serial", ctypes.c_ulong),
            ("send_event", ctypes.c_int),
            ("display", ctypes.c_void_p),
            ("window", ctypes.c_ulong),
            ("message_type", ctypes.c_ulong),
            ("format", ctypes.c_int),
            ("data", ctypes.c_long * 5),
        ]

    class Event(ctypes.Union):
        _fields_ = [("client", ClientMessage), ("padding", ctypes.c_long * 24)]

    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    x11.XInternAtom.restype = ctypes.c_ulong
    x11.XSendEvent.argtypes = [
        ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long, ctypes.POINTER(Event)
    ]
    x11.XSendEvent.restype = ctypes.c_int
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(environment["DISPLAY"].encode())
    if not display:
        raise RuntimeError("could not connect to Xvfb for close request")
    try:
        event = Event()
        event.client.type = 33  # ClientMessage
        event.client.display = display
        event.client.window = int(window)
        event.client.message_type = x11.XInternAtom(display, b"WM_PROTOCOLS", 0)
        event.client.format = 32
        event.client.data[0] = x11.XInternAtom(display, b"WM_DELETE_WINDOW", 0)
        if not x11.XSendEvent(display, int(window), 0, 0, ctypes.byref(event)):
            raise RuntimeError("XSendEvent rejected WM_DELETE_WINDOW")
        x11.XFlush(display)
    finally:
        x11.XCloseDisplay(display)


def launch(
    editor: str,
    root: Path | None,
    recent_projects: Path,
    environment: dict[str, str],
    frames: int = 0,
    read_only: bool = False,
):
    command = [
        editor,
        f"--recent-projects={recent_projects}",
        "--graphical",
    ]
    if root is not None:
        command.append(f"--project={root}")
    if read_only:
        command.append("--read-only")
    if frames:
        command.append(f"--frames={frames}")
    return subprocess.Popen(
        command,
        env=environment,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def finish_project_selector(
    editor: subprocess.Popen[str],
    xdotool: str,
    environment: dict[str, str],
    root: Path,
    name: str,
    action: str,
    expected_access: str,
    recent_projects: Path,
) -> str:
    def press(*keys: str) -> None:
        # Keep chord modifiers visible across native GPU frames and ImGui's trickled input queue.
        subprocess.run([xdotool, "key", "--delay", "100", *keys], env=environment, check=True)
        time.sleep(0.15)

    previous_recent_revision = (recent_projects.stat().st_mtime_ns
                                if recent_projects.is_file() else None)
    window = wait_for_window(xdotool, environment)
    subprocess.run([xdotool, "windowfocus", window], env=environment, check=True)
    time.sleep(0.5)
    subprocess.run(
        [xdotool, "mousemove", "--window", window, "120", "70", "click", "1"],
        env=environment,
        check=True,
    )
    time.sleep(0.2)
    subprocess.run(
        [xdotool, "type", "--clearmodifiers", "--delay", "1", str(root)],
        env=environment,
        check=True,
    )
    time.sleep(0.2)
    if action == "created":
        subprocess.run(
            [xdotool, "mousemove", "--window", window, "120", "94", "click", "1"],
            env=environment,
            check=True,
        )
        time.sleep(0.2)
        press("ctrl+a")
        subprocess.run(
            [xdotool, "type", "--clearmodifiers", "--delay", "1", name],
            env=environment,
            check=True,
        )
        time.sleep(0.2)
        press("ctrl+n")
        deadline = time.monotonic() + 15
        while time.monotonic() < deadline and not (root / "project.nexora").is_file():
            time.sleep(0.1)
        if not (root / "project.nexora").is_file():
            subprocess.run([xdotool, "windowclose", window], env=environment, check=False)
            _, stderr = collect_output(editor, 30)
            raise RuntimeError(f"graphical selector did not create the project: {stderr}")
    else:
        press("ctrl+o")
    # The descriptor exists before background content import finishes. Recent projects are
    # atomically recorded only after activation; wait for that publication before closing.
    deadline = time.monotonic() + 15
    activated = False
    while time.monotonic() < deadline:
        if (recent_projects.is_file() and
                recent_projects.stat().st_mtime_ns != previous_recent_revision and
                str(root.resolve()) in recent_projects.read_text()):
            activated = True
            break
        if editor.poll() is not None:
            break
        time.sleep(0.05)
    if not activated:
        subprocess.run([xdotool, "windowclose", window], env=environment, check=False)
        _, stderr = collect_output(editor, 30)
        raise RuntimeError(f"project selector did not publish activation: {stderr}")
    subprocess.run([xdotool, "windowclose", window], env=environment, check=True)
    _, stderr = collect_output(editor, 30)
    if editor.returncode != 0 or "graphical evidence:" not in stderr:
        raise RuntimeError(f"project selector {action} failed: {stderr}")
    if f"selector={action}" not in stderr or f"access={expected_access}" not in stderr:
        raise RuntimeError(f"project selector did not activate the requested project: {stderr}")
    return stderr


def crash_with_pending_recovery(
    editor_path: str,
    root: Path,
    recent_projects: Path,
    environment: dict[str, str],
    xdotool: str,
    payload: str,
) -> None:
    journal = root / ".nexora/workspace.recovery"
    workspace = root / ".nexora/workspace"
    committed_workspace = workspace.read_bytes()
    with journal.open("wb") as output:
        output.write(payload.encode("utf-8"))
        output.flush()
        os.fsync(output.fileno())
    directory_fd = os.open(journal.parent, os.O_RDONLY | os.O_DIRECTORY)
    try:
        os.fsync(directory_fd)
    finally:
        os.close(directory_fd)

    editor = launch(editor_path, root, recent_projects, environment)
    try:
        wait_for_window(xdotool, environment)
        editor.kill()
        _, stderr = collect_output(editor, 5)
        if editor.returncode != -signal.SIGKILL:
            raise RuntimeError(f"Editor did not terminate through SIGKILL: {stderr}")
        if journal.read_bytes() != payload.encode("utf-8"):
            raise RuntimeError("crashed Editor changed the pending recovery journal")
        if workspace.read_bytes() != committed_workspace:
            raise RuntimeError("crashed Editor changed the last committed workspace")
    finally:
        if editor.poll() is None:
            editor.kill()
            editor.communicate(timeout=5)


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
    _, stderr = collect_output(editor, 30)
    if editor.returncode != 0 or "graphical evidence:" not in stderr:
        raise RuntimeError(f"recovery process failed or omitted graphical diagnostics: {stderr}")
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
    user_state = Path(tempfile.mkdtemp(prefix="nexora-editor-user-state-"))
    recent_projects = user_state / "recent-projects"
    xvfb, display = start_xvfb(args.xvfb, "1600x900x24")
    environment = os.environ.copy()
    editor = None
    try:
        if display is None:
            raise RuntimeError("Xvfb did not become ready")
        environment["DISPLAY"] = display

        # Launch without --project and complete both graphical selector paths through real X11
        # pointer, text, and shortcut input. Creation owns the writer lease and persists the
        # descriptor; the second launch opens the same project as an explicit read-only observer.
        selector_root = root / "Created By Selector"
        editor = launch(args.editor, None, recent_projects, environment)
        finish_project_selector(
            editor,
            args.xdotool,
            environment,
            selector_root,
            "Selector Acceptance",
            "created",
            "read-write",
            recent_projects,
        )
        editor = None
        selector_descriptor = (selector_root / "project.nexora").read_text()
        if not re.fullmatch(
            r"schema=2\nuuid=[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-"
            r"[0-9a-f]{4}-[0-9a-f]{12}\nname=Selector Acceptance\n",
            selector_descriptor,
        ):
            raise RuntimeError(f"selector created an invalid project: {selector_descriptor!r}")
        editor = launch(args.editor, None, recent_projects, environment, read_only=True)
        finish_project_selector(
            editor,
            args.xdotool,
            environment,
            selector_root,
            "",
            "opened",
            "read-only",
            recent_projects,
        )
        editor = None

        (root / "Content").mkdir()
        (root / ".nexora").mkdir()
        source_asset = root / "Content/Hero.mesh"
        source_asset.write_text("mesh")
        identity_sidecar = Path(str(source_asset) + ".meta")
        (root / "project.nexora").write_text("schema=1\nname=Display Acceptance\n")
        (root / ".nexora/workspace").write_text("schema=1\n")
        editor = launch(args.editor, root, recent_projects, environment)
        window = wait_for_window(args.xdotool, environment)

        # The live Editor owns the only writer lease. A second writer must fail with an actionable
        # diagnostic, while an explicit read-only process can render the same project without
        # mutating project-owned state.
        contender = launch(args.editor, root, recent_projects, environment, frames=8)
        _, contender_stderr = collect_output(contender, 30)
        if contender.returncode == 0 or "project is already open for writing" not in contender_stderr:
            raise RuntimeError(f"second writer was not rejected: {contender_stderr}")
        observer = launch(
            args.editor, root, recent_projects, environment, frames=8, read_only=True
        )
        _, observer_stderr = collect_output(observer, 30)
        if observer.returncode != 0 or "access=read-only" not in observer_stderr:
            raise RuntimeError(f"read-only graphical open failed: {observer_stderr}")

        subprocess.run([args.xdotool, "windowfocus", window], env=environment, check=True)
        subprocess.run([args.xdotool, "windowsize", window, "1024", "640"], env=environment,
                       check=True)
        subprocess.run([args.xdotool, "mousemove", "200", "160", "click", "1"],
                       env=environment, check=True)
        subprocess.run([args.xdotool, "mousemove", "640", "300", "click", "4"],
                       env=environment, check=True)
        # Resize may move the earlier sidebar click onto an editable widget. Explicitly give
        # the central Scene canvas keyboard focus before authoring shortcuts, as a user would.
        subprocess.run([args.xdotool, "click", "1"], env=environment, check=True)
        time.sleep(0.5)
        subprocess.run([args.xdotool, "key", "--delay", "100", "ctrl+shift+n"], env=environment, check=True)
        time.sleep(0.2)
        subprocess.run([args.xdotool, "key", "--delay", "100", "F5"], env=environment, check=True)
        time.sleep(0.3)
        subprocess.run([args.xdotool, "key", "--delay", "100", "F6"], env=environment, check=True)
        time.sleep(0.2)
        subprocess.run([args.xdotool, "key", "--delay", "100", "F10"], env=environment, check=True)
        time.sleep(0.2)
        subprocess.run([args.xdotool, "key", "--delay", "100", "F5"], env=environment, check=True)
        time.sleep(0.2)
        # The initial Scene Root is unsaved. Native close must keep the window alive until the
        # user decides; Escape cancels the prompt and permits subsequent editing and saving.
        request_window_close(window, environment)
        time.sleep(0.5)
        if editor.poll() is not None or (root / ".nexora/scenes/Main.scene").exists():
            raise RuntimeError(
                f"unsaved close dismissed the Editor ({editor.poll()}) or wrote the scene "
                f"({(root / '.nexora/scenes/Main.scene').exists()})"
            )
        subprocess.run([args.xdotool, "key", "--delay", "100", "Escape"], env=environment, check=True)
        time.sleep(0.2)
        subprocess.run([args.xdotool, "key", "--delay", "100", "ctrl+s"], env=environment, check=True)
        scene_file = root / ".nexora/scenes/Main.scene"
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline and not scene_file.is_file():
            time.sleep(0.1)
        if not scene_file.is_file():
            raise RuntimeError("scene save after cancelling close did not finish")
        saved_scene = scene_file.read_text()
        if not re.search(r"^node [0-9]+ 0 Entity$", saved_scene, re.MULTILINE):
            raise RuntimeError("Hierarchy shortcut did not save the created root")
        # A real close event must stop the unbounded loop and still drain/persist cleanly.
        subprocess.run([args.xdotool, "windowclose", window], env=environment, check=True)
        _, stderr = collect_output(editor, 30)
        if editor.returncode != 0 or "graphical evidence:" not in stderr:
            raise RuntimeError(f"close-event shutdown failed: {stderr}")
        if "pie_steps=1" not in stderr:
            raise RuntimeError(f"PIE play/pause/step/stop did not complete: {stderr}")
        editor = None
        camera_path = root / ".nexora/scenes/Main.overview.camera"
        camera_lines = camera_path.read_text().splitlines()
        if (len(camera_lines) != 2 or camera_lines[0] != "NEXORA_SCENE_CAMERA 1" or
                float(camera_lines[1].split()[-1]) >= 10.0):
            raise RuntimeError(f"Scene overview zoom was not persisted: {camera_lines!r}")
        saved_orthographic_size = float(camera_lines[1].split()[-1])
        project_descriptor = (root / "project.nexora").read_text()
        if not re.fullmatch(
            r"schema=2\nuuid=[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-"
            r"[0-9a-f]{4}-[0-9a-f]{12}\nname=Display Acceptance\n",
            project_descriptor,
        ):
            raise RuntimeError(f"legacy project was not upgraded canonically: {project_descriptor!r}")
        if not recent_projects.is_file() or "Display Acceptance" not in recent_projects.read_text():
            raise RuntimeError("the opened project was not persisted in recent-project state")
        if not identity_sidecar.is_file():
            raise RuntimeError("first Editor launch did not create an asset identity sidecar")
        identity_text = identity_sidecar.read_text()
        if not re.fullmatch(
            r"schema=1\nuuid=[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-"
            r"[0-9a-f]{4}-[0-9a-f]{12}\ntype=\.mesh\n",
            identity_text,
        ):
            raise RuntimeError(f"asset identity sidecar is invalid: {identity_text!r}")

        # A corrupt project-owned layout is rejected without preventing startup. The bounded
        # run replaces it with the current schema after the default dock layout is rebuilt.
        (root / ".nexora/editor-layout.ini").write_text("schema=999\ncorrupt\n")
        editor = launch(args.editor, root, recent_projects, environment, frames=8)
        _, stderr = collect_output(editor, 30)
        if (editor.returncode != 0 or "invalid or unsupported editor layout" not in stderr or
                "scene_nodes=2" not in stderr):
            raise RuntimeError(f"corrupt-layout recovery failed: {stderr}")
        editor = None
        if not (root / ".nexora/editor-layout.ini").read_text().startswith("schema=1\n"):
            raise RuntimeError("corrupt layout was not replaced with the current schema")
        reloaded_orthographic_size = float(camera_path.read_text().splitlines()[1].split()[-1])
        if abs(reloaded_orthographic_size - saved_orthographic_size) > 1e-5:
            raise RuntimeError("Scene overview zoom did not survive project reopen")

        # The legacy schema remains readable and is migrated by the normal shutdown save.
        current_layout = (root / ".nexora/editor-layout.ini").read_text().split("\n", 1)[1]
        (root / ".nexora/editor-layout.ini").write_text("schema=0\n" + current_layout)
        editor = launch(args.editor, root, recent_projects, environment, frames=8)
        _, stderr = collect_output(editor, 30)
        if editor.returncode != 0 or "graphical evidence:" not in stderr:
            raise RuntimeError(f"layout migration run failed: {stderr}")
        editor = None
        if not (root / ".nexora/editor-layout.ini").read_text().startswith("schema=1\n"):
            raise RuntimeError("legacy layout was not migrated to the current schema")

        # Durably stage a journal, then SIGKILL the real Editor before a recovery choice.
        # Relaunch must reacquire the writer lease and accept the keyboard-only choice.
        crash_with_pending_recovery(
            args.editor, root, recent_projects, environment, args.xdotool,
            "schema=1\ndocument=Recovered.scene\n",
        )
        editor = launch(args.editor, root, recent_projects, environment, frames=600)
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
        crash_with_pending_recovery(
            args.editor, root, recent_projects, environment, args.xdotool,
            "schema=1\ndocument=Discarded.scene\n",
        )
        editor = launch(args.editor, root, recent_projects, environment, frames=600)
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
        if identity_sidecar.read_text() != identity_text:
            raise RuntimeError("asset identity changed across Editor process reopen")
        if Path(str(identity_sidecar) + ".meta").exists():
            raise RuntimeError("asset identity sidecar was indexed as a source asset")
        return 0
    finally:
        if editor is not None and editor.poll() is None:
            editor.kill()
            editor.wait(timeout=5)
        xvfb.terminate()
        xvfb.wait(timeout=5)
        shutil.rmtree(root, ignore_errors=True)
        shutil.rmtree(user_state, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
