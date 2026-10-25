#!/usr/bin/env python3
"""Mutation guards for the published Scene ABI and callback metadata."""
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
VALIDATOR = ROOT / 'Tools/Build/ValidateApiM6.py'


class SceneContract(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='nexora-api-scene-')
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        shutil.copytree(ROOT / 'Engine/API', self.root / 'Engine/API')
        shutil.copytree(ROOT / 'Bindings/Zig', self.root / 'Bindings/Zig')

    def validate(self):
        return subprocess.run([sys.executable, str(VALIDATOR), str(self.root)],
                              capture_output=True, text=True)

    def test_unchanged_contract(self):
        result = self.validate()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('documented exports', result.stdout)

    def test_requires_new_extension_baseline(self):
        (self.root / 'Engine/API/abi_baseline_scene_v1.json').unlink()
        self.assertNotEqual(self.validate().returncode, 0)

    def test_rejects_new_c_and_zig_field_reordering(self):
        for name in ['Engine/API/include/nexora/gameplay_scene.h',
                     'Bindings/Zig/scene.zig']:
            with self.subTest(name=name):
                path = self.root / name
                original = path.read_text(encoding='utf-8')
                path.write_text(re.sub(r'\b(request|scene)\b',
                                       lambda match: 'scene' if match[0] == 'request' else 'request',
                                       original), encoding='utf-8')
                result = self.validate()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('order', result.stderr)
                path.write_text(original, encoding='utf-8')

    def test_rejects_missing_services_suffix(self):
        path = self.root / 'Engine/API/include/nexora/gameplay_services.h'
        path.write_text(path.read_text(encoding='utf-8').replace('get_scene_api', 'future_api_slot'), encoding='utf-8')
        self.assertNotEqual(self.validate().returncode, 0)

    def test_requires_each_scene_callback_metadata(self):
        path = self.root / 'Engine/API/abi_manifest.json'
        original = json.loads(path.read_text(encoding='utf-8'))
        names = ['NexoraGameplayServicesApiV1.get_scene_api'] + [
            'NexoraGameplaySceneApiV1.' + name
            for name in ['resolve_scene', 'request_load', 'request_unload', 'get_status', 'get_active_scene', 'get_scene_info']]
        for name in names:
            with self.subTest(callback=name):
                altered = dict(original, exports=[e for e in original['exports'] if e['name'] != name])
                path.write_text(json.dumps(altered), encoding='utf-8')
                result = self.validate()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('lacks API-M6 metadata', result.stderr)
        altered = json.loads(json.dumps(original))
        del altered['exports'][-1]['threading']
        path.write_text(json.dumps(altered), encoding='utf-8')
        self.assertNotEqual(self.validate().returncode, 0)


if __name__ == '__main__':
    unittest.main()
