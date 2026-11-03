"""Real Windows Editor: Ctrl+P, authored Zig edits, stateful replacement and Stop cancellation."""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from WindowsNativeTestDesktop import foreground_launcher, input_desktop

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "EditorImGui"))
from PackagedPlayProject import write_fixture


GAME = r'''const nx = @import("nexora");
const version: u32 = VERSION;
pub const Game = struct {
    pub const state_version: u32 = 1;
    pub const reload_lifecycle = true;
    counter: u64 = 0,
    scene: u64 = 0,
    entity: u64 = 0,
    fn check(self: *const @This(), context: *const nx.game.Context) !void {
        var point = nx.Vec3{};
        const read = context.host.read_component orelse return error.Unsupported;
        if (read(context.host.context, self.entity, nx.componentType("Nexora.Transform"),
                 &point, @sizeOf(nx.Vec3)) != 0 or point.x != @as(f64, @floatFromInt(self.counter)))
            return error.StateWasNotPreserved;
        const scenes = try context.scenes();
        if (try scenes.active() != self.scene) return error.SceneWasNotPreserved;
    }
    fn report(self: *const @This(), context: *const nx.game.Context, comptime phase: []const u8) void {
        context.log("live " ++ phase ++ " version={d} counter={d} scene={d} entity={d}",
                    .{version, self.counter, self.scene, self.entity});
    }
    pub fn start(self: *@This(), context: *const nx.game.Context) !void {
        const scenes = try context.scenes();
        self.scene = try scenes.active();
        const spawn = context.host.spawn_entity orelse return error.Unsupported;
        const descriptor = nx.EntitySpawnDescriptor{};
        if (spawn(context.host.context, self.scene, &descriptor, &self.entity) != 0)
            return error.HostFailure;
        try self.check(context);
        self.report(context, "ready");
    }
    pub fn prepareReload(_: *@This(), _: *const nx.game.Context) !void {}
    pub fn reloaded(self: *@This(), context: *const nx.game.Context) !void {
        try self.check(context);
        self.report(context, "ready");
    }
    pub fn update(self: *@This(), context: *const nx.game.Context, _: f64) !void {
        try self.check(context);
        self.counter += 1;
        const write = context.host.write_component orelse return error.Unsupported;
        const point = nx.Vec3{.x = @floatFromInt(self.counter)};
        if (write(context.host.context, self.entity, nx.componentType("Nexora.Transform"),
                  &point, @sizeOf(nx.Vec3)) != 0) return error.HostFailure;
        if (self.counter % 60 == 0) self.report(context, "tick");
    }
    pub fn stop(self: *@This(), context: *const nx.game.Context) void {
        if (context.host.despawn_entity) |destroy| _ = destroy(context.host.context, self.entity);
    }
    pub fn saveState(self: *const @This(), writer: *nx.game.StateWriter) !void {
        try writer.int(u64, self.counter);
        try writer.int(u64, self.scene);
        try writer.int(u64, self.entity);
    }
    pub fn loadState(self: *@This(), reader: *nx.game.StateReader) !void {
        self.counter = try reader.int(u64);
        self.scene = try reader.int(u64);
        self.entity = try reader.int(u64);
        try reader.finish();
    }
};
'''


def require(value, message):
    if not value:
        raise RuntimeError(message)


