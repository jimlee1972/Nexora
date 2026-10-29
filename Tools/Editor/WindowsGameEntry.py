"""Native exported Game.exe admission: ordinary flags, no console suppression or shell."""
import ctypes as C
from ctypes import wintypes as W
import json
import os
from pathlib import Path
import subprocess
import time


def verify_player_cli(player, evidence, environment):
    from PackageWindowsEditor import require_gui_subsystem
    require_gui_subsystem(player)
    evidence.mkdir(parents=True, exist_ok=True)
    environment = dict(environment, NEXORA_PLAYER_STARTUP_EVIDENCE='1')
    for label, arguments, expected in (
            ('help', ['--help'], 2),
            ('private-no-arguments', [], 2),
            ('unicode-invalid-package', ['--verify-game-package', str(evidence / '不存在 package.nxgame')], 1)):
        result = subprocess.run([str(player), *arguments], cwd=evidence, env=environment,
                                capture_output=True, timeout=15, creationflags=0)
        (evidence / (label + '.stdout.log')).write_bytes(result.stdout)
        (evidence / (label + '.stderr.log')).write_bytes(result.stderr)
        if result.returncode != expected or b'player GUI startup: console_attached=0' not in result.stderr:
            raise RuntimeError(f'Player GUI/Unicode/redirected CLI failed: {label}, exit {result.returncode}')
        if label == 'help' and b'--verify-game-package' not in result.stderr:
            raise RuntimeError('Player usage diagnostics lost')


