"""Synthetic dependency admission tests; these do not execute Windows binaries."""
import sys
from pathlib import Path
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PackageWindowsEditor import dependency_closure


class Dependencies(unittest.TestCase):
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
