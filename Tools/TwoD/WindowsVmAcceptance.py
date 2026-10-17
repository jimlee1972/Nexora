#!/usr/bin/env python3
"""Serialized, byte-preserving acceptance of the two exported Windows Player fixtures."""
import argparse
import base64
from contextlib import contextmanager
import ctypes
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import tempfile
import threading
import time
import uuid
import zipfile


PACKAGES = ('UI-Shipping', 'ZigGame-Shipping-Fixture')


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate JSON key: ' + key)
        result[key] = value
    return result


def read_json_bytes(data):
    if b'\0' in data:
        raise ValueError('JSON contains NUL bytes; preserve and replace the damaged staging copy')
    return json.loads(data.decode('utf-8-sig'), object_pairs_hook=unique_object)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def inspect_package(path):
    """Admit only hash-verified Shipping inventory members, with Windows path semantics."""
    with zipfile.ZipFile(path) as archive:
        names = {}
        total = 0
        for entry in archive.infolist():
            name = entry.filename.rstrip('/')
            parts = PurePosixPath(name).parts
            if (not parts or name.startswith('/') or '\\' in name or ':' in name
                    or any(p in ('', '.', '..') or p.endswith((' ', '.')) for p in name.split('/'))
                    or any(ord(c) < 32 for c in name)
                    or (entry.external_attr >> 16) & 0o170000 == 0o120000):
                raise ValueError('unsafe ZIP member')
            folded = name.casefold()
            if folded in names:
                raise ValueError('duplicate Windows ZIP path')
            names[folded] = entry
            total += entry.file_size
        if total > 128 * 1024 * 1024:
            raise ValueError('fixture ZIP exceeds 128 MiB uncompressed')
        expected = set()
        manifests = {}
        for package in PACKAGES:
            key = package + '/inventory.json'
            if key.casefold() not in names:
                raise ValueError('missing fixture inventory: ' + key)
            inventory = read_json_bytes(archive.read(names[key.casefold()]))
            if (inventory.get('schema') != 1 or inventory.get('development_test_copy') is not False
                    or not isinstance(inventory.get('files'), dict) or not inventory['files']):
                raise ValueError('requires a Shipping fixture inventory')
            expected.add(key.casefold())
            for relative, checksum in inventory['files'].items():
                if (not isinstance(relative, str) or not relative or '\\' in relative or ':' in relative
                        or relative.startswith('/') or any(p in ('', '.', '..') for p in relative.split('/'))
                        or not isinstance(checksum, str) or not re.fullmatch('[0-9a-f]{64}', checksum)):
                    raise ValueError('invalid inventory entry')
                member = (package + '/' + relative).casefold()
                if member in expected or member not in names or names[member].is_dir():
                    raise ValueError('missing or duplicate inventory member')
                expected.add(member)
                if digest(archive.read(names[member])) != checksum:
                    raise ValueError('ZIP inventory hash mismatch: ' + relative)
            if not {'NexoraTwoDPlayer.exe', 'gameplay.dll', 'game.nx2game', 'Run.cmd'} <= inventory['files'].keys():
                raise ValueError('incomplete Player fixture')
            manifests[package] = inventory
        actual = {name for name, entry in names.items() if not entry.is_dir()}
        if actual != expected:
            raise ValueError('ZIP contains files outside its inventories')
        return manifests


