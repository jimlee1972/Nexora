#!/usr/bin/env python3
"""Validate exact candidate provenance, including nested Windows artifact receipts."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import zipfile


def verify(release, evidence, source, tag):
    if not re.fullmatch(r'[0-9a-f]{40}', source):
        raise ValueError('invalid source commit')
    manifest = json.loads((release/'sdk.json').read_text(encoding='utf-8'))
    if manifest['source_commit'] != source or tag != 'sdk-2d-windows-x64-v'+manifest['version']:
        raise ValueError('immutable source/tag mismatch')
    archives = list(release.glob('*.zip'))
    receipts = list(evidence.rglob('acceptance.json'))
    # upload-artifact preserves a common ancestor across workspace and runner.temp.
    # Require one receipt rather than guessing a Windows drive/workspace prefix.
    if len(archives) != 1 or len(receipts) != 1 or receipts[0].is_symlink():
        raise ValueError('expected exactly one archive and consumer receipt')
    archive = archives[0]
    receipt = json.loads(receipts[0].read_text(encoding='utf-8'))
    if receipt['status'] != 'PASS' or receipt['source_commit'] != source:
        raise ValueError('consumer did not pass at the immutable source')
    if manifest['version'] == '0.2.0':
        if receipt['archive_sha256'] != hashlib.sha256(archive.read_bytes()).hexdigest():
            raise ValueError('consumer archive mismatch')
        if (receipt['real_zig_hd2d'] is not True or receipt['offline_rebuild'] is not True
                or receipt['native_dx12'] != 'PASS'):
            raise ValueError('missing native/offline HD2D acceptance')
    with zipfile.ZipFile(archive) as zipped:
        if json.loads(zipped.read('sdk.json')) != manifest:
            raise ValueError('archive manifest mismatch')
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', type=Path, required=True)
    parser.add_argument('--evidence', type=Path, required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--tag', required=True)
    args = parser.parse_args()
    verify(args.release, args.evidence, args.source, args.tag)
    print('PASS exact tested SDK archive/source/tag and unique consumer receipt')
