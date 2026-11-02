"""Versioned SDK admission must reject self-consistent but incomplete HD2D payloads."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('sdk_manifest', Path(__file__).resolve().parents[1]/'SDK.py')
SDK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SDK)


class ManifestTests(unittest.TestCase):
    def test_old_sdk_still_verifies_and_hd2d_requires_companion_assets_and_licenses(self):
        old = ('bin/NexoraGameHost.exe', 'bin/NexoraTwoDAssetCompiler.exe', 'Tools/Ninja/ninja.exe',
               'Tools/Zig/build.zig', 'Tools/Zig/game_entry.zig', 'Bindings/Zig/nexora.zig',
               'CMake/Modules/NexoraGameProject.cmake')
        new = ('Bindings/Zig/hd2d_diagnostic.zig', 'Bindings/Zig/twod_effects.zig',
               'Bindings/Zig/diagnostic/base.nx2atlas', 'Bindings/Zig/diagnostic/mask.nx2atlas',
               'Bindings/Zig/diagnostic/links.nx2emission', 'Bindings/Zig/diagnostic/ui_fixture.nxfont',
               'Tools/TwoD/PrepareEmission.py', 'Tools/TwoD/StageHD2D.py',
               'licenses/Noto-CJK-OFL.txt', 'licenses/Noto-CJK-NOTICE.md')
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = dict(schema=1, target='x86_64-windows-msvc', backend='dx12',
                configuration='Shipping', link_mode='Monolithic', abi=3, zig='0.14.0', ninja='1.13.2',
                version='0.1.0', source_commit='a'*40)
            def refresh():
                manifest['files'] = SDK.files(root)
                (root/'sdk.json').write_text(json.dumps(manifest), encoding='utf-8')
            for name in old:
                p = root/name; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(b'explicit test payload')
            refresh(); self.assertEqual(SDK.verify(root)['version'], '0.1.0')
            manifest['version'] = '0.2.0'; refresh()
            with self.assertRaisesRegex(ValueError, 'incomplete SDK'): SDK.verify(root)
            for name in new:
                p = root/name; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(b'explicit HD2D test payload')
            refresh(); self.assertEqual(SDK.verify(root)['version'], '0.2.0')
            # Every omitted new contract dependency must fail even with a matching inventory.
            for name in new:
                p = root/name; data = p.read_bytes(); p.unlink(); refresh()
                with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'incomplete SDK'): SDK.verify(root)
                p.write_bytes(data)
            refresh()
            (root/new[0]).write_bytes(b'tampered after archive verification')
            with self.assertRaisesRegex(ValueError, 'inventory mismatch'): SDK.verify(root)


if __name__ == '__main__':
    unittest.main()
