#!/usr/bin/env python3
"""Reject accidental effects ABI removal/reordering or undocumented callbacks."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
VALIDATOR = ROOT / 'Tools/Build/ValidateApiM6.py'


class EffectsContract(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix='nexora-effects-abi-')
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        shutil.copytree(ROOT / 'Engine/API', self.root / 'Engine/API')
        shutil.copytree(ROOT / 'Bindings/Zig', self.root / 'Bindings/Zig')

    def validate(self):
        return subprocess.run([sys.executable, str(VALIDATOR), str(self.root)],
                              capture_output=True, text=True)

    def test_current_contract(self):
        result = self.validate()
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_baseline_required(self):
        (self.root / 'Engine/API/abi_baseline_twod_effects_v1.json').unlink()
        self.assertNotEqual(self.validate().returncode, 0)

    def test_c_and_zig_reordering_rejected(self):
        for name in ['Engine/API/include/nexora/gameplay_twod_effects.h',
                     'Bindings/Zig/twod_effects.zig']:
            with self.subTest(name=name):
                path = self.root / name
                original = path.read_text(encoding='utf-8')
                swapped = original.replace('receive_light', 'temporary_field')
                swapped = swapped.replace('receive_shadow', 'receive_light')
                swapped = swapped.replace('temporary_field', 'receive_shadow')
                path.write_text(swapped, encoding='utf-8')
                result = self.validate()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('order', result.stderr)
                path.write_text(original, encoding='utf-8')

    def test_lighting_tail_baseline_required(self):
        (self.root / 'Engine/API/abi_baseline_twod_lighting_v1.json').unlink()
        self.assertNotEqual(self.validate().returncode, 0)

    def test_pool_identity_cannot_move_into_original_prefix(self):
        for name in ['Engine/API/include/nexora/gameplay_twod_effects.h',
                     'Bindings/Zig/twod_effects.zig']:
            path = self.root / name
            original = path.read_text(encoding='utf-8')
            changed = original.replace('pool_identity', 'temporary_tail')
            changed = changed.replace('submit_frame', 'pool_identity')
            changed = changed.replace('temporary_tail', 'submit_frame')
            path.write_text(changed, encoding='utf-8')
            result = self.validate()
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('order', result.stderr)
            path.write_text(original, encoding='utf-8')

    def test_each_callback_requires_metadata(self):
        path = self.root / 'Engine/API/abi_manifest.json'
        original = json.loads(path.read_text(encoding='utf-8'))
        names = ['NexoraGameplayServicesApiV1.get_twod_effects_api'] + [
            'NexoraGameplayTwoDEffectsApiV1.' + name
            for name in ['get_capabilities', 'get_state', 'submit_frame', 'pool_identity']]
        for name in names:
            with self.subTest(name=name):
                changed = dict(original, exports=[e for e in original['exports'] if e['name'] != name])
                path.write_text(json.dumps(changed), encoding='utf-8')
                result = self.validate()
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('lacks API-M6 metadata', result.stderr)


if __name__ == '__main__':
    unittest.main()
