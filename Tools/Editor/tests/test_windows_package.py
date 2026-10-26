"""Synthetic dependency admission tests; these do not execute Windows binaries."""
import sys
import struct
from pathlib import Path
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PackageWindowsEditor import dependency_closure, relocated_environment, stage_game_build_runtime, require_gui_subsystem


class Dependencies(unittest.TestCase):
    def test_editor_requires_gui_pe_subsystem(self):
        with tempfile.TemporaryDirectory() as temporary:
            executable = Path(temporary) / 'Editor.exe'
            for magic in (0x10b, 0x20b):
                data = bytearray(128 + 24 + 240)
                data[:2] = b'MZ'
                struct.pack_into('<I', data, 60, 128)
                data[128:132] = b'PE\0\0'
                struct.pack_into('<H', data, 148, 240)
                struct.pack_into('<H', data, 152, magic)
                struct.pack_into('<H', data, 220, 2)
                executable.write_bytes(data)
                require_gui_subsystem(executable)
                struct.pack_into('<H', data, 220, 3)
                executable.write_bytes(data)
                with self.assertRaisesRegex(RuntimeError, 'Windows GUI subsystem'):
                    require_gui_subsystem(executable)
            executable.write_bytes(b'not a PE')
            with self.assertRaisesRegex(RuntimeError, 'DOS header'):
                require_gui_subsystem(executable)

    def test_windows_environment_case_and_isolated_path(self):
        for root_key in ('SYSTEMROOT', 'SystemRoot'):
            with self.subTest(root_key=root_key):
                environment = relocated_environment({root_key: r'C:\Windows', 'Path': r'D:\build;C:\OpenSSL'})
                self.assertEqual(environment['PATH'], r'C:\Windows\System32;C:\Windows')
                self.assertNotIn('Path', environment)

    def test_closure_cycles_and_unrelated_tools(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            redist = root / 'redist'
            redist.mkdir()
            for name in ('NexoraEditor.exe', 'NexoraCore.dll', 'Unrelated.exe'):
                (root / name).touch()
            (redist / 'vcruntime140.dll').touch()
            graph = {'NexoraEditor.exe': ['NexoraCore.dll', 'KERNEL32.dll'],
                     'NexoraCore.dll': ['NexoraCore.dll', 'vcruntime140.dll'],
                     'vcruntime140.dll': ['api-ms-win-crt-runtime-l1-1-0.dll']}
            files = dependency_closure(root / 'NexoraEditor.exe', redist, lambda p: graph[p.name])
            self.assertEqual({p.name for p in files}, {'NexoraEditor.exe', 'NexoraCore.dll', 'vcruntime140.dll'})

    def test_explicit_openssl_root_and_its_crt_closure(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            editor = root / 'NexoraEditor.exe'
            editor.touch()
            openssl = root / 'openssl'
            (openssl / 'bin').mkdir(parents=True)
            (openssl / 'bin/libcrypto-3-x64.dll').touch()
            (root / 'vcruntime140.dll').touch()
            graph = {'NexoraEditor.exe': ['libcrypto-3-x64.dll'],
                     'libcrypto-3-x64.dll': ['vcruntime140.dll', 'bcrypt.dll'],
                     'vcruntime140.dll': []}
            files = dependency_closure(editor, root, lambda p: graph[p.name], openssl)
            self.assertIn(openssl / 'bin/libcrypto-3-x64.dll', files)
            with self.assertRaises(RuntimeError):
                dependency_closure(editor, root, lambda p: graph[p.name])

    def test_private_game_host_and_tools_relocate_without_development_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source, package, redist = root / 'source', root / 'package', root / 'redist'
            for directory in (source, package, redist, source / 'cmake/bin',
                              source / 'cmake/share/cmake-test/Modules', source / 'cmake/doc/test',
                              source / 'zig/lib/std'):
                directory.mkdir(parents=True, exist_ok=True)
            for relative in ('Player.exe', 'NexoraCore.dll', 'cmake/bin/cmake.exe',
                             'cmake/share/cmake-test/Modules/resource.cmake',
                             'cmake/doc/test/Copyright.txt', 'ninja.exe', 'ninja-license.txt',
                             'zig/zig.exe', 'zig/LICENSE', 'zig/lib/std/resource.zig'):
                (source / relative).write_text('fixture')
            (source / 'EngineSource.cpp').write_text('must not be packaged')
            (source / 'GameRuntime.profile').write_text('NEXORA_GAME_RUNTIME_1\nplayer=Player.exe\nruntime=' +
                str(source / 'NexoraCore.dll') + '\n')
            result = stage_game_build_runtime(package, source / 'GameRuntime.profile', redist,
                source / 'cmake', source / 'ninja.exe', source / 'zig',
                source / 'ninja-license.txt', lambda _: [])
            profile = (package / 'Runtime/GameRuntime.profile').read_text()
            self.assertEqual(profile, 'NEXORA_GAME_RUNTIME_1\nplayer=Player.exe\nruntime=NexoraCore.dll\n')
            self.assertEqual(result['runtime_files'], 2)
            self.assertTrue((package / 'Runtime/Player.exe').is_file())
            self.assertTrue((package / 'Tools/zig/lib/std/resource.zig').is_file())
            self.assertTrue((package / 'licenses/game-build-tools/Zig-MIT.txt').is_file())
            self.assertFalse((package / 'EngineSource.cpp').exists())
            self.assertFalse((package / 'Player.exe').exists())

    def test_unclassified_or_missing_dependency_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            editor = root / 'NexoraEditor.exe'
            editor.touch()
            for dependency in ('foreign.dll', 'NexoraMissing.dll', 'vcruntime140.dll'):
                with self.subTest(dependency=dependency), self.assertRaises(RuntimeError):
                    dependency_closure(editor, root, lambda p: [dependency])


if __name__ == '__main__':
    unittest.main()
