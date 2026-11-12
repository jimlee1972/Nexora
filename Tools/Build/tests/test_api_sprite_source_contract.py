"""Reject removal/reordering or undocumented callbacks in the additive Sprite source ABI."""
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
VALIDATOR = ROOT/'Tools/Build/ValidateApiM6.py'


class SpriteSourceContract(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix='nexora-api-source-')
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        shutil.copytree(ROOT/'Engine/API', self.root/'Engine/API')
        shutil.copytree(ROOT/'Bindings/Zig', self.root/'Bindings/Zig')

    def validate(self):
        return subprocess.run([sys.executable, str(VALIDATOR), str(self.root)],
                              capture_output=True, text=True)

    def test_published_contract_and_required_baseline(self):
        self.assertEqual(self.validate().returncode, 0)
        (self.root/'Engine/API/abi_baseline_sprite_source_v1.json').unlink()
        self.assertNotEqual(self.validate().returncode, 0)

    def test_rejects_source_identity_reorder_in_c_and_zig(self):
        for name in ['Engine/API/include/nexora/nexora.h', 'Bindings/Zig/nexora.zig']:
            with self.subTest(path=name):
                path = self.root/name
                original = path.read_text()
                path.write_text(re.sub(r'\b(source_high|source_low)\b',
                    lambda match: 'source_low' if match[0] == 'source_high' else 'source_high', original))
                self.assertNotEqual(self.validate().returncode, 0)
                path.write_text(original)

    def test_requires_each_optional_getter_and_callback_metadata(self):
        path = self.root/'Engine/API/abi_manifest.json'
        original = json.loads(path.read_text())
        names = ['NexoraTwoDApiV1.get_source_api',
                 'NexoraGameplayServicesApiV1.get_sprite_content_api',
                 'NexoraGameplaySpriteContentApiV1.register_sprite_source']
        names += ['NexoraTwoDSourceApiV1.'+name for name in
                  ['set_sprite', 'get_sprite', 'create_instances', 'read_instances', 'write_instances']]
        for name in names:
            with self.subTest(callback=name):
                altered = dict(original, exports=[e for e in original['exports'] if e['name'] != name])
                path.write_text(json.dumps(altered))
                result = self.validate()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('lacks API-M6 metadata', result.stderr)


if __name__ == '__main__':
    unittest.main()