def verify_installed_game(game, private_player, evidence, environment, cwd, missing_handles=False):
    from PackageWindowsEditor import require_gui_subsystem, digest
    require_gui_subsystem(game)
    require_gui_subsystem(private_player)
    if digest(game) != digest(private_player):
        raise RuntimeError('Exported Game.exe is not the shipped native player')
    evidence.mkdir(parents=True, exist_ok=True)
    user = C.WinDLL('user32', use_last_error=True)
    kernel = C.WinDLL('kernel32', use_last_error=True)
    user.IsWindowVisible.argtypes = [W.HWND]
    user.GetWindowThreadProcessId.argtypes = [W.HWND, C.POINTER(W.DWORD)]
    user.GetClassNameW.argtypes = [W.HWND, W.LPWSTR, C.c_int]
    user.SendMessageTimeoutW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM,
                                       W.UINT, W.UINT, C.POINTER(C.c_size_t)]
    user.SendMessageTimeoutW.restype = C.c_ssize_t
    user.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
    callback_type = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    user.EnumWindows.argtypes = [callback_type, W.LPARAM]
    kernel.CreateToolhelp32Snapshot.argtypes = [W.DWORD, W.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = W.HANDLE
    kernel.CloseHandle.argtypes = [W.HANDLE]

    class ProcessEntry(C.Structure):
        _fields_ = [('size', W.DWORD), ('usage', W.DWORD), ('pid', W.DWORD),
                    ('heap', C.c_size_t), ('module', W.DWORD), ('threads', W.DWORD),
                    ('parent', W.DWORD), ('priority', W.LONG), ('flags', W.DWORD),
                    ('executable', W.WCHAR * 260)]
    for name in ('Process32FirstW', 'Process32NextW'):
        getattr(kernel, name).argtypes = [W.HANDLE, C.POINTER(ProcessEntry)]

    def windows():
        found = []
        @callback_type
        def visit(hwnd, _):
            if user.IsWindowVisible(hwnd):
                pid = W.DWORD()
                user.GetWindowThreadProcessId(hwnd, C.byref(pid))
                name = C.create_unicode_buffer(256)
                user.GetClassNameW(hwnd, name, len(name))
                found.append((int(hwnd), pid.value, name.value))
            return True
        if not user.EnumWindows(visit, 0):
            raise C.WinError(C.get_last_error())
        return found

    def children(pid):
        snapshot = kernel.CreateToolhelp32Snapshot(2, 0)
        if snapshot == C.c_void_p(-1).value:
            raise C.WinError(C.get_last_error())
        try:
            entry = ProcessEntry()
            entry.size = C.sizeof(entry)
            result = []
            available = kernel.Process32FirstW(snapshot, C.byref(entry))
            if not available:
                raise C.WinError(C.get_last_error())
            while available:
                if entry.parent == pid:
                    result.append({'pid': entry.pid, 'name': entry.executable})
                available = kernel.Process32NextW(snapshot, C.byref(entry))
            if C.get_last_error() != 18:  # ERROR_NO_MORE_FILES is the complete snapshot boundary.
                raise C.WinError(C.get_last_error())
            return result
        finally:
            kernel.CloseHandle(snapshot)

    baseline_consoles = {hwnd for hwnd, _, cls in windows() if cls == 'ConsoleWindowClass'}
    environment = dict(environment, NEXORA_PLAYER_STARTUP_EVIDENCE='1')
    stdout_path, stderr_path = evidence / 'stdout.log', evidence / 'stderr.log'
    probes, observations, close_sent, first_response = 0, [], False, None
    # The no-argument executable-relative Game.launch is the actual double-click path.
    with stdout_path.open('wb') as stdout, stderr_path.open('wb') as stderr:
        startup = subprocess.STARTUPINFO()
        if missing_handles:
            # Explorer-style absence, not console suppression. Keep creationflags=0.
            startup.dwFlags = subprocess.STARTF_USESTDHANDLES
            startup.hStdInput = startup.hStdOutput = startup.hStdError = 0
        process = subprocess.Popen([str(game)], cwd=cwd, env=environment,
                                   stdout=None if missing_handles else stdout,
                                   stderr=None if missing_handles else stderr,
                                   startupinfo=startup, creationflags=0)
        deadline = time.monotonic() + 45
        try:
            while process.poll() is None and not close_sent:
                now = time.monotonic()
                if now > deadline:
                    raise RuntimeError('No-argument game initialization timed out')
                visible = windows()
                extra = children(process.pid)
                consoles = [x for x in visible if x[2] == 'ConsoleWindowClass' and x[0] not in baseline_consoles]
                own = [x for x in visible if x[1] == process.pid]
                if extra or consoles or len(own) > 1:
                    raise RuntimeError(f'Game created extra processes/windows: {extra}, {consoles}, {own}')
                if own:
                    ready = 'player native ready: presented_frames=2' in stderr_path.read_text(encoding='utf-8')
                    if not ready and not missing_handles:
                        time.sleep(0.02)
                        continue
                    result = C.c_size_t()
                    if not user.SendMessageTimeoutW(own[0][0], 0, 0, 0, 2, 500, C.byref(result)):
                        if ready:
                            raise RuntimeError('Presented game window is unresponsive')
                        time.sleep(0.02)
                        continue
                    probes += 1
                    first_response = first_response or now
                    if not observations:
                        observations.append({'pid': process.pid, 'class': own[0][2], 'visible_windows': len(own)})
                    # Redirected run waits for two successful presents. The separate no-handle run
                    # observes a responsive native loop, then requires the real close path exit 0.
                    if probes >= 3 and (ready or (missing_handles and now - first_response >= 1)):
                        if not user.PostMessageW(own[0][0], 0x10, 0, 0):
                            raise C.WinError(C.get_last_error())
                        close_sent = True
                time.sleep(0.02)
            process.wait(timeout=10)
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=10)
    report = json.loads(stdout_path.read_text(encoding='utf-8')) if process.returncode == 0 and not missing_handles else {}
    stderr = stderr_path.read_text(encoding='utf-8')
    if (not close_sent or probes < 3 or process.returncode != 0 or
            (not missing_handles and (
                'player GUI startup: console_attached=0' not in stderr or
                report.get('status') != 'RENDERED_GAME_PACKAGE' or
                report.get('presented_frames', 0) < 2 or report.get('gameplay_updates', 0) < 1 or
                not report.get('gameplay_loaded') or not report.get('drained'))) or
            children(process.pid) or any(pid == process.pid for _, pid, _ in windows())):
        raise RuntimeError(f'No-argument game/close admission failed: exit {process.returncode}, {report}, {stderr}')
    receipt = {'status': 'PASS', 'argv': [], 'creationflags': 0, 'subsystem': 2,
               'private_player_matches': True, 'standard_handles_absent': missing_handles,
               'console_attached': False if not missing_handles else 'entry separately verified',
               'response_probes': probes, 'window_observations': observations,
               'child_processes': [], 'normal_close_exit': process.returncode, 'runtime': report}
    (evidence / 'result.json').write_text(json.dumps(receipt, indent=2), encoding='utf-8')
    return receipt


if __name__ == '__main__':
    import sys
    verify_player_cli(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve(), os.environ)
    print('Native player GUI PE, console-free Unicode entry and redirected CLI passed')
