"""Portable PE import parsing and export admission, including delay-loaded DLLs."""
import importlib.util
import os
import subprocess
import sys
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('export', Path(__file__).resolve().parents[2] / 'TwoD/ExportGame.py')
EXPORT = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EXPORT)


def fixture(normal='KERNEL32.dll', delay='USER32.dll'):
    data = bytearray(2048)
    data[:2] = b'MZ'
    struct.pack_into('<I', data, 60, 128)
    data[128:132] = b'PE\0\0'
    struct.pack_into('<HHIIIHH', data, 132, 0x8664, 1, 0, 0, 0, 240, 0)
    opt = 152
    struct.pack_into('<H', data, opt, 0x20b)
    struct.pack_into('<Q', data, opt+24, 0x140000000)
    struct.pack_into('<I', data, opt+60, 512)
    struct.pack_into('<I', data, opt+108, 16)
    struct.pack_into('<II', data, opt+112+8, 0x1000, 40)
    struct.pack_into('<II', data, opt+112+13*8, 0x1100, 64)
    struct.pack_into('<IIII', data, opt+240+8, 1536, 0x1000, 1536, 512)
    struct.pack_into('<IIIII', data, 512, 0, 0, 0, 0x1200, 0)
    struct.pack_into('<8I', data, 768, 1, 0x1280, 0, 0, 0, 0, 0, 0)
    data[1024:1024+len(normal)+1] = normal.encode()+b'\0'
    data[1152:1152+len(delay)+1] = delay.encode()+b'\0'
    return data


class PeTests(unittest.TestCase):
    def test_normal_delay_and_unicode_path(self):
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp) / '陰陽 game.exe'
            p.write_bytes(fixture())
            self.assertEqual(EXPORT.pe_imports(p), ['KERNEL32.dll', 'USER32.dll'])

    def test_malformed_headers_rva_names_and_terminators(self):
        cases = [b'', fixture()[:300]]
        for offset, value in [(60, 999999), (152+112+8, 0xfffffff0), (512+12, 0x1ffff), (512+20, 1)]:
            b = fixture()
            struct.pack_into('<I', b, offset, value)
            cases.append(b)
        cases.extend([fixture('../evil.dll'), fixture('C:evil.dll')])
        unterminated = fixture()
        unterminated[1024:1285] = b'a'*261
        cases.append(unterminated)
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp)/'bad.exe'
            for b in cases:
                with self.subTest(size=len(b)):
                    p.write_bytes(b)
                    with self.assertRaises(ValueError):
                        EXPORT.pe_imports(p)

    def test_cli_diagnostics_are_utf8_even_with_legacy_stdio(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            missing = root/'陰陽 missing.json'
            result = subprocess.run([sys.executable, str(Path(EXPORT.__file__)), str(missing),
                                     '--runtime', str(root/'Player.exe'), '--output', str(root/'game')],
                                    env=dict(os.environ, PYTHONIOENCODING='cp1252'), capture_output=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('陰陽', result.stderr.decode('utf-8'))
            self.assertNotIn(b'UnicodeEncodeError', result.stderr)

    def test_delay_dependency_policy(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            p = root/'game.exe'
            with mock.patch.object(EXPORT.sys, 'platform', 'win32'):
                for name in ('NexoraEditor.dll', 'NexoraRuntime.dll', 'unknown.dll', 'VCRUNTIME140.dll'):
                    p.write_bytes(fixture(delay=name))
                    with self.subTest(name=name), self.assertRaises(ValueError):
                        EXPORT.dependencies(p, root, False, None)


if __name__ == '__main__':
    unittest.main()
