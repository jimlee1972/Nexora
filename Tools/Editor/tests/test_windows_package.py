"""Synthetic dependency admission tests; these do not execute Windows binaries."""
import sys
from pathlib import Path
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PackageWindowsEditor import dependency_closure, relocated_environment


class Dependencies(unittest.TestCase):
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
