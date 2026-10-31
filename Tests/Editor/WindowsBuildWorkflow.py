"""Real Windows Editor controls, script builds, direct GUI game launch and rollback."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "EditorImGui"))
from PackagedPlayProject import write_fixture
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Tools/Editor"))
from PackageWindowsEditor import require_gui_subsystem


def require(value, message):
    if not value:
        raise RuntimeError(message)


def hashes(directory, exclude_editor=False):
    return {str(p.relative_to(directory)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in directory.rglob("*") if p.is_file() and
            (not exclude_editor or ".nexora" not in p.relative_to(directory).parts)}


class Native:
    def __init__(self):
        self.user = C.WinDLL("user32", use_last_error=True)
        self.kernel = C.WinDLL("kernel32", use_last_error=True)
        self.user.SetThreadDpiAwarenessContext.argtypes = [W.HANDLE]
        self.user.SetThreadDpiAwarenessContext(C.c_void_p(-4))
        self.user.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
        self.user.GetWindowTextW.argtypes = [W.HWND, W.LPWSTR, C.c_int]
        self.user.GetClassNameW.argtypes = [W.HWND, W.LPWSTR, C.c_int]
        self.user.IsWindowVisible.argtypes = [W.HWND]
        self.callback = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
        self.user.EnumWindows.argtypes = [self.callback, W.LPARAM]
        self.user.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
        self.user.SendMessageTimeoutW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM,
                                                W.UINT, W.UINT, C.POINTER(C.c_size_t)]
        self.user.SendMessageTimeoutW.restype = C.c_ssize_t
        self.user.GetClientRect.argtypes = [W.HWND, C.POINTER(W.RECT)]
        self.user.ClientToScreen.argtypes = [W.HWND, C.POINTER(W.POINT)]
        self.user.SetForegroundWindow.argtypes = [W.HWND]
        self.user.GetForegroundWindow.restype = W.HWND
        self.user.SetFocus.argtypes = [W.HWND]
        self.kernel.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]
        self.kernel.CreateToolhelp32Snapshot.restype = W.HANDLE
        self.kernel.CloseHandle.argtypes = [W.HANDLE]

        class Entry(C.Structure):
            _fields_ = [("size", W.DWORD), ("usage", W.DWORD), ("pid", W.DWORD),
                        ("heap", C.c_size_t), ("module", W.DWORD), ("threads", W.DWORD),
                        ("parent", W.DWORD), ("priority", W.LONG), ("flags", W.DWORD),
                        ("executable", W.WCHAR * 260)]
        self.Entry = Entry
        for name in ("Process32FirstW", "Process32NextW"):
            getattr(self.kernel, name).argtypes = [W.HANDLE, C.POINTER(Entry)]

    def windows(self, pid):
        found = []
        @self.callback
        def visit(hwnd, _):
            owner = W.DWORD()
            self.user.GetWindowThreadProcessId(hwnd, C.byref(owner))
            if owner.value == pid and self.user.IsWindowVisible(hwnd):
                title, cls = C.create_unicode_buffer(256), C.create_unicode_buffer(256)
                self.user.GetWindowTextW(hwnd, title, 256)
                self.user.GetClassNameW(hwnd, cls, 256)
                found.append({"hwnd": int(hwnd), "title": title.value, "class": cls.value})
            return True
        require(self.user.EnumWindows(visit, 0), "EnumWindows failed")
        return found

    def processes(self):
        snapshot = self.kernel.CreateToolhelp32Snapshot(2, 0)
        require(snapshot != C.c_void_p(-1).value, "Process snapshot failed")
        try:
            entry = self.Entry()
            entry.size = C.sizeof(entry)
            result = []
            available = self.kernel.Process32FirstW(snapshot, C.byref(entry))
            require(available, "Process snapshot was not readable")
            while available:
                result.append({"pid": entry.pid, "parent": entry.parent, "name": entry.executable})
                available = self.kernel.Process32NextW(snapshot, C.byref(entry))
            require(C.get_last_error() == 18, "Process enumeration was incomplete")
            return result
        finally:
            self.kernel.CloseHandle(snapshot)

    def descendants(self, pid):
        processes, owners, result = self.processes(), {pid}, []
        while True:
            added = [p for p in processes if p["parent"] in owners and p["pid"] not in owners]
            if not added:
                return result
            result += added
            owners.update(p["pid"] for p in added)

    def responsive(self, hwnd):
        result = C.c_size_t()
        return bool(self.user.SendMessageTimeoutW(hwnd, 0, 0, 0, 2, 500, C.byref(result)))

    def post(self, hwnd, message, wparam=0, lparam=0):
        require(self.user.PostMessageW(hwnd, message, wparam, lparam), "Native input post failed")

    def click(self, hwnd, x, y):
        rect = W.RECT()
        self.user.GetClientRect(hwnd, C.byref(rect))
        require(0 <= x < rect.right and 0 <= y < rect.bottom, "Control outside Editor client")
        self.user.SetForegroundWindow(hwnd)
        self.post(hwnd, 0x200, 0, (y << 16) | x)
        self.post(hwnd, 0x201, 1, (y << 16) | x)
        time.sleep(.08)
        self.post(hwnd, 0x202, 0, (y << 16) | x)
        self.last_click = (hwnd, x, y)

    def replace_text(self, hwnd, text):
        # Triple-click selects the single InputText line through the real widget. Send all
        # characters to that owned HWND; do not inject global keyboard input into another app.
        require(self.last_click[0] == hwnd, "No native field click to replace")
        time.sleep(.35)
        for _ in range(3):
            self.click(*self.last_click)
            time.sleep(.04)
        time.sleep(.1)
        encoded = text.encode("utf-16-le")
        for index in range(0, len(encoded), 2):
            self.post(hwnd, 0x102, int.from_bytes(encoded[index:index + 2], "little"))
        time.sleep(.2)


class Editor:
    def __init__(self, native, executable, project, evidence, label, environment):
        self.native, self.project, self.evidence = native, project, evidence
        self.log_path = evidence / (label + ".stderr.log")
        self.stdout = (evidence / (label + ".stdout.log")).open("wb")
        self.stderr = self.log_path.open("wb")
        self.process = subprocess.Popen([str(executable), "--project=" + str(project), "--graphical",
            "--recent-projects=" + str(project / ".nexora/recent.json")], cwd=project,
            env=environment, stdout=self.stdout, stderr=self.stderr, creationflags=0)
        self.hwnd = None
        self.inputs = []
        self.wait(lambda: self.main_window(), 45, "Editor main window missing")

    def log(self):
        return self.log_path.read_text(encoding="utf-8", errors="replace")

    def main_window(self):
        found = [w for w in self.native.windows(self.process.pid)
                 if w["class"] == "Nexora.Window" and w["title"] == "Nexora Editor"]
        if len(found) == 1 and self.native.responsive(found[0]["hwnd"]):
            self.hwnd = found[0]["hwnd"]
            return True
        return False

    def state(self):
        records = re.findall(r"^game build ui (.*)$", self.log(), re.M)
        return dict(field.split("=", 1) for field in records[-1].split()) if records else {}

    def wait(self, predicate, seconds, message):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if predicate():
                return
            require(self.process.poll() is None, self.log())
            if self.hwnd and self.state().get("busy") == "1" and self.state().get("phase") == "4":
                require(self.native.responsive(self.hwnd), "Editor stopped responding during background compilation")
            time.sleep(.05)
        raise RuntimeError(message + "\n" + self.log()[-6000:])

    def click(self, control):
        key = "p" + str(control)
        self.wait(lambda: key in self.state(), 10, "Build control missing: " + key)
        point = self.state()[key]
        time.sleep(.15)
        require(self.state().get(key) == point, "Build control geometry was not stable")
        x, y = [round(float(v)) for v in point.split(",")]
        self.native.click(self.hwnd, x, y)
        self.inputs.append({"control": control, "point": [x, y], "time": time.monotonic()})
        time.sleep(.2)

    def open_settings(self):
        self.click(0)
        self.click(1)

    def build(self, run=False):
        before = len(re.findall(r"^game build result:", self.log(), re.M))
        self.click(15 if run else 13)
        return before

    def result(self, before, expected, seconds=150):
        def completed():
            results = re.findall(r"^game build result: (.*)$", self.log(), re.M)
            return len(results) > before and self.state().get("busy") == "0"
        self.wait(completed, seconds, "Build did not finish")
        result = re.findall(r"^game build result: (.*)$", self.log(), re.M)[-1]
        require((result == "published") == expected, "Unexpected Build result: " + result)
        require(self.native.responsive(self.hwnd), "Editor became unresponsive")
        return result

    def close(self):
        if self.process.poll() is None:
            self.native.post(self.hwnd, 0x10)
            require(self.process.wait(timeout=15) == 0, "Editor close failed")
        self.stdout.close()
        self.stderr.close()
        (self.evidence / (self.log_path.stem + ".inputs.json")).write_text(
            json.dumps(self.inputs, indent=2), encoding="utf-8")

    def abort(self):
        if self.process.poll() is None:
            self.process.terminate()
            self.process.wait(timeout=15)
        self.stdout.close()
        self.stderr.close()


def run(args):
    editor_path, evidence = Path(args.editor).resolve(), Path(args.evidence_dir).resolve()
    require_gui_subsystem(editor_path)
    evidence.mkdir(parents=True, exist_ok=True)
    project = Path(tempfile.mkdtemp(prefix="Nexora build UI ")).resolve()
    project = project / "開發 project"
    project.mkdir()
    owner = project.parent
    native, editor, passed = Native(), None, False
    environment = dict(os.environ, LOCALAPPDATA=str(owner / "User Data"),
                       NEXORA_PROJECT_GAME_BUILD_EVIDENCE="1", NEXORA_PLAYER_STARTUP_EVIDENCE="1")
    try:
        write_fixture(project)
        (project / "Scripts").mkdir()
        source = project / "Scripts/game.zig"
        original = Path(args.script_source).read_bytes()
        source.write_bytes(original)
        manifest = "Scripts/預覽 project.json"
        (project / manifest).write_text(json.dumps({"schema": 1, "module_name": "WorkflowGame",
                                                  "script": "game.zig"}), encoding="utf-8")
        editor = Editor(native, editor_path, project, evidence, "save-settings", environment)
        editor.open_settings()
        editor.click(11)
        editor.click(12)
        native.replace_text(editor.hwnd, manifest)
        editor.click(10)
        native.replace_text(editor.hwnd, "Build/預覽 Game")
        editor.click(5)
        def saved():
            value = json.loads((project / "project.build.json").read_text(encoding="utf-8"))
            return value.get("build") == {"mode": "script-project", "script_manifest": manifest,
                                          "output": "Build/預覽 Game"}
        editor.wait(saved, 10, "GUI did not persist script mode and Unicode paths")
        saved_settings = (project / "project.build.json").read_bytes()
        editor.close()
        editor = None
        baseline = hashes(project, exclude_editor=True)
        editor = Editor(native, editor_path, project, evidence, "build-workflow", environment)
        editor.open_settings()
        before = editor.build(run=True)
        editor.result(before, True)
        output = project / "Build/預覽 Game"
        require((output / "Game.dll").is_file(), "GUI did not build the saved script project")
        require_gui_subsystem(output / "Game.exe")
        require(hashlib.sha256((output / "Game.exe").read_bytes()).digest() ==
                hashlib.sha256(Path(args.player).read_bytes()).digest(), "Game.exe is not the native Player")
        editor.wait(lambda: editor.state().get("running") == "1", 10, "Build & Run did not start")
        children = native.descendants(editor.process.pid)
        require(len(children) == 1 and children[0]["name"] == "Game.exe",
                "Game was not launched directly: " + str(children))
        game_pid = children[0]["pid"]
        game_window = None
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            own = native.windows(game_pid)
            require(len(own) <= 1 and all(w["class"] != "ConsoleWindowClass" for w in own),
                    "Game created extra windows or console: " + str(own))
            if len(own) == 1 and native.responsive(own[0]["hwnd"]):
                game_window = own[0]
                break
            time.sleep(.05)
        require(game_window, "Published game did not produce a responsive window")
        time.sleep(.7)
        require(not native.descendants(game_pid), "Published game created child processes")
        native.post(game_window["hwnd"], 0x10)
        editor.wait(lambda: editor.state().get("running") == "0", 15, "Game close did not drain")
        require("player GUI startup: console_attached=0" in editor.log() and
                "player native ready: presented_frames=2" in editor.log() and
                "game run result: exit=0" in editor.log(), "Direct GUI game did not complete native frames/close")
        require(not native.descendants(editor.process.pid), "Game close retained child processes")
        before = editor.build(run=True)
        editor.result(before, True)
        editor.wait(lambda: editor.state().get("running") == "1" and
                    any(p["name"] == "Game.exe" for p in native.descendants(editor.process.pid)),
                    15, "Repeated Build & Run did not launch")
        stopped_game = native.descendants(editor.process.pid)
        editor.click(14)
        editor.wait(lambda: editor.state().get("running") == "0", 15, "Stop Game did not drain")
        require(not native.descendants(editor.process.pid), "Stop Game retained child processes")
        stable = hashes(output)
        source.write_bytes(original + b"\nthis_is_a_compile_error\n")
        before = editor.build()
        compiler_error = editor.result(before, False)
        require(int(editor.state().get("diagnostics", "0")) > 0 and hashes(output) == stable,
                "Compiler failure lost diagnostics or replaced the previous game")
        source.write_bytes(original + b'\ntest "stale build" { @import("std").Thread.sleep(@import("std").time.ns_per_s); }\n')
        before = editor.build()
        editor.wait(lambda: editor.state().get("phase") == "4" and
                    any(p["name"].lower() in ("zig.exe", "ninja.exe")
                        for p in native.descendants(editor.process.pid)), 60, "No in-flight build for source change")
        source.write_bytes(original + b"\n// Newer source must not publish the older candidate.\n")
        stale = editor.result(before, False)
        require(("changed" in stale.lower() or "stale" in stale.lower()) and hashes(output) == stable,
                "Obsolete source snapshot replaced the successful game")
        source.write_bytes(original + b'\ntest "cancel build" { @import("std").Thread.sleep(30 * @import("std").time.ns_per_s); }\n')
        before = editor.build()
        editor.wait(lambda: editor.state().get("phase") == "4" and
                    any(p["name"].lower() in ("zig.exe", "ninja.exe")
                        for p in native.descendants(editor.process.pid)), 60, "No running compilation to cancel")
        cancelling = native.descendants(editor.process.pid)
        editor.click(14)
        cancelled = editor.result(before, False, 30)
        require(not native.descendants(editor.process.pid) and hashes(output) == stable,
                "Cancel retained processes or replaced the previous game")
        source.write_bytes(original)
        require((project / "project.build.json").read_bytes() == saved_settings,
                "Build rewrote the saved configuration")
        authored_after = {name: digest for name, digest in hashes(project, exclude_editor=True).items()
                          if Path(name).parts[0] != "Build"}
        require(authored_after == baseline, "Build changed the authored source tree")
        editor.close()
        editor = None
        receipt = {"status": "PASS", "scope": "Windows native GUI, current host; not clean-host acceptance",
                   "saved_configuration": json.loads(saved_settings), "direct_game": game_window,
                   "game_pid": game_pid, "game_console_attached": False, "game_normal_exit": 0,
                   "compile_failure": compiler_error, "stale_source": stale, "cancelled": cancelled,
                   "stopped_game": stopped_game,
                   "children_at_cancel": cancelling, "rollback_hashes": stable,
                   "authored_sources_unchanged": baseline, "editor_sha256": hashlib.sha256(editor_path.read_bytes()).hexdigest()}
        (evidence / "receipt.json").write_text(json.dumps(receipt, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps(receipt, ensure_ascii=True))
        passed = True
    finally:
        if editor is not None:
            editor.abort()
        if passed:
            require(owner.parent == Path(tempfile.gettempdir()).resolve() and
                    owner.name.startswith("Nexora build UI "), "Refusing unrelated cleanup")
            shutil.rmtree(owner)
        else:
            (evidence / "failed-project.txt").write_text(str(project), encoding="utf-8")
            print("Failed GUI project retained:", project, file=sys.stderr)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ("editor", "player", "script-source", "evidence-dir"):
        parser.add_argument("--" + name, required=True)
    run(parser.parse_args())
