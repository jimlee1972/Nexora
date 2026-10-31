#!/usr/bin/env python3
"""Publish content-addressed linear mask/links plus an atomic companion catalog.

Masks inherit every source rectangle/grid/cell selection from the base manifest. Their
canvas dimensions must match; packing width may differ. RGB is never sRGB-decoded.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile

import PrepareAssets as assets


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate manifest field')
        result[key] = value
    return result


def load(path):
    return json.loads(assets.bounded(path), object_pairs_hook=unique_object)


def prepare(manifest_path, compiler, output):
    root = manifest_path.resolve().parent
    config = load(manifest_path)
    if set(config) - {'schema', 'id', 'base_manifest', 'mask_id', 'mask_width', 'images'} or type(config.get('schema')) is not int or config['schema'] != 1:
        raise ValueError('unsupported emission manifest')
    base_path = assets.source_path(root, config['base_manifest'])
    base = load(base_path)
    if base.get('type') != 'atlas':
        raise ValueError('base manifest must be an atlas')
    assets.uuid(config['id'])
    assets.uuid(config['mask_id'])
    mask = copy.deepcopy(base)
    mask.update(id=config['mask_id'], nearest=False, color_space='linear')
    mask['width'] = assets.integer(config.get('mask_width', base.get('width', 1024)), 3, 4096)
    entries = config['images']
    if not isinstance(entries, list) or not 1 <= len(entries) <= 1024:
        raise ValueError('mask image budget')
    paths = {}
    for entry in entries:
        if set(entry) != {'source', 'mask'} or entry['source'] in paths:
            raise ValueError('mask source mapping must be unique')
        paths[entry['source']] = entry['mask']
    if set(paths) != {entry['path'] for entry in base['images']}:
        raise ValueError('every source image requires exactly one mask')
    for source in mask['images']:
        source_path = assets.source_path(base_path.parent, source['path'])
        mask_path = assets.source_path(root, paths[source['path']])
        a = assets.png(assets.bounded(source_path))
        b = assets.png(assets.bounded(mask_path))
        if a[:2] != b[:2]:
            raise ValueError('mask canvas must match the base source before slicing')
        source['path'] = mask_path.relative_to(root).as_posix()
    parts = assets.images(mask, root)
    ids = sorted(struct.unpack_from('<I', part)[0] for part in parts)
    if len(ids) != len(set(ids)):
        raise ValueError('duplicate region identity')
    wire = (struct.pack('<II', 0x4532444e, 1) + assets.uuid(config['id']) + assets.uuid(base['id'])
            + assets.uuid(config['mask_id']) + struct.pack('<I', len(ids))
            + b''.join(struct.pack('<II', identity, identity) for identity in ids))
    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.nx2-emission-', dir=output.parent) as directory:
        stage = Path(directory)
        assets.compile_manifest(base, base_path.parent, compiler, stage/'base.nx2atlas')
        assets.compile_manifest(mask, root, compiler, stage/'mask.nx2atlas')
        (stage/'links.source').write_bytes(wire)
        subprocess.run([str(compiler.resolve()), 'emission', 'links.source', 'links.nx2emission',
                        'base.nx2atlas', 'mask.nx2atlas'], cwd=stage, check=True)
        names = {}
        for key, suffix in [('mask', 'nx2atlas'), ('links', 'nx2emission')]:
            source = stage/f'{key}.{suffix}'
            data = source.read_bytes()
            digest = hashlib.sha256(data).hexdigest()
            name = f'{key}-{digest}.{suffix}'
            destination = output.parent/name
            if destination.exists():
                if assets.bounded(destination) != data:
                    raise ValueError('content-addressed asset conflicts with existing bytes')
            else:
                # Publish immutable generations first. A failure may leave an unreferenced
                # generation, but the previous catalog and everything it references survive.
                os.replace(source, destination)
            names[key] = name
        catalog = {'schema': 1, 'id': config['id'], 'base_atlas': base['id'],
                   'mask_atlas': config['mask_id'], **names,
                   'mask_channel': 'linear_red', 'filter': 'linear', 'alpha': 'sprite_coverage'}
        staged_catalog = stage/'catalog.json'
        staged_catalog.write_text(json.dumps(catalog, indent=2, sort_keys=True)+'\n', encoding='utf-8')
        os.replace(staged_catalog, output)
    return catalog


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('manifest', type=Path)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    prepare(args.manifest, args.compiler, args.output)


if __name__ == '__main__':
    main()