@contextmanager
def vm_lock(identity, directory=None):
    """Use an OS lock: a crash releases it, and aliases resolve to the same VM UUID."""
    root = Path(directory or tempfile.gettempdir())
    path = root / ('nexora-vm-' + digest(identity.encode()) + '.lock')
    with path.open('a+b') as file:
        if not path.stat().st_size:
            file.write(b'0')
            file.flush()
        file.seek(0)
        try:
            if os.name == 'nt':
                import msvcrt
                msvcrt.locking(file.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(file, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise RuntimeError('another acceptance process owns this VM') from error
        try:
            yield
        finally:
            file.seek(0)
            if os.name == 'nt':
                msvcrt.locking(file.fileno(), msvcrt.LK_UNLCK, 1)
            else:
                fcntl.flock(file, fcntl.LOCK_UN)


class VBox:
    def __init__(self, command, output, runner=subprocess.run):
        self.command = list(command)
        self.output = Path(output)
        self.runner = runner
        self.active = threading.Lock()
        self.sequence = 0

    def call(self, arguments, timeout=90):
        if not self.active.acquire(blocking=False):
            raise RuntimeError('overlapping VBoxManage operation')
        try:
            self.sequence += 1
            prefix = self.output / ('command-%03d' % self.sequence)
            redacted = list(arguments)
            for flag in ('--passwordfile', '--username'):
                if flag in redacted:
                    redacted[redacted.index(flag) + 1] = '<redacted>'
            prefix.with_suffix('.json').write_text(json.dumps(redacted) + '\n', encoding='utf-8')
            try:
                result = self.runner(self.command + list(arguments), capture_output=True, timeout=timeout)
            except subprocess.TimeoutExpired as error:
                prefix.with_suffix('.stdout').write_bytes(error.stdout or b'')
                prefix.with_suffix('.stderr').write_bytes(error.stderr or b'')
                raise RuntimeError('VBoxManage timed out; do not retry an uncertain guest operation') from error
            prefix.with_suffix('.stdout').write_bytes(result.stdout)
            prefix.with_suffix('.stderr').write_bytes(result.stderr)
            # CLIXML progress on stderr is diagnostic bytes, not a host PowerShell error record.
            if result.returncode:
                raise RuntimeError('VBoxManage exited %d; see %s.stderr' % (result.returncode, prefix.name))
            return result.stdout
        finally:
            self.active.release()


def vm_info(client, vm):
    lines = client.call(['showvminfo', vm, '--machinereadable']).decode('utf-8', errors='replace').splitlines()
    return {k: v.strip('"') for line in lines for k, sep, v in [line.partition('=')] if sep}


def wait_for(client, vm, predicate, timeout):
    deadline = time.monotonic() + timeout
    while True:
        info = vm_info(client, vm)
        if predicate(info):
            return info
        if info.get('VMState') == 'aborted':
            raise RuntimeError('VM aborted; preserve its logs and disk')
        if time.monotonic() >= deadline:
            raise RuntimeError('VM readiness/state deadline exceeded')
        time.sleep(2)


def desktop_ready(info):
    # Additions run level 2 only admits system services; native DXGI needs the desktop (3).
    return info.get('VMState') == 'running' and info.get('GuestAdditionsRunLevel') == '3'


def cleanup_after_failure(client, vm):
    result = {'final_vm_state': 'unknown'}
    try:
        state = vm_info(client, vm).get('VMState')
        result['last_observed_vm_state'] = state
        if state == 'running':
            client.call(['controlvm', vm, 'acpipowerbutton'])
            wait_for(client, vm, lambda i: i.get('VMState') == 'poweroff', 45)
        result['final_vm_state'] = vm_info(client, vm).get('VMState', 'unknown')
    except Exception as error:
        result['cleanup_error'] = str(error)
        # An older cycle's poweroff cannot describe this failed cleanup.
    return result


def available_mib():
    if os.name != 'nt':
        return None
    class Memory(ctypes.Structure):
        _fields_ = [('length', ctypes.c_ulong), ('load', ctypes.c_ulong)] + [
            (name, ctypes.c_ulonglong) for name in ('total', 'available', 'page_total', 'page_available',
                                                   'virtual_total', 'virtual_available', 'extended')]
    memory = Memory()
    memory.length = ctypes.sizeof(memory)
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(memory)):
        raise OSError('GlobalMemoryStatusEx failed')
    return memory.available // (1024 * 1024)


def check_headroom(memory_mib, reserve_mib, available=None):
    if available is None:
        available = available_mib()
    if available is not None and available < memory_mib + reserve_mib:
        raise RuntimeError('insufficient host RAM headroom; shut down normally and reduce VM RAM')
    return available


def wait_for_headroom(memory_mib, reserve_mib, timeout=60):
    # VirtualBox can publish poweroff before its frontend releases the guest RAM.
    # Wait for measured availability; never lower the reserve or retry guest work.
    deadline = time.monotonic() + timeout
    while True:
        available = available_mib()
        if available is None or available >= memory_mib + reserve_mib:
            return available
        if time.monotonic() >= deadline:
            check_headroom(memory_mib, reserve_mib, available)
        time.sleep(2)


def validate_report(data, run_id, cycle):
    report = read_json_bytes(data)
    if (report.get('schema') != 1 or report.get('status') != 'PASS'
            or report.get('run_id') != run_id or report.get('cycle') != cycle
            or report.get('hashes_verified') is not True or report.get('corrupt_save_preserved') is not True
            or type(report.get('desktop_session')) is not int or report['desktop_session'] <= 0
            or report.get('player_processes') != 0):
        raise ValueError('missing, stale or incomplete guest report')
    cases = report.get('cases', [])
    expected = {'ui-first': 0, 'ui-restart': 0, 'ui-corrupt-save': 1, 'ui-unicode-save': 0,
                'zig-native': 0, 'zig-corrupt-cook': 1, 'ui-window-close': 0}
    if len(cases) != len(expected) or {c.get('name') for c in cases} != set(expected):
        raise ValueError('incomplete guest cases')
    for case in cases:
        if type(case.get('exit')) is not int or case['exit'] != expected[case['name']]:
            raise ValueError('guest process exit mismatch')
        if case['name'] == 'ui-window-close' and case.get('panel_observed') is not True:
            raise ValueError('window close was not preceded by a presented fixture panel')
        if case['exit'] == 0:
            result = case.get('report', {})
            minimum = 120 if case['name'] == 'zig-native' else 1
            if (result.get('status') != 'PASS' or result.get('project_mode') is not True
                    or result.get('allocations_balanced') is not True
                    or result.get('native_frames', 0) < minimum):
                raise ValueError('missing native process completion')
    return report


def run(args):
    inspect_package(args.package_zip)
    args.output.mkdir(parents=True, exist_ok=False)
    client = VBox([args.vboxmanage], args.output)
    initial = vm_info(client, args.vm)
    identity = initial.get('UUID')
    if not identity:
        raise RuntimeError('VM UUID unavailable')
    run_id = 'run-' + uuid.uuid4().hex
    root = 'C:\\NexoraVmAcceptance\\' + run_id
    credentials = ['--username', args.username, '--passwordfile', str(args.password_file.resolve())]
    guest_script = Path(__file__).with_name('WindowsVmGuestAcceptance.ps1')
    script_sha = digest(guest_script.read_bytes())
    zip_sha = digest(args.package_zip.read_bytes())
    summary = {'schema': 1, 'run_id': run_id, 'vm_uuid': identity, 'package_zip_sha256': zip_sha,
               'guest_script_sha256': script_sha, 'cycles': [], 'status': 'FAILED',
               'physical_devices': 'NOT VERIFIED', 'frontend': args.frontend}
    with vm_lock(identity):
        initial = vm_info(client, args.vm)
        if initial.get('VMState') != 'poweroff':
            raise RuntimeError('start from a normally powered-off VM; no saved state is discarded')
        summary['vm_memory_mib'] = int(initial['memory'])
        check_headroom(summary['vm_memory_mib'], args.reserve_mib)
        started = False
        try:
            for cycle in range(1, args.cycles + 1):
                headroom = wait_for_headroom(summary['vm_memory_mib'], args.reserve_mib)
                summary.setdefault('host_available_mib_before_boot', []).append(headroom)
                summary['final_vm_state'] = 'unknown'
                client.call(['startvm', args.vm, '--type', args.frontend])
                started = True
                wait_for(client, args.vm, desktop_ready, args.boot_timeout)
                if cycle == 1:
                    client.call(['guestcontrol', args.vm, 'mkdir', *credentials, '--parents', root])
                    for source, name in ((args.package_zip, 'package.zip'), (guest_script, 'guest.ps1')):
                        client.call(['guestcontrol', args.vm, 'copyto', *credentials, str(source.resolve()), root + '\\' + name])
                entry = ("$ProgressPreference='SilentlyContinue';$ErrorActionPreference='Stop';"
                         "$p='" + root + "\\guest.ps1';"
                         "if((Get-FileHash -LiteralPath $p).Hash.ToLowerInvariant() -ne '" + script_sha + "'){throw 'script hash'};"
                         "& ([scriptblock]::Create([IO.File]::ReadAllText($p,[Text.UTF8Encoding]::new($false,$true))))"
                         " -Root '" + root + "' -Cycle " + str(cycle) + " -ExpectedZipSha '" + zip_sha + "' -ScheduleInteractive")
                encoded = base64.b64encode(entry.encode('utf-16-le')).decode('ascii')
                client.call(['guestcontrol', args.vm, 'run', *credentials, '--timeout', str(args.guest_timeout * 1000),
                             '--exe', 'C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe',
                             '--wait-stdout', '--wait-stderr', '--', '-NoProfile', '-NonInteractive',
                             '-OutputFormat', 'Text', '-EncodedCommand', encoded], args.guest_timeout + 30)
                destination = args.output / ('cycle-%d.json' % cycle)
                client.call(['guestcontrol', args.vm, 'copyfrom', *credentials,
                             root + '\\cycle-%d.json' % cycle, str(destination.resolve())])
                report = validate_report(destination.read_bytes(), run_id, cycle)
                summary['cycles'].append(report)
                client.call(['guestcontrol', args.vm, 'run', *credentials, '--timeout', '30000', '--exe',
                             'C:\\Windows\\System32\\shutdown.exe', '--wait-stdout', '--wait-stderr', '--', '/s', '/t', '0'])
                wait_for(client, args.vm, lambda i: i.get('VMState') == 'poweroff', args.boot_timeout)
                started = False
                summary['final_vm_state'] = 'poweroff'
                print('Cycle %d: verified native cases, durable restart and poweroff' % cycle, flush=True)
            summary['status'] = 'PASS'
            summary['final_vm_state'] = 'poweroff'
        finally:
            if started:
                summary.update(cleanup_after_failure(client, args.vm))
                # No force-off, session-close or automatic retry of uncertain guest work.
            (args.output / 'acceptance.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--vm', required=True)
    parser.add_argument('--username', required=True)
    parser.add_argument('--password-file', required=True, type=Path)
    parser.add_argument('--package-zip', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--vboxmanage', default=shutil.which('VBoxManage') or 'VBoxManage')
    parser.add_argument('--frontend', choices=('gui', 'headless'), default='gui',
                        help='native window acceptance needs an available guest desktop')
    parser.add_argument('--cycles', type=int, default=2)
    parser.add_argument('--reserve-mib', type=int, default=1024)
    parser.add_argument('--boot-timeout', type=int, default=300)
    parser.add_argument('--guest-timeout', type=int, default=300)
    args = parser.parse_args()
    if not 2 <= args.cycles <= 10 or min(args.reserve_mib, args.boot_timeout, args.guest_timeout) <= 0:
        parser.error('require 2..10 cycles and positive resource/deadline settings')
    run(args)
    print('Windows VM acceptance PASS; physical device/product gates remain separate')


if __name__ == '__main__':
    main()
