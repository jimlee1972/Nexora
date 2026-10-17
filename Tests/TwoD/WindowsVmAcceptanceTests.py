#!/usr/bin/env python3
"""Regress the observed NUL inventory, CLIXML transport and overlapping-run failures."""
import importlib.util
import base64
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import unittest
from unittest import mock
import zipfile

SOURCE = Path(__file__).resolve().parents[2] / 'Tools/TwoD/WindowsVmAcceptance.py'
spec = importlib.util.spec_from_file_location('windows_vm', SOURCE)
vm = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vm)


class AcceptanceTests(unittest.TestCase):
    def test_failed_cleanup_cannot_reuse_an_older_cycles_poweroff(self):
        client = mock.Mock()
        with mock.patch.object(vm, 'vm_info', return_value={'VMState': 'running'}), \
                mock.patch.object(vm, 'wait_for', side_effect=RuntimeError('cleanup deadline')):
            result = vm.cleanup_after_failure(client, 'fixture')
        self.assertEqual(result['final_vm_state'], 'unknown')
        self.assertEqual(result['last_observed_vm_state'], 'running')
        self.assertEqual(result['cleanup_error'], 'cleanup deadline')
        client.call.assert_called_once_with(['controlvm', 'fixture', 'acpipowerbutton'])

    def test_system_service_readiness_does_not_admit_native_desktop_tests(self):
        self.assertFalse(vm.desktop_ready({'VMState': 'running', 'GuestAdditionsRunLevel': '2'}))
        self.assertFalse(vm.desktop_ready({'VMState': 'poweroff', 'GuestAdditionsRunLevel': '3'}))
        self.assertTrue(vm.desktop_ready({'VMState': 'running', 'GuestAdditionsRunLevel': '3'}))

    def test_zero_filled_inventory_is_not_an_encoding_repair(self):
        with self.assertRaisesRegex(ValueError, 'NUL'):
            vm.read_json_bytes(bytes(806))
        with self.assertRaises(UnicodeDecodeError):
            vm.read_json_bytes(b'{"x":"\xff"}')
        with self.assertRaisesRegex(ValueError, 'duplicate'):
            vm.read_json_bytes(b'{"x":1,"x":2}')
        self.assertEqual(vm.read_json_bytes(b'\xef\xbb\xbf{"x":1}'), {'x': 1})

    def package(self, path, corrupt=False, extra=None):
        with zipfile.ZipFile(path, 'w') as archive:
            for package in vm.PACKAGES:
                files = {name: (package + name).encode() for name in
                         ('NexoraTwoDPlayer.exe', 'gameplay.dll', 'game.nx2game', 'Run.cmd')}
                inventory = {'schema': 1, 'development_test_copy': False,
                             'files': {name: vm.digest(data) for name, data in files.items()}}
                for name, data in files.items():
                    archive.writestr(package + '/' + name, data + (b'bad' if corrupt else b''))
                archive.writestr(package + '/inventory.json', json.dumps(inventory))
            if extra:
                archive.writestr(extra, b'unlisted')

    def test_zip_requires_real_hashes_and_closed_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            good, bad, extra = (root / name for name in ('good.zip', 'bad.zip', 'extra.zip'))
            self.package(good)
            self.assertEqual(set(vm.inspect_package(good)), set(vm.PACKAGES))
            self.package(bad, corrupt=True)
            with self.assertRaisesRegex(ValueError, 'hash mismatch'):
                vm.inspect_package(bad)
            self.package(extra, extra='hidden.dll')
            with self.assertRaisesRegex(ValueError, 'outside'):
                vm.inspect_package(extra)

    def test_windows_paths_reject_traversal_and_case_aliases(self):
        with tempfile.TemporaryDirectory() as directory:
            for index, extra in enumerate(('../outside', 'UI-Shipping/GAMEPLAY.DLL', 'bad:stream', 'bad\\file')):
                path = Path(directory) / ('bad%d.zip' % index)
                self.package(path, extra=extra)
                with self.assertRaises(ValueError):
                    vm.inspect_package(path)

    def test_clixml_stderr_remains_bytes_and_nonzero_still_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            code = "import sys;sys.stdout.buffer.write(b'PASS\\n');sys.stderr.buffer.write(b'#< CLIXML\\r\\n<broken>\\xff')"
            client = vm.VBox([sys.executable, '-c', code], output)
            self.assertEqual(client.call(['run']), b'PASS\n')
            self.assertEqual((output / 'command-001.stderr').read_bytes(), b'#< CLIXML\r\n<broken>\xff')
            client = vm.VBox([sys.executable, '-c', code + ';sys.exit(7)'], output)
            with self.assertRaisesRegex(RuntimeError, 'exited 7'):
                client.call(['run', '--passwordfile', 'private-path', '--username', 'private-user'])
            logged = (output / 'command-001.json').read_text()
            self.assertNotIn('private-path', logged)
            self.assertNotIn('private-user', logged)

    def test_cli_deadline_does_not_turn_into_an_automatic_retry(self):
        with tempfile.TemporaryDirectory() as directory:
            client = vm.VBox([sys.executable, '-c', 'import time;time.sleep(20)'], directory)
            with self.assertRaisesRegex(RuntimeError, 'do not retry'):
                client.call(['run'], timeout=0.2)
            self.assertEqual(client.sequence, 1)

    def test_no_concurrent_command_on_a_client(self):
        entered, release = threading.Event(), threading.Event()
        def runner(*args, **kwargs):
            entered.set()
            if not release.wait(5):
                raise RuntimeError('test synchronization failed')
            return subprocess.CompletedProcess([], 0, b'ok', b'')
        with tempfile.TemporaryDirectory() as directory:
            client = vm.VBox(['unused'], directory, runner)
            thread = threading.Thread(target=lambda: client.call(['run']))
            thread.start()
            self.assertTrue(entered.wait(5))
            try:
                with self.assertRaisesRegex(RuntimeError, 'overlapping'):
                    client.call(['copyfrom'])
            finally:
                release.set()
                thread.join(5)
            self.assertFalse(thread.is_alive())

    def test_cross_process_vm_lock_releases_without_removing_a_lock_file(self):
        code = ("import importlib.util,sys; s=importlib.util.spec_from_file_location('vm',sys.argv[1]);"
                "m=importlib.util.module_from_spec(s);s.loader.exec_module(m);"
                "\nwith m.vm_lock('same-uuid',sys.argv[2]): print('owned')")
        with tempfile.TemporaryDirectory() as directory:
            with vm.vm_lock('same-uuid', directory):
                blocked = subprocess.run([sys.executable, '-c', code, str(SOURCE), directory], capture_output=True)
                self.assertNotEqual(blocked.returncode, 0)
                self.assertIn(b'another acceptance process', blocked.stderr)
            free = subprocess.run([sys.executable, '-c', code, str(SOURCE), directory], capture_output=True)
            self.assertEqual(free.returncode, 0, free.stderr)
            self.assertEqual(len(list(Path(directory).glob('*.lock'))), 1)

    def test_resource_guard_precedes_starting_the_vm(self):
        with self.assertRaisesRegex(RuntimeError, 'RAM headroom'):
            vm.check_headroom(6144, 1024, available=4961)
        self.assertEqual(vm.check_headroom(3072, 1024, available=4961), 4961)

    def test_poweroff_waits_for_ram_reclamation_without_weakening_the_guard(self):
        with mock.patch.object(vm, 'available_mib', side_effect=[3500, 4500]), mock.patch.object(vm.time, 'sleep'):
            self.assertEqual(vm.wait_for_headroom(3072, 1024), 4500)
        with mock.patch.object(vm, 'available_mib', return_value=3500):
            with self.assertRaisesRegex(RuntimeError, 'RAM headroom'):
                vm.wait_for_headroom(3072, 1024, timeout=0)

    def test_a_pass_prefix_or_an_old_report_is_not_completion(self):
        with self.assertRaises(ValueError):
            vm.validate_report(b'{"status":"PASS"}', 'run-new', 1)
        with self.assertRaises(ValueError):
            vm.validate_report(json.dumps({'schema': 1, 'status': 'PASS', 'run_id': 'run-old', 'cycle': 1,
                                          'hashes_verified': True, 'corrupt_save_preserved': True,
                                          'player_processes': 0, 'cases': []}).encode(), 'run-new', 1)


if __name__ == '__main__':
    if sys.argv[1:] == ['--guest-tests']:
        script = SOURCE.with_name('WindowsVmGuestAcceptance.ps1')
        tests = Path(__file__).with_name('WindowsVmGuestTests.ps1')
        entry = ("& ([scriptblock]::Create([IO.File]::ReadAllText('" + str(tests).replace("'", "''")
                 + "',[Text.UTF8Encoding]::new($false,$true)))) -GuestScript '"
                 + str(script).replace("'", "''") + "'")
        result = subprocess.run(['powershell.exe', '-NoProfile', '-NonInteractive', '-OutputFormat', 'Text',
                                 '-EncodedCommand', base64.b64encode(entry.encode('utf-16-le')).decode()],
                                capture_output=True, timeout=50)
        sys.stdout.buffer.write(result.stdout)
        sys.stderr.buffer.write(result.stderr)
        raise SystemExit(result.returncode)
    unittest.main()
