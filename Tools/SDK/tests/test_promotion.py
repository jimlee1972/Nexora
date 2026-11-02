"""Immutable promotion must handle actual artifact roots without accepting ambiguity."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

SPEC = importlib.util.spec_from_file_location('promotion', Path(__file__).resolve().parents[1]/'VerifyPromotion.py')
PROMOTION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PROMOTION)


class PromotionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.release, self.evidence = root/'release', root/'evidence'
        self.release.mkdir(); self.evidence.mkdir()
        self.source, self.tag = 'a'*40, 'sdk-2d-windows-x64-v0.2.0'
        self.manifest = dict(source_commit=self.source, version='0.2.0', files={})
        (self.release/'sdk.json').write_text(json.dumps(self.manifest), encoding='utf-8')
        self.archive = self.release/'Nexora-2D-SDK-Windows-x64-0.2.0.zip'
        with zipfile.ZipFile(self.archive, 'w') as zipped:
            zipped.writestr('sdk.json', json.dumps(self.manifest))
        self.receipt = dict(status='PASS', source_commit=self.source,
            archive_sha256=hashlib.sha256(self.archive.read_bytes()).hexdigest(),
            real_zig_hd2d=True, offline_rebuild=True, native_dx12='PASS')
        self.path = self.evidence/'a/Nexora/Nexora/evidence/acceptance.json'
        self.path.parent.mkdir(parents=True)
        self.save()

    def save(self):
        self.path.write_text(json.dumps(self.receipt), encoding='utf-8')

    def verify(self):
        return PROMOTION.verify(self.release, self.evidence, self.source, self.tag)

    def test_nested_workspace_and_flat_receipt(self):
        self.assertEqual(self.verify(), self.receipt)
        self.path.rename(self.evidence/'acceptance.json')
        self.assertEqual(self.verify(), self.receipt)

    def test_missing_ambiguous_and_linked_receipts(self):
        extra = self.evidence/'acceptance.json'
        extra.write_text(json.dumps(self.receipt), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'exactly one'): self.verify()
        extra.unlink(); self.path.unlink()
        with self.assertRaisesRegex(ValueError, 'exactly one'): self.verify()
        if hasattr(__import__('os'), 'symlink'):
            target = self.evidence/'receipt.json'
            target.write_text(json.dumps(self.receipt), encoding='utf-8')
            try: self.path.symlink_to(target)
            except OSError: return  # Windows may not grant symlink privileges.
            with self.assertRaisesRegex(ValueError, 'exactly one'): self.verify()

    def test_reject_changed_source_tag_hash_and_acceptance(self):
        for key, value in [('status', 'FAIL'), ('source_commit', 'b'*40),
                ('archive_sha256', '0'*64), ('real_zig_hd2d', False),
                ('offline_rebuild', False), ('native_dx12', 'NOT_RUN')]:
            original = self.receipt[key]
            self.receipt[key] = value; self.save()
            with self.subTest(key=key), self.assertRaises(ValueError): self.verify()
            self.receipt[key] = original; self.save()
        with self.assertRaisesRegex(ValueError, 'source/tag'):
            PROMOTION.verify(self.release, self.evidence, self.source, self.tag+'-retry')
        self.manifest['version'] = '0.1.0'
        with zipfile.ZipFile(self.archive, 'w') as zipped:
            zipped.writestr('sdk.json', json.dumps(self.manifest))
        self.receipt['archive_sha256'] = hashlib.sha256(self.archive.read_bytes()).hexdigest(); self.save()
        with self.assertRaisesRegex(ValueError, 'archive manifest'): self.verify()


if __name__ == '__main__':
    unittest.main()
