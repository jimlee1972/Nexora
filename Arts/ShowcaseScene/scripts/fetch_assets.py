"""Restore pinned CC0 source assets required by rebuild_ruins.py."""
import argparse
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-only', action='store_true')
    args = parser.parse_args()
    manifest = json.loads((ROOT / 'asset-manifest.json').read_text(encoding='utf-8'))
    count = 0
    for asset in manifest['assets']:
        for entry in asset['files']:
            destination = (ROOT / entry['path']).resolve()
            if not destination.is_relative_to(ROOT):
                raise ValueError('Asset path must remain inside the scene directory')
            if destination.is_file() and sha256(destination) == entry['sha256']:
                count += 1
                continue
            if args.verify_only:
                raise RuntimeError('Missing or changed asset: ' + entry['path'])
            request = urllib.request.Request(entry['url'], headers={'User-Agent': 'Mozilla/5.0'})
            destination.parent.mkdir(parents=True, exist_ok=True)
            temporary = destination.with_name(destination.name + '.download')
            with urllib.request.urlopen(request, timeout=120) as response, temporary.open('wb') as stream:
                while chunk := response.read(1024 * 1024):
                    stream.write(chunk)
            if temporary.stat().st_size != entry['bytes'] or sha256(temporary) != entry['sha256']:
                raise RuntimeError('Asset checksum mismatch: ' + entry['path'])
            temporary.replace(destination)
            count += 1
    print(f'Verified {count} source files from {len(manifest["assets"])} CC0 assets.')


if __name__ == '__main__':
    main()
