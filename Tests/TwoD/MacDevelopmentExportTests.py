"""Portable dyld-graph fixtures; actual Mach-O export and startup run in macOS CI."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('exporter', ROOT/'Tools/TwoD/ExportGame.py')
exporter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(exporter)


class MacExportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.bin = self.root/'bin'; self.bin.mkdir()
        self.libs = self.root/'libs'; self.libs.mkdir()
        self.app = self.bin/'Player'
        self.module = self.bin/'scene.dylib'
        self.runtime = self.libs/'libNexoraRuntime.dylib'
        self.core = self.libs/'libNexoraCore.dylib'
        self.files = [self.app, self.module, self.runtime, self.core]
        for path in self.files: path.write_bytes(('fixture '+path.name).encode())
        self.imports = {self.app: ['@rpath/'+self.runtime.name],
                        self.runtime: ['@loader_path/'+self.core.name], self.core: [], self.module: []}
        self.commands = []
        self.descriptor = self.bin/'game.json'
        self.descriptor.write_text(json.dumps(dict(schema=1, id='com.nexora.test', title='DEV ONLY',
            scene='Main', module=self.module.name, audio='off')), encoding='utf-8')
        self.addCleanup(mock.patch.stopall)
        mock.patch.object(exporter.sys, 'platform', 'darwin').start()
        mock.patch.object(exporter.subprocess, 'check_output', side_effect=self.otool).start()
        mock.patch.object(exporter.subprocess, 'run', side_effect=self.mutate).start()

    def otool(self, command, **_):
        self.assertEqual(command[0], 'otool')
        source = Path(command[-1])
        if command[1] == '-l':
            return 'cmd LC_RPATH\ncmdsize 40\npath @loader_path/../libs (offset 12)\n' if source == self.app else ''
        identity = '@rpath/'+source.name if source.suffix == '.dylib' else None
        if command[1] == '-D': return str(source)+':\n'+(identity+'\n' if identity else '')
        self.assertEqual(command[1], '-L')
        names = ([identity] if identity else []) + self.imports[source] + ['/usr/lib/libSystem.B.dylib']
        return str(source)+':\n'+''.join('\t'+name+' (compatibility version 1.0.0, current version 1.0.0)\n' for name in names)

    def mutate(self, command, **_):
        self.commands.append(command)
        self.assertIn(command[0], ('install_name_tool', 'codesign'))
        target = Path(command[-1])
        self.assertNotIn(target, self.files)
        target.write_bytes(target.read_bytes()+b'\n'+json.dumps(command[:-1]).encode())
        return subprocess.CompletedProcess(command, 0)

    def export(self, development=True):
        return exporter.export(self.descriptor, self.app, self.root/'package', allow_development=development)

    def test_transitive_graph_relocation_resign_inventory_and_unchanged_sources(self):
        original = {p: p.read_bytes() for p in self.files}
        output = self.export()
        inventory = json.loads((output/'inventory.json').read_text(encoding='utf-8'))
        self.assertTrue(inventory['development_test_copy'])
        for name in ('Player', 'gameplay.dylib', self.runtime.name, self.core.name):
            self.assertEqual(inventory['files'][name], exporter.checksum(output/name))
        self.assertTrue(any('@loader_path/'+self.runtime.name in command for command in self.commands))
        self.assertTrue(any('@loader_path/'+self.core.name in command for command in self.commands))
        self.assertEqual(sum(c[0] == 'codesign' for c in self.commands), 4)
        self.assertEqual(original, {p: p.read_bytes() for p in self.files})
        relocated = self.root/'relocated'; output.rename(relocated)
        for name, digest in inventory['files'].items():
            self.assertEqual(exporter.checksum(relocated/name), digest)

    def test_parent_rpath_resolves_transitive_dependency(self):
        self.imports[self.runtime] = ['@rpath/'+self.core.name]
        self.assertTrue((self.export()/self.core.name).is_file())

    def test_missing_dependency_rejects_before_publication(self):
        self.core.unlink()
        with self.assertRaisesRegex(ValueError, 'unresolved macOS dependency'): self.export()
        self.assertFalse((self.root/'package').exists())

    def test_conflicting_basename_and_non_system_framework_reject(self):
        other = self.root/'other'; other.mkdir()
        conflicting = other/self.core.name; conflicting.write_bytes(b'different core fixture')
        self.imports[self.module] = [str(conflicting)]
        with self.assertRaisesRegex(ValueError, 'conflicting macOS dependency filename'): self.export()
        self.assertFalse((self.root/'package').exists())
        self.imports[self.module] = ['/private/Test.framework/Versions/A/Test']
        with self.assertRaisesRegex(ValueError, 'non-system macOS framework'): self.export()
        self.assertFalse((self.root/'package').exists())

    def test_editor_and_foreign_modules_reject(self):
        self.imports[self.app] = ['@rpath/libNexoraEditor.dylib']
        (self.libs/'libNexoraEditor.dylib').write_bytes(b'editor fixture')
        with self.assertRaisesRegex(ValueError, 'unsupported macOS'): self.export()
        self.assertFalse((self.root/'package').exists())
        config = json.loads(self.descriptor.read_text(encoding='utf-8'))
        (self.bin/'foreign.so').write_bytes(b'foreign fixture'); config['module'] = 'foreign.so'
        self.descriptor.write_text(json.dumps(config), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'module must match'): self.export()

    def test_shipping_is_not_claimed_and_signing_failure_preserves_fresh_output(self):
        with self.assertRaisesRegex(ValueError, 'Shipping distribution is not provided'): self.export(False)
        self.assertFalse((self.root/'package').exists())
        with mock.patch.object(exporter.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'codesign')):
            with self.assertRaises(subprocess.CalledProcessError): self.export()
        self.assertFalse((self.root/'package').exists())


if __name__ == '__main__':
    unittest.main()