def fault_source(version, kind):
    source = GAME.replace("VERSION", f"0x{version:02x}")
    if kind == "compile":
        return source + "\nthis is deliberately invalid Zig;\n"
    if kind == "restore":
        return source.replace("try reader.finish();", "try reader.finish();\n        return error.InjectedRestore;")
    if kind == "initialize":
        return source.replace("pub fn prepareReload(_: *@This(), _: *const nx.game.Context) !void {}",
                              "pub fn prepareReload(_: *@This(), _: *const nx.game.Context) !void { return error.InjectedInitialize; }")
    if kind == "start":
        return source.replace('self.report(context, "ready");\n    }\n    pub fn update', '''const write = context.host.write_component orelse return error.Unsupported;
        const poison = nx.Vec3{.x = -12345};
        if (write(context.host.context, self.entity, nx.componentType("Nexora.Transform"),
                  &poison, @sizeOf(nx.Vec3)) != 0) return error.HostFailure;
        return error.InjectedStart;
    }
    pub fn update''')
    raise ValueError("Unknown native reload fault: " + kind)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--editor", required=True)
    parser.add_argument("--evidence-dir", required=True)
    parser.add_argument("--cycles", type=int, default=32)
    args = parser.parse_args()
    require(1 <= args.cycles <= 64, "cycles must be in [1, 64]")
    evidence = Path(args.evidence_dir).resolve()
    evidence.mkdir(parents=True, exist_ok=False)
    root = evidence / "Project 陰陽 spaces"
    root.mkdir()
    write_fixture(root)
    settings = root / "project.build.json"
    configuration = json.loads(settings.read_text(encoding="utf-8"))
    configuration.update(schema=2, build={"mode": "script-project",
                         "script_manifest": "Scripts/project.json", "output": "Build/Game"})
    settings.write_text(json.dumps(configuration), encoding="utf-8")
    (root / "Scripts").mkdir()
    (root / "Scripts/project.json").write_text(
        '{"schema":1,"module_name":"LiveEditorFixture","script":"game.zig"}', encoding="utf-8")
    script = root / "Scripts/game.zig"

    def write_source(source):
        timestamp = script.stat().st_mtime_ns if script.exists() else None
        script.write_text(source, encoding="utf-8")
        if timestamp is not None:
            os.utime(script, ns=(timestamp, timestamp))

    def edit(version):
        write_source(GAME.replace("VERSION", f"0x{version:02x}"))

    edit(0)
    baseline = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                for p in root.rglob("*") if p.is_file() and ".nexora" not in p.parts and p != script}
    user = C.WinDLL("user32", use_last_error=True)
    user.GetForegroundWindow.restype = W.HWND
    user.SetForegroundWindow.argtypes = [W.HWND]
    user.IsWindowVisible.argtypes = [W.HWND]
    user.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
    user.GetClassNameW.argtypes = [W.HWND, W.LPWSTR, C.c_int]
    user.SendMessageTimeoutW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM,
                                       W.UINT, W.UINT, C.POINTER(C.c_size_t)]
    user.SendMessageTimeoutW.restype = C.c_ssize_t
    user.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
    kernel = C.WinDLL("kernel32", use_last_error=True)

    class ProcessEntry(C.Structure):
        _fields_ = [("size", W.DWORD), ("usage", W.DWORD), ("pid", W.DWORD),
                    ("heap", C.c_size_t), ("module", W.DWORD), ("threads", W.DWORD),
                    ("parent", W.DWORD), ("priority", W.LONG), ("flags", W.DWORD),
                    ("name", W.WCHAR * 260)]

    kernel.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = W.HANDLE
    kernel.Process32FirstW.argtypes = [W.HANDLE, C.POINTER(ProcessEntry)]
    kernel.Process32NextW.argtypes = [W.HANDLE, C.POINTER(ProcessEntry)]
    kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    kernel.OpenProcess.restype = W.HANDLE
    kernel.WaitForSingleObject.argtypes = [W.HANDLE, W.DWORD]
    kernel.CloseHandle.argtypes = [W.HANDLE]
    class BasicLimits(C.Structure):
        _fields_ = [("process_time", C.c_int64), ("job_time", C.c_int64), ("flags", W.DWORD),
                    ("minimum_working_set", C.c_size_t), ("maximum_working_set", C.c_size_t),
                    ("active_processes", W.DWORD), ("affinity", C.c_size_t),
                    ("priority", W.DWORD), ("scheduling", W.DWORD)]

    class IoCounters(C.Structure):
        _fields_ = [(name, C.c_uint64) for name in
                    ("read_ops", "write_ops", "other_ops", "read_bytes", "write_bytes", "other_bytes")]

    class ExtendedLimits(C.Structure):
        _fields_ = [("basic", BasicLimits), ("io", IoCounters),
                    ("process_memory", C.c_size_t), ("job_memory", C.c_size_t),
                    ("peak_process_memory", C.c_size_t), ("peak_job_memory", C.c_size_t)]

    class Accounting(C.Structure):
        _fields_ = [("user_time", C.c_int64), ("kernel_time", C.c_int64),
                    ("period_user_time", C.c_int64), ("period_kernel_time", C.c_int64),
                    ("page_faults", W.DWORD), ("total_processes", W.DWORD),
                    ("active_processes", W.DWORD), ("terminated_processes", W.DWORD)]

    kernel.CreateJobObjectW.argtypes = [C.c_void_p, W.LPCWSTR]
    kernel.CreateJobObjectW.restype = W.HANDLE
    kernel.SetInformationJobObject.argtypes = [W.HANDLE, C.c_int, C.c_void_p, W.DWORD]
    kernel.AssignProcessToJobObject.argtypes = [W.HANDLE, W.HANDLE]
    kernel.QueryInformationJobObject.argtypes = [W.HANDLE, C.c_int, C.c_void_p, W.DWORD,
                                               C.POINTER(W.DWORD)]
    job = None
    children = []

    def running_compiler_children():
        snapshot = kernel.CreateToolhelp32Snapshot(2, 0)
        require(snapshot and snapshot != C.c_void_p(-1).value, "Cannot observe owned compiler children")
        records = []
        try:
            entry = ProcessEntry()
            entry.size = C.sizeof(entry)
            present = kernel.Process32FirstW(snapshot, C.byref(entry))
            while present:
                records.append((entry.pid, entry.parent, entry.name))
                present = kernel.Process32NextW(snapshot, C.byref(entry))
        finally:
            kernel.CloseHandle(snapshot)
        owned = {process.pid}
        while True:
            more = {pid for pid, parent, name in records if parent in owned} - owned
            if not more:
                break
            owned.update(more)
        # Retain handles only for this Editor's descendants, never other desktop applications.
        for pid, parent, name in records:
            if pid in owned and pid != process.pid:
                handle = kernel.OpenProcess(0x100000, False, pid)  # SYNCHRONIZE only.
                if handle:
                    if kernel.WaitForSingleObject(handle, 0) == 258:
                        children.append((handle, pid, name))
                    else:
                        kernel.CloseHandle(handle)
        return bool(children)
    errors = evidence / "stderr.log"
    environment = dict(os.environ, LOCALAPPDATA=str(evidence / "user-data"),
                       NEXORA_SCRIPT_RELOAD_EVIDENCE="1", NEXORA_EDITOR_STARTUP_EVIDENCE="1")
    process = None
    sequence = []
    faults = []
    case_deadline = time.monotonic() + 3600

    def checkpoint(phase):
        # Preserve real intermediate observations even if the host/test runner disappears.
        # A checkpoint is deliberately never a PASS receipt or a complete gate result.
        record = {"status": "IN_PROGRESS", "phase": phase, "cycles": args.cycles,
                  "sequence": sequence, "faults": faults,
                  "editor_sha256": hashlib.sha256(Path(args.editor).read_bytes()).hexdigest(),
                  "harness_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
        temporary = evidence / "checkpoint.tmp"
        temporary.write_text(json.dumps(record, indent=2), encoding="utf-8")
        temporary.replace(evidence / "checkpoint.json")

    def output():
        return errors.read_text(encoding="utf-8", errors="replace")

    def wait(predicate, description, seconds=120):
        deadline = min(case_deadline, time.monotonic() + seconds)
        while time.monotonic() < deadline:
            exit_code = process.poll()
            require(exit_code is None, f"Editor exited with code {exit_code}: " + output()[-12000:])
            text = output()
            found = predicate(text)
            if found:
                return found
            time.sleep(.05)
        raise RuntimeError(description + "\n" + output()[-12000:])

    def chord(hwnd):
        user.SetForegroundWindow(hwnd)
        deadline = time.monotonic() + 3
        while user.GetForegroundWindow() != hwnd and time.monotonic() < deadline:
            time.sleep(.02)
        require(user.GetForegroundWindow() == hwnd, "Refusing keyboard input to a foreign window")
        user.keybd_event(0x11, 0, 0, 0)
        user.keybd_event(0x50, 0, 0, 0)
        user.keybd_event(0x50, 0, 2, 0)
        user.keybd_event(0x11, 0, 2, 0)

    try:
        checkpoint("launch")
        with input_desktop(evidence), foreground_launcher(), errors.open("wb") as stderr, (evidence / "stdout.log").open("wb") as stdout:
            job = kernel.CreateJobObjectW(None, None)
            limits = ExtendedLimits()
            limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE.
            require(job and kernel.SetInformationJobObject(job, 9, C.byref(limits), C.sizeof(limits)),
                    "Cannot create an owning Editor/SDK process job")
            process = subprocess.Popen([str(Path(args.editor).resolve()), "--project=" + str(root),
                                        "--graphical", "--native-scene-preview"], cwd=root,
                                       env=environment, stdout=stdout, stderr=stderr)
            owned_process = kernel.OpenProcess(0x101, False, process.pid)  # SET_QUOTA | TERMINATE.
            try:
                require(owned_process and kernel.AssignProcessToJobObject(job, owned_process),
                        "Cannot bind the owned Editor to its cleanup job")
            finally:
                if owned_process:
                    kernel.CloseHandle(owned_process)
            wait(lambda text: "editor splash: main-window-shown" in text, "Editor did not become ready", 35)
            windows = []
            callback_type = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)

            @callback_type
            def collect(hwnd, parameter):
                pid = W.DWORD()
                user.GetWindowThreadProcessId(hwnd, C.byref(pid))
                name = C.create_unicode_buffer(256)
                user.GetClassNameW(hwnd, name, 256)
                if pid.value == process.pid and user.IsWindowVisible(hwnd) and name.value == "Nexora.Window":
                    windows.append(hwnd)
                return True

            user.EnumWindows(collect, 0)
            require(len(windows) == 1, "Expected exactly one visible owned Editor main window")
            hwnd = windows[0]
            chord(hwnd)  # Actual Unity Play command, never --play or a synthetic host callback.
            previous = None
            for version in range(args.cycles + 1):
                if version:
                    edit(version)
                ready = wait(lambda text: re.findall(
                    rf"script reload gameplay: live ready version={version} counter=(\d+) scene=(\d+) entity=(\d+)", text),
                    f"Version {version} failed to restore its World/state")[-1]
                ready = tuple(map(int, ready))
                wait(lambda text: len(re.findall(r"script reload admission: .* applied=1 ", text)) >= version + 1,
                     f"Version {version} did not commit")
                wait(lambda text: f"script reload gameplay: live tick version={version} " in text,
                     f"Version {version} did not dispatch after commit")
                if previous:
                    require(ready[0] > previous[0] and ready[1:] == previous[1:],
                            "Counter or Scene/entity handle reset across a real source edit")
                previous = ready
                sequence.append({"version": version, "counter": ready[0], "scene": ready[1], "entity": ready[2]})
                checkpoint("replacement-" + str(version))
                result = C.c_size_t()
                require(user.SendMessageTimeoutW(hwnd, 0, 0, 0, 2, 500, C.byref(result)),
                        "Editor UI became unresponsive during source replacement")
            # Exercise production SDK/compiler/admission failures through this same real Play.
            # Each old update checks its World before writing, so a failed candidate cannot hide
            # leaked World mutations by overwriting them on the next tick.
            active_version = args.cycles
            admissions = re.findall(r"script reload admission: .* applied=(\d) generation=(\d+) ", output())
            require(admissions and admissions[-1][0] == "1", "No admitted live generation before faults")
            generation = int(admissions[-1][1])
            for index, kind in enumerate(("compile", "restore", "initialize", "start"), 1):
                before = len(re.findall(r"script reload admission: .* applied=0 ", output()))
                bad_version = args.cycles + index
                write_source(fault_source(bad_version, kind))
                failed = wait(lambda text: re.findall(
                    r"script reload admission: .* applied=0 generation=(\d+) message=([^\r\n]*)", text)[before:],
                    kind + " fault did not produce a rejected production admission")
                require(len(failed) == 1 and int(failed[0][0]) == generation,
                        kind + " fault replaced the active module")
                offset = len(output())
                tick = wait(lambda text: re.findall(
                    rf"script reload gameplay: live tick version={active_version} counter=(\d+) scene=(\d+) entity=(\d+)",
                    text[offset:]), kind + " fault stopped old gameplay dispatch")[-1]
                tick = tuple(map(int, tick))
                require(tick[0] > previous[0] and tick[1:] == previous[1:],
                        kind + " fault changed old World/state/handles")
                require(not re.search(rf"script reload gameplay: live (?:ready|tick) version={bad_version} ", output()),
                        kind + " fault dispatched an uncommitted generation")
                result = C.c_size_t()
                require(user.SendMessageTimeoutW(hwnd, 0, 0, 0, 2, 500, C.byref(result)),
                        "Editor UI became unresponsive after " + kind + " failure")
                faults.append({"kind": kind, "candidate_version": bad_version, "generation": generation,
                               "counter": tick[0], "scene": tick[1], "entity": tick[2],
                               "rejected": True, "old_dispatch_continued": True, "diagnostic": failed[0][1]})
                previous = tick
                checkpoint("rejected-" + kind)
            recovery_version = args.cycles + 8
            before = len(re.findall(r"script reload admission: .* applied=1 ", output()))
            edit(recovery_version)
            recovered = wait(lambda text: re.findall(
                rf"script reload gameplay: live ready version={recovery_version} counter=(\d+) scene=(\d+) entity=(\d+)", text),
                "Valid source did not recover after rejected candidates")[-1]
            recovered = tuple(map(int, recovered))
            wait(lambda text: len(re.findall(r"script reload admission: .* applied=1 ", text)) > before,
                 "Recovered source did not commit")
            wait(lambda text: f"script reload gameplay: live tick version={recovery_version} " in text,
                 "Recovered generation did not dispatch")
            require(recovered[0] > previous[0] and recovered[1:] == previous[1:],
                    "Recovery reset state or retained candidate World corruption")
            recovery = {"version": recovery_version, "counter": recovered[0],
                        "scene": recovered[1], "entity": recovered[2], "committed": True}
            checkpoint("recovered")
            progress_before = output().count("script reload progress: stage=4 ")
            edit(recovery_version + 1)
            wait(lambda text: text.count("script reload progress: stage=4 ") > progress_before,
                 "Cancellation fixture did not start real compilation")
            wait(lambda text: running_compiler_children(), "No live owned compiler process observed", 5)
            stopped_before = output().count("script reload stopped:")
            chord(hwnd)
            wait(lambda text: text.count("script reload stopped:") > stopped_before,
                 "Stop did not drain compilation", 30)
            require(all(kernel.WaitForSingleObject(handle, 0) == 0 for handle, pid, name in children),
                    "An observed compiler child survived completed Stop/drain")
            user.PostMessageW(hwnd, 0x10, 0, 0)
            require(process.wait(timeout=10) == 0, "Normal Editor close failed")
            account = Accounting()
            drain_deadline = time.monotonic() + 5
            while True:
                require(kernel.QueryInformationJobObject(job, 1, C.byref(account), C.sizeof(account), None),
                        "Cannot verify the owned Editor process tree drained")
                if not account.active_processes or time.monotonic() >= drain_deadline:
                    break
                time.sleep(.02)
            require(account.active_processes == 0, "Owned Editor/SDK processes survived normal close")
        require(all(hashlib.sha256((root / name).read_bytes()).hexdigest() == digest
                    for name, digest in baseline.items()), "Play or compiler changed authored project files")
        receipt = {"status": "PASS", "cycles": args.cycles, "full_32_cycle_gate": args.cycles >= 32,
                   "actual_ctrl_p": True, "preserved_mtime_edits": True, "sequence": sequence,
                   "stop_cancelled_real_compilation": True, "exit": process.returncode,
                   "observed_children_terminated": [{"pid": pid, "name": name} for handle, pid, name in children],
                   "remaining_owned_job_processes": account.active_processes,
                   "active_generation_before_faults": generation,
                   "faults": faults, "recovery": recovery,
                   "authored_baseline": baseline, "scope": "Windows native Editor configured 3D Play; no Sprite/device acceptance"}
        (evidence / "receipt.json").write_text(json.dumps(receipt, indent=2), encoding="utf-8")
        print(json.dumps(receipt))
    except Exception as error:
        (evidence / "failure.json").write_text(json.dumps({
            "status": "FAIL", "error": str(error), "sequence": sequence,
            "completed_replacements": max(0, len(sequence) - 1),
            "faults": faults,
            "editor_exit_code_before_cleanup": process.poll() if process else None,
        }, indent=2), encoding="utf-8")
        raise
    finally:
        for handle, pid, name in children:
            kernel.CloseHandle(handle)
        try:
            if process and process.poll() is None:
                process.kill()
                process.wait(timeout=10)
        finally:
            if job:
                kernel.CloseHandle(job)


if __name__ == "__main__":
    main()
